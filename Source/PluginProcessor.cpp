#include "PluginProcessor.h"
#include "PluginEditor.h"

// ── Parameter layout ──────────────────────────────────────────────────────────

juce::AudioProcessorValueTreeState::ParameterLayout
DDD1HubProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back (std::make_unique<juce::AudioParameterInt>(
        "midiChannel", "MIDI Channel", 1, 16, 1));
    p.push_back (std::make_unique<juce::AudioParameterFloat>(
        "bpm", "BPM",
        juce::NormalisableRange<float> (40.f, 200.f, 0.1f), 120.f));
    return { p.begin(), p.end() };
}

// ── Constructor / destructor ──────────────────────────────────────────────────

DDD1HubProcessor::DDD1HubProcessor()
    : AudioProcessor (BusesProperties()
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    for (auto& s : padKb) s.activeSeqTunes.fill (-1);
    std::fill (std::begin (delayEchoLastFired), std::end (delayEchoLastFired), juce::int64 (-1000000));
    for (int i = 0; i < numPads; ++i)
    {
        heldBitsLo[i].store (0);
        heldBitsHi[i].store (0);
        padHitSeq[i].store (0);
        overdubDirty[i].store (false);
    }

    // Default note receives: typical DDD-1 drum INST KEY assignments
    const int defaultNotes[14] = { 36, 38, 37, 44, 46, 56, 58, 48, 45, 43, 51, 49, 39, 54 };
    for (int i = 0; i < numPads; ++i)
    {
        pads[i].noteReceive = defaultNotes[i];
        pads[i].instKey     = defaultNotes[i];
    }

    // Search for drum-pattern-database in likely locations; fall back to app data single file
    {
        auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
        juce::File candidates[] = {
            home.getChildFile ("PycharmProjects/drum-pattern-database"),
            home.getChildFile ("Documents/drum-pattern-database"),
            home.getChildFile ("Desktop/drum-pattern-database"),
            juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                .getChildFile ("DDD1MidiHub/patterns.json"),
        };
        patternBankFilePath = candidates[3].getFullPathName(); // fallback
        for (auto& f : candidates)
            if (f.exists()) { patternBankFilePath = f.getFullPathName(); break; }
    }
    loadPatternBank (juce::File (patternBankFilePath));

    patternSetBankFilePath = juce::File::getSpecialLocation (
        juce::File::userApplicationDataDirectory)
        .getChildFile ("DDD1MidiHub/patternsets.json").getFullPathName();
    patternSetBank.load (juce::File (patternSetBankFilePath));

    loadRatings();
    loadIdeas();

#if JUCE_DEBUG
    // Roundtrip test: capture → serialize → deserialize → verify
    {
        auto testIdea = captureCurrentIdea ("__debug_roundtrip__");
        IdeaBank tempBank;
        tempBank.add (testIdea);
        juce::TemporaryFile tmp (".json");
        tempBank.save (tmp.getFile());
        IdeaBank verifyBank;
        verifyBank.load (tmp.getFile());
        const auto* reloaded = verifyBank.findById (testIdea.id);
        bool pass = reloaded
                 && reloaded->name       == testIdea.name
                 && reloaded->padConfigs.size() == testIdea.padConfigs.size()
                 && reloaded->patterns.size()   == testIdea.patterns.size();
        DBG ("IdeaBank roundtrip: " << (pass ? "PASS" : "FAIL")
             << "  pads=" << (int)testIdea.padConfigs.size()
             << "  patterns=" << (int)testIdea.patterns.size());
    }
#endif
}

DDD1HubProcessor::~DDD1HubProcessor()
{
    // Send All Notes Off before closing, so the DDD-1 doesn't keep playing
    {
        juce::ScopedLock lk (midiOutLock);
        if (midiOut)
            for (int ch = 1; ch <= 16; ++ch)
                midiOut->sendMessageNow (juce::MidiMessage::allNotesOff (ch));
    }

    auto closeInput = [](std::unique_ptr<juce::MidiInput>& dev, juce::CriticalSection& lock)
    {
        std::unique_ptr<juce::MidiInput> toClose;
        { juce::ScopedLock lk (lock); toClose = std::move (dev); }
        if (toClose) toClose->stop();
    };
    closeInput (ddd1In, ddd1InLock);
    closeInput (kbIn,   kbInLock);
}

// ── Virtual piano injection ───────────────────────────────────────────────────

void DDD1HubProcessor::injectKbNote (const juce::MidiMessage& msg)
{
    juce::ScopedLock lk (kbInLock);
    kbInBuf.addEvent (msg, 0);
}

// ── Static helpers ────────────────────────────────────────────────────────────

double DDD1HubProcessor::lfoValue (double phase, int shape)
{
    switch (shape)
    {
        case 0: return std::sin (2.0 * juce::MathConstants<double>::pi * phase);
        case 1: return phase < 0.5 ? 1.0 : -1.0;
        case 2: return phase < 0.5 ? 4.0*phase - 1.0 : 3.0 - 4.0*phase;
        case 3: return 2.0*phase - 1.0;
        case 4: return 1.0 - 2.0*phase;
        default: return 0.0;
    }
}

int DDD1HubProcessor::lfoOffset (double phase, int depth, int shape, bool done)
{
    if (depth == 0) return 0;
    double p = done ? 1.0 : phase;
    double v = lfoValue (p, shape);
    if (!done && p > 0.8) v *= (1.0 - p) / 0.2;
    return (int)std::round (v * depth);
}

juce::uint8 DDD1HubProcessor::decayedVel (juce::uint8 origVel, int idx, float decay)
{
    if (decay < 0.1f || idx == 0) return origVel;
    float factor = std::exp (-decay * idx / 300.f);
    return (juce::uint8)juce::jlimit (1, 127, (int)(origVel * factor));
}

double DDD1HubProcessor::retriggHz (int rateIdx, double bpm)
{
    static const double npb[] = { 2.0, 4.0, 8.0, 16.0 };
    return (bpm / 60.0) * npb[juce::jlimit (0, 3, rateIdx)];
}

double DDD1HubProcessor::lfoHz (int rateIdx, double bpm)
{
    // Notes per beat: 1/4=1, 1/8=2, 1/16=4, 1/32=8, 1/64=16, 1/128=32
    static const double npb[] = { 1.0, 2.0, 4.0, 8.0, 16.0, 32.0 };
    return (bpm / 60.0) * npb[juce::jlimit (0, 5, rateIdx)];
}

int DDD1HubProcessor::pulsesPerRetrig (int rateIdx)
{
    static const int p[] = { 12, 6, 3, 1 };
    return p[juce::jlimit (0, 3, rateIdx)];
}

double DDD1HubProcessor::pulsesPerLFO (int rateIdx)
{
    // Pulses per LFO cycle at 24 PPQN: 1/4=24, 1/8=12, 1/16=6, 1/32=3, 1/64=1.5, 1/128=0.75
    static const double p[] = { 24.0, 12.0, 6.0, 3.0, 1.5, 0.75 };
    return p[juce::jlimit (0, 5, rateIdx)];
}

// ── MIDI I/O ──────────────────────────────────────────────────────────────────

void DDD1HubProcessor::openMidiOutput (const juce::String& deviceId)
{
    juce::ScopedLock lk (midiOutLock);
    midiOut.reset();
    midiOutputId = deviceId;
    if (deviceId.isEmpty()) return;
    for (auto& info : juce::MidiOutput::getAvailableDevices())
        if (info.identifier == deviceId)
            { midiOut = juce::MidiOutput::openDevice (deviceId); break; }
}

static std::unique_ptr<juce::MidiInput> openMidiIn (
    const juce::String& deviceId, juce::MidiInputCallback* cb,
    std::unique_ptr<juce::MidiInput>& existing, juce::CriticalSection& lock)
{
    std::unique_ptr<juce::MidiInput> toClose;
    { juce::ScopedLock lk (lock); toClose = std::move (existing); }
    if (toClose) toClose->stop();

    if (deviceId.isEmpty()) return nullptr;

    for (auto& info : juce::MidiInput::getAvailableDevices())
    {
        if (info.identifier == deviceId)
        {
            auto dev = juce::MidiInput::openDevice (deviceId, cb);
            if (dev) dev->start();
            return dev;
        }
    }
    return nullptr;
}

void DDD1HubProcessor::openDDD1Input (const juce::String& deviceId)
{
    ddd1InputId = deviceId;
    auto dev = openMidiIn (deviceId, this, ddd1In, ddd1InLock);
    juce::ScopedLock lk (ddd1InLock);
    ddd1In = std::move (dev);
}

void DDD1HubProcessor::openKbInput (const juce::String& deviceId)
{
    kbInputId = deviceId;
    auto dev = openMidiIn (deviceId, this, kbIn, kbInLock);
    juce::ScopedLock lk (kbInLock);
    kbIn = std::move (dev);
}

void DDD1HubProcessor::handleIncomingMidiMessage (juce::MidiInput* src,
                                                   const juce::MidiMessage& msg)
{
    // JUCE standalone may register us as callback for ALL MIDI devices.
    // Only accept messages from devices we explicitly opened.
    {
        juce::ScopedLock lk (ddd1InLock);
        if (src == ddd1In.get()) { ddd1InBuf.addEvent (msg, 0); return; }
    }
    {
        juce::ScopedLock lk (kbInLock);
        if (src == kbIn.get()) { kbInBuf.addEvent (msg, 0); return; }
    }
    // src is an unknown device — silently ignore
}

// ── Prepare / release ─────────────────────────────────────────────────────────

void DDD1HubProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    for (auto& s : padKb)
    {
        s.heldNotes.clear();
        s.retrigCounts.clear();
        s.activeSeqTunes.fill (-1);
        s.retriggPhase       = 0.0;
        s.lfoPhase           = 0.0;
        s.lfoOneShotDone     = false;
        s.pulsesToNextRetrig = 0;
        s.arpNotes.clear();
        s.arpStep            = 0;
        s.arpPingDir         = 1;
        s.pulsesToNextArp    = 0;
        s.arpLastNote        = -1;
        s.patternCurrentStep = 0;
        s.pulsesToNextStep   = 0;
        s.patternLastNote    = -1;
    }
    { juce::ScopedLock lk (delayLock); delayQueue.clear(); }
    std::fill (std::begin (delayEchoLastFired), std::end (delayEchoLastFired), juce::int64 (-1000000));
    samplesSinceLastClock = (int)(sampleRate * 3);
    lastClockSampleTime   = -1;
    totalSamples          = 0;
}

void DDD1HubProcessor::releaseResources()
{
    for (auto& s : padKb) { s.heldNotes.clear(); s.retrigCounts.clear(); }
}

// ── Keyboard mode helpers ─────────────────────────────────────────────────────

void DDD1HubProcessor::scheduleDelayEchoes (int note, juce::uint8 vel, int padIdx,
                                             juce::int64 startSample, int midiCh, double bpm)
{
    const auto& cfg = pads[padIdx];
    double intervalSec;
    if (clockSyncActive)
    {
        int ppb = pulsesPerRetrig (cfg.delayRate);
        intervalSec = (ppb / 24.0) * (60.0 / bpm);
    }
    else
    {
        intervalSec = 1.0 / retriggHz (cfg.delayRate, bpm);
    }
    juce::int64 intervalSmp = juce::jmax ((juce::int64)1,
                                          (juce::int64)(intervalSec * currentSampleRate));
    // Map 0–100 → factor 1.0–0.5: full slider range stays musical, echoes never go silent
    float decayFactor = juce::jmap (cfg.delayDecay / 100.f, 0.f, 1.f, 1.0f, 0.5f);

    juce::ScopedLock lk (delayLock);
    for (int r = 1; r <= cfg.delayRepeats; ++r)
    {
        if (delayQueue.size() >= 256) break;
        float dv = juce::jlimit (1.f, 127.f, (float)vel * std::powf (decayFactor, (float)(r - 1)));
        delayQueue.push_back ({ startSample + (juce::int64)r * intervalSmp,
                                juce::MidiMessage::noteOn (midiCh, note, (juce::uint8)(int)dv),
                                padIdx });
    }
    std::sort (delayQueue.begin(), delayQueue.end(),
        [] (const DelayEvent& a, const DelayEvent& b) { return a.fireAtSample < b.fireAtSample; });
}

void DDD1HubProcessor::scheduleClockEchoes (int note, juce::uint8 vel, int padIdx, int midiCh)
{
    const auto& cfg = pads[padIdx];
    int ppb = pulsesPerRetrig (cfg.delayRate);
    // Map 0–100 → factor 1.0–0.5: full slider range stays musical, echoes never go silent
    float decayFactor = juce::jmap (cfg.delayDecay / 100.f, 0.f, 1.f, 1.0f, 0.5f);
    auto& echoes = padKb[padIdx].clockEchos;
    for (int r = 1; r <= cfg.delayRepeats; ++r)
    {
        float dv = juce::jlimit (1.f, 127.f, (float)vel * std::powf (decayFactor, (float)(r - 1)));
        echoes.push_back ({ note, (juce::uint8)(int)dv, midiCh, r * ppb });
    }
}

void DDD1HubProcessor::sendInitialKbNote (int padIdx, int note, juce::uint8 vel,
                                          int t, juce::MidiBuffer& out,
                                          int midiCh, float bpm)
{
    const auto& cfg = pads[padIdx];
    auto& kb = padKb[padIdx];

    // Purge stale delay echoes from the previous note on this pad to avoid ghost triggers
    {
        juce::ScopedLock lk (delayLock);
        delayQueue.erase (
            std::remove_if (delayQueue.begin(), delayQueue.end(),
                [padIdx] (const DelayEvent& e) { return e.padIdx == padIdx; }),
            delayQueue.end());
    }

    int lfoOff  = cfg.lfoEnabled
                ? lfoOffset (kb.lfoPhase, cfg.lfoDepth, cfg.lfoShape, kb.lfoOneShotDone)
                : 0;
    int seqTune = juce::jlimit (72, 96, 84 + (note - 72) + cfg.semitoneOffset + lfoOff);

    if (kb.activeSeqTunes[note] >= 0)
        out.addEvent (juce::MidiMessage::noteOff (midiCh, kb.activeSeqTunes[note]), t);
    kb.activeSeqTunes[note] = seqTune;
    juce::uint8 kbVel = (juce::uint8)juce::jlimit (1, 127, cfg.kbVelocity);
    out.addEvent (juce::MidiMessage::noteOn (midiCh, seqTune, (juce::uint8)64), t);
    out.addEvent (juce::MidiMessage::noteOn (midiCh, cfg.instKey, kbVel), t);

    if (cfg.delayEnabled)
    {
        juce::int64 start = totalSamples + (juce::int64)t;
        scheduleDelayEchoes (seqTune,     64,    padIdx, start, midiCh, bpm);
        scheduleDelayEchoes (cfg.instKey, kbVel, padIdx, start, midiCh, bpm);
    }
}

void DDD1HubProcessor::fireKbRetrigger (int padIdx, int note, juce::uint8 origVel,
                                        int t, juce::MidiBuffer& out, int midiCh, float bpm)
{
    const auto& cfg = pads[padIdx];
    auto& kb = padKb[padIdx];

    int& rc = kb.retrigCounts[note];
    rc++;
    if (cfg.retriggMax > 0 && rc > cfg.retriggMax)
        return;
    juce::uint8 vel = decayedVel ((juce::uint8)juce::jlimit (1, 127, cfg.kbVelocity),
                                  rc, cfg.retriggDecay);
    if (vel == 0) return;

    int lfoOff  = cfg.lfoEnabled
                ? lfoOffset (kb.lfoPhase, cfg.lfoDepth, cfg.lfoShape, kb.lfoOneShotDone)
                : 0;
    int seqTune = juce::jlimit (72, 96, 84 + (note - 72) + cfg.semitoneOffset + lfoOff);

    if (kb.activeSeqTunes[note] >= 0)
        out.addEvent (juce::MidiMessage::noteOff (midiCh, kb.activeSeqTunes[note]), t);
    kb.activeSeqTunes[note] = seqTune;
    out.addEvent (juce::MidiMessage::noteOn (midiCh, seqTune, (juce::uint8)64), t);
    out.addEvent (juce::MidiMessage::noteOn (midiCh, cfg.instKey, vel), t);

    if (cfg.delayEnabled)
    {
        juce::int64 start = totalSamples + (juce::int64)t;
        scheduleDelayEchoes (seqTune,     64,  padIdx, start, midiCh, bpm);
        scheduleDelayEchoes (cfg.instKey, vel, padIdx, start, midiCh, bpm);
    }
}

// ── processBlock ──────────────────────────────────────────────────────────────

void DDD1HubProcessor::processBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    audio.clear();
    const int   numSamples = audio.getNumSamples();
    const int   midiCh     = (int)*apvts.getRawParameterValue ("midiChannel");
    const float userBpm    = *apvts.getRawParameterValue ("bpm");
    const float bpm        = clockSyncActive ? detectedBpm : userBpm;

    // ── Steal input buffers (callback thread → audio thread) ─────────────────
    juce::MidiBuffer ddd1Msgs;
    {
        juce::ScopedLock lk (ddd1InLock);
        ddd1Msgs = std::move (ddd1InBuf);
        ddd1InBuf.clear();
    }
    juce::MidiBuffer kbMsgs;
    {
        juce::ScopedLock lk (kbInLock);
        kbMsgs = std::move (kbInBuf);
        kbInBuf.clear();
    }

    juce::MidiBuffer outBuf;

    samplesSinceLastClock += numSamples;
    clockSyncActive = (samplesSinceLastClock < (int)(currentSampleRate * 2.0));

    // ── Transport / clock (from Ableton IAC via midi, or DDD-1 via ddd1Msgs) ──
    // Called from both loops; only clock/start/stop are processed here.
    auto processTransport = [&](const juce::MidiMessage& msg, int samplePos)
    {
        if (msg.isMidiClock())
        {
            if (!transportRunning)
                transportRunning = true;   // auto-start: clock without prior MidiStart (DDD-1 was already playing)
            juce::int64 now = totalSamples + samplePos;
            if (lastClockSampleTime >= 0)
            {
                double interval = (now - lastClockSampleTime) / currentSampleRate;
                if (interval > 0.0) // guard: two clocks at same sample position → skip to avoid inf BPM
                {
                    double instantBpm = 60.0 / (interval * 24.0);
                    detectedBpm = (float)(0.9 * detectedBpm + 0.1 * instantBpm);
                    detectedBpm = juce::jlimit (20.f, 400.f, detectedBpm);
                }
            }
            lastClockSampleTime   = now;
            samplesSinceLastClock = 0;

            // Fire clock-domain delay echoes (pulse-accurate, no BPM estimation)
            for (int p = 0; p < numPads; ++p)
            {
                auto& echoes = padKb[p].clockEchos;
                for (auto& e : echoes)
                {
                    if (--e.pulsesLeft == 0)
                    {
                        outBuf.addEvent (juce::MidiMessage::noteOn (e.midiCh, e.note, e.vel), samplePos);
                        delayEchoLastFired[e.note] = totalSamples + samplePos;
                    }
                }
                echoes.erase (std::remove_if (echoes.begin(), echoes.end(),
                    [] (const PadKbState::ClockEcho& e) { return e.pulsesLeft <= 0; }),
                    echoes.end());
            }

            // Advance pending group trigs (deferred fan-outs from GroupedTrigs mode)
            for (auto& pg : pendingGroupTrigs)
            {
                if (--pg.pulsesRemaining <= 0)
                {
                    if (pg.targetPadIdx >= 0 && pg.targetPadIdx < numPads)
                    {
                        int seqTune = juce::jlimit (72, 96, 84 + pg.seqTune);
                        outBuf.addEvent (juce::MidiMessage::noteOn (pg.midiCh, seqTune, pg.velocity), samplePos);
                        outBuf.addEvent (juce::MidiMessage::noteOn (pg.midiCh, pads[pg.targetPadIdx].instKey, pg.velocity), samplePos);
                    }
                }
            }
            pendingGroupTrigs.erase (
                std::remove_if (pendingGroupTrigs.begin(), pendingGroupTrigs.end(),
                    [] (const PendingGroupTrig& pg) { return pg.pulsesRemaining <= 0; }),
                pendingGroupTrigs.end());

            for (int p = 0; p < numPads; ++p)
            {
                const auto& cfg = pads[p];
                auto& kb = padKb[p];

                if (cfg.muted) continue;

                if (cfg.mode == PadMode::Keyboard)
                {
                    if (cfg.retriggEnabled && !kb.heldNotes.empty())
                    {
                        if (--kb.pulsesToNextRetrig <= 0)
                        {
                            kb.pulsesToNextRetrig = pulsesPerRetrig (cfg.retriggRate);
                            for (auto& [note, vel] : kb.heldNotes)
                                fireKbRetrigger (p, note, vel, samplePos, outBuf, midiCh, bpm);
                        }
                    }

                    if (cfg.lfoEnabled)
                    {
                        double inc = 1.0 / pulsesPerLFO (cfg.lfoRate);
                        if (cfg.lfoMode == 0)
                        {
                            kb.lfoPhase += inc;
                            if (kb.lfoPhase >= 1.0) kb.lfoPhase -= 1.0;
                        }
                        else if (!kb.lfoOneShotDone)
                        {
                            kb.lfoPhase += inc;
                            if (kb.lfoPhase >= 1.0) { kb.lfoPhase = 1.0; kb.lfoOneShotDone = true; }
                        }
                    }
                }
                else if (cfg.mode == PadMode::Arpeggiator)
                {
                    if (!transportRunning) continue;
                    if (kb.arpNotes.empty()) continue;

                    if (--kb.pulsesToNextArp <= 0)
                    {
                        kb.pulsesToNextArp = pulsesPerRetrig (cfg.arpRate);

                        if (kb.arpLastNote >= 0)
                        {
                            int prevTune = juce::jlimit (72, 96, 84 + (kb.arpLastNote - 72) + cfg.semitoneOffset);
                            outBuf.addEvent (juce::MidiMessage::noteOff (midiCh, prevTune), samplePos);
                            outBuf.addEvent (juce::MidiMessage::noteOff (midiCh, cfg.instKey), samplePos);
                            kb.arpLastNote = -1;
                        }

                        if (!kb.arpNotes.empty())
                        {
                            int poolSize = (int)kb.arpNotes.size();
                            int idx      = kb.arpStep % poolSize;
                            int note     = kb.arpNotes[(size_t)idx];
                            int seqTune  = juce::jlimit (72, 96, 84 + (note - 72) + cfg.semitoneOffset);
                            outBuf.addEvent (juce::MidiMessage::noteOn (midiCh, seqTune,     (juce::uint8)64),              samplePos);
                            outBuf.addEvent (juce::MidiMessage::noteOn (midiCh, cfg.instKey, kb.arpHitVelocity),            samplePos);
                            delayEchoLastFired[cfg.instKey] = totalSamples + samplePos; // guard MIDI THRU
                            padHitSeq[p].fetch_add (1, std::memory_order_relaxed);
                            kb.arpLastNote = note;
                            advanceArpStep (kb, cfg, poolSize);
                            if (cfg.delayEnabled)
                            {
                                scheduleClockEchoes (seqTune,     64,                  p, midiCh);
                                scheduleClockEchoes (cfg.instKey, kb.arpHitVelocity,  p, midiCh);
                            }
                        }
                    }
                }
                else if (cfg.mode == PadMode::PatternBank)
                {
                    if (!transportRunning) continue;

                    // Derive step position from global clock so patterns always stay
                    // in sync with the grid regardless of when the mode was activated.
                    int pulsesPerStep = pulsesPerRetrig (cfg.patternResolution);
                    if (globalPulseCounter % pulsesPerStep != 0) continue;

                    int globalStep = globalPulseCounter / pulsesPerStep;

                    // Load pattern steps (may be empty / no pattern selected)
                    std::vector<PatternStep> localSteps;
                    if (cfg.selectedPatternId.isNotEmpty())
                    {
                        juce::ScopedLock lk (patternBankLock);
                        const RhythmPattern* pat = patternBank.findById (cfg.selectedPatternId);
                        if (pat && !pat->steps.empty())
                            localSteps = pat->steps;
                    }

                    int numSteps = localSteps.empty() ? 16 : (int)localSteps.size();

                    // Compute step index
                    int stepIdx;
                    stepIdx = (globalStep + cfg.patternOffset) % numSteps;

                    // Always update playhead — even with no/empty pattern
                    kb.patternPlayingStep  = stepIdx;
                    kb.patternCurrentStep  = stepIdx;

                    if (localSteps.empty()) continue;

                    // NoteOff for previous step
                    if (kb.patternLastNote >= 0)
                    {
                        outBuf.addEvent (juce::MidiMessage::noteOff (midiCh, kb.patternLastNote), samplePos);
                        outBuf.addEvent (juce::MidiMessage::noteOff (midiCh, cfg.instKey), samplePos);
                        kb.patternLastNote = -1;
                    }

                    const auto& step = localSteps[(size_t)stepIdx];
                    if (step.hit)
                    {
                        int seqTune = juce::jlimit (72, 96, 84 + step.tune);
                        outBuf.addEvent (juce::MidiMessage::noteOn (midiCh, seqTune, (juce::uint8)64), samplePos);
                        outBuf.addEvent (juce::MidiMessage::noteOn (midiCh, cfg.instKey, (juce::uint8)step.velocity), samplePos);
                        padHitSeq[p].fetch_add (1, std::memory_order_relaxed);
                        kb.patternLastNote = seqTune;
                        if (cfg.delayEnabled)
                        {
                            scheduleClockEchoes (seqTune,     64,                         p, midiCh);
                            scheduleClockEchoes (cfg.instKey, (juce::uint8)step.velocity, p, midiCh);
                        }
                    }
                }
            }

            // ── Global pulse counter (for editor playhead) ────────────────
            globalPulseCounter++;
            int pulsesPerPattern = patternLengthBars * 96;
            if (globalPulseCounter >= pulsesPerPattern)
                globalPulseCounter = 0;
            currentGlobalStep = (globalPulseCounter * patternTotalSteps) / pulsesPerPattern;
        }
        else if (msg.isMidiStart())
        {
            transportRunning    = true;
            globalPulseCounter  = 0;
            currentGlobalStep   = 0;
            for (int p = 0; p < numPads; ++p)
            {
                padKb[p].pulsesToNextRetrig = pulsesPerRetrig (pads[p].retriggRate);
                padKb[p].pulsesToNextArp    = pulsesPerRetrig (pads[p].arpRate);
                padKb[p].patternCurrentStep  = 0;
                padKb[p].patternPlayingStep  = 0;
                padKb[p].patternLastNote     = -1;
                if (pads[p].lfoMode == 0) padKb[p].lfoPhase = 0.0;
            }
        }
        else if (msg.isMidiStop())
        {
            transportRunning = false;
            // Send explicit noteOffs for any ringing pattern/arp notes before resetting state
            for (int p = 0; p < numPads; ++p)
            {
                if (padKb[p].patternLastNote >= 0)
                {
                    outBuf.addEvent (juce::MidiMessage::noteOff (midiCh, padKb[p].patternLastNote), 0);
                    outBuf.addEvent (juce::MidiMessage::noteOff (midiCh, pads[p].instKey), 0);
                }
                if (padKb[p].arpLastNote >= 0)
                {
                    int t = juce::jlimit (72, 96, 84 + (padKb[p].arpLastNote - 72) + pads[p].semitoneOffset);
                    outBuf.addEvent (juce::MidiMessage::noteOff (midiCh, t), 0);
                    outBuf.addEvent (juce::MidiMessage::noteOff (midiCh, pads[p].instKey), 0);
                }
            }
            {
                juce::ScopedLock lk (delayLock);
                delayQueue.clear();
            }
            pendingGroupTrigs.clear();
            for (int p = 0; p < numPads; ++p)
            {
                heldBitsLo[p].store (0, std::memory_order_relaxed);
                heldBitsHi[p].store (0, std::memory_order_relaxed);
                padKb[p].heldNotes.clear();
                padKb[p].retrigCounts.clear();
                padKb[p].arpNotes.clear();
                padKb[p].arpLastNote        = -1;
                padKb[p].patternCurrentStep = 0;
                padKb[p].patternPlayingStep = 0;
                padKb[p].patternLastNote    = -1;
                padKb[p].clockEchos.clear();
            }
            for (int ch = 1; ch <= 16; ++ch)
                outBuf.addEvent (juce::MidiMessage::allNotesOff (ch), 0);
        }
    };

    // midi buffer = fallback clock source (DAW or auto-connected devices).
    // Only used when no explicit DDD-1 input is selected; avoids double-counting
    // when DDD-1 is also connected (JUCE auto-routes it into this buffer too).
    if (ddd1InputId.isEmpty())
        for (auto meta : midi)
            processTransport (meta.getMessage(), meta.samplePosition);

    // ── Keyboard notes — exclusively from the explicit KB In port ─────────────
    auto processKbMsg = [&](const juce::MidiMessage& msg, int t)
    {
        if (msg.isNoteOn())
        {
            int         note = msg.getNoteNumber();
            juce::uint8 vel  = msg.getVelocity();
            for (int p = 0; p < numPads; ++p)
            {
                auto& kb = padKb[p];
                if (pads[p].muted) continue;

                if (pads[p].mode == PadMode::Keyboard)
                {
                    kb.heldNotes[note]    = vel;
                    kb.retrigCounts[note] = 0;
                    if (note < 64) heldBitsLo[p].fetch_or  (1ULL << note,         std::memory_order_relaxed);
                    else           heldBitsHi[p].fetch_or  (1ULL << (note - 64),  std::memory_order_relaxed);
                    kb.lfoPhase = 0.0;
                    kb.lfoOneShotDone = false;
                    if (!clockSyncActive) kb.retriggPhase = 0.0;
                    if (clockSyncActive && pads[p].retriggEnabled)
                    {
                        int ppb = pulsesPerRetrig (pads[p].retriggRate);
                        int rem = globalPulseCounter % ppb;
                        if (rem <= ppb / 2)
                        {
                            // First half of period: close to last beat → fire now, align next retrig
                            sendInitialKbNote (p, note, vel, t, outBuf, midiCh, bpm);
                            kb.pulsesToNextRetrig = (rem == 0) ? ppb : (ppb - rem);
                        }
                        else
                        {
                            // Second half: closer to next beat → quantize forward, skip initial fire
                            kb.pulsesToNextRetrig = ppb - rem;
                        }
                    }
                    else
                    {
                        sendInitialKbNote (p, note, vel, t, outBuf, midiCh, bpm);
                    }
                    // Record into pattern if overdub enabled
                    if (pads[p].overdubEnabled && transportRunning &&
                        pads[p].selectedPatternId.isNotEmpty())
                    {
                        juce::ScopedLock lk (patternBankLock);
                        const RhythmPattern* pat = patternBank.findById (pads[p].selectedPatternId);
                        if (pat && !pat->steps.empty())
                        {
                            RhythmPattern updated = *pat;
                            int numSteps  = (int)updated.steps.size();
                            int step      = (globalPulseCounter / pulsesPerRetrig (1)) % numSteps;
                            updated.steps[(size_t)step].hit      = true;
                            updated.steps[(size_t)step].tune     = note - 72;
                            updated.steps[(size_t)step].velocity = vel;
                            if (!patternBank.overwrite (pads[p].selectedPatternId, updated))
                                patternBank.insert (updated);
                            overdubDirty[p].store (true, std::memory_order_relaxed);
                        }
                    }
                }
                else if (pads[p].mode == PadMode::Arpeggiator)
                {
                    kb.heldNotes[note] = vel;
                    rebuildArpPool (kb, pads[p]);
                    if (clockSyncActive)
                    {
                        int ppb = pulsesPerRetrig (pads[p].arpRate);
                        kb.pulsesToNextArp = ppb - (globalPulseCounter % ppb);
                    }
                }
            }

        }
        else if (msg.isNoteOff())
        {
            int note = msg.getNoteNumber();
            for (int p = 0; p < numPads; ++p)
            {
                auto& kb = padKb[p];

                if (pads[p].mode == PadMode::Keyboard)
                {
                    kb.heldNotes.erase (note);
                    kb.retrigCounts.erase (note);
                    if (note < 64) heldBitsLo[p].fetch_and (~(1ULL << note),        std::memory_order_relaxed);
                    else           heldBitsHi[p].fetch_and (~(1ULL << (note - 64)), std::memory_order_relaxed);
                    outBuf.addEvent (juce::MidiMessage::noteOff (midiCh, pads[p].instKey), t);
                    if (kb.activeSeqTunes[note] >= 0)
                    {
                        outBuf.addEvent (juce::MidiMessage::noteOff (midiCh, kb.activeSeqTunes[note]), t);
                        kb.activeSeqTunes[note] = -1;
                    }
                }
                else if (pads[p].mode == PadMode::Arpeggiator)
                {
                    if (!pads[p].arpLatch)
                    {
                        kb.heldNotes.erase (note);
                        rebuildArpPool (kb, pads[p]);
                        if (kb.arpNotes.empty() && kb.arpLastNote >= 0)
                        {
                            int prevTune = juce::jlimit (72, 96, 84 + (kb.arpLastNote - 72) + pads[p].semitoneOffset);
                            outBuf.addEvent (juce::MidiMessage::noteOff (midiCh, prevTune), t);
                            outBuf.addEvent (juce::MidiMessage::noteOff (midiCh, pads[p].instKey), t);
                            kb.arpLastNote = -1;
                        }
                    }
                }
            }
        }
        // KB In carries notes only — clock is handled by ddd1In or the midi buffer.
    };

    for (auto meta : kbMsgs)
        processKbMsg (meta.getMessage(), meta.samplePosition);

    // ── DDD-1 pad hits (from DDD-1 MIDI OUT → callback buffer) ───────────────
    bool ddd1NoteSeen[128] = {}; // dedup: ignore second hit of same note in one block
    for (auto meta : ddd1Msgs)
    {
        auto msg = meta.getMessage();
        const int t = 0; // placed at block start

        if (msg.isNoteOn())
        {
            int note   = msg.getNoteNumber();
            int padIdx = -1;
            for (int p = 0; p < numPads; ++p)
                if (pads[p].noteReceive == note) { padIdx = p; break; }

            if (ddd1NoteSeen[note]) continue; // same note twice in one block → skip
            ddd1NoteSeen[note] = true;

            if (padIdx < 0) continue; // unknown note: ignore (don't echo back)

            padHitSeq[padIdx].fetch_add (1, std::memory_order_relaxed);
            if (pads[padIdx].muted) continue;

            switch (pads[padIdx].mode)
            {
                case PadMode::PassThrough:
                {
                    // DDD-1 already plays this hit from its sequencer.
                    // Only schedule delay echoes if enabled; guard against MIDI THRU bounces.
                    juce::uint8 hitVel = msg.getVelocity();
                    if (pads[padIdx].delayEnabled)
                    {
                        juce::int64 elapsed = totalSamples + t - delayEchoLastFired[note];
                        if (elapsed >= (juce::int64)(currentSampleRate * 0.03))
                            scheduleDelayEchoes (note, hitVel, padIdx,
                                                 totalSamples + t, midiCh, bpm);
                    }
                    break;
                }

                case PadMode::Keyboard:
                    // KB In controls this pad — DDD-1 hits are ignored.
                    break;

                case PadMode::Arpeggiator:
                {
                    auto& kb        = padKb[padIdx];
                    const auto& cfg = pads[padIdx];

                    // Debounce: ignore MIDI THRU echo of our own arp noteOn(instKey)
                    {
                        juce::int64 elapsed = totalSamples + t - delayEchoLastFired[note];
                        if (elapsed < (juce::int64)(currentSampleRate * 0.03)) break;
                    }

                    if (kb.heldNotes.empty())
                    {
                        // No keyboard held: build pitch sweep from seqTune range [72..96]
                        kb.arpNotes.clear();
                        int n = juce::jmax (1, cfg.arpOctaves);
                        if (n == 1)
                        {
                            kb.arpNotes.push_back (72); // → seqTune 84 (centre pitch)
                        }
                        else
                        {
                            for (int i = 0; i < n; ++i)
                            {
                                int seqT = 72 + i * 24 / (n - 1); // evenly spaced 72–96
                                seqT = juce::jlimit (72, 96, seqT);
                                int poolNote = seqT - 12; // inverse of seqTune formula
                                if (kb.arpNotes.empty() || kb.arpNotes.back() != poolNote)
                                    kb.arpNotes.push_back (poolNote);
                            }
                        }
                    }
                    else
                    {
                        rebuildArpPool (kb, cfg);
                    }

                    // Restart sequence on each hit
                    kb.arpStep       = (cfg.arpDirection == 1 && !kb.arpNotes.empty())
                                     ? (int)kb.arpNotes.size() - 1 : 0;
                    kb.arpPingDir    = 1;
                    kb.arpHitVelocity = msg.getVelocity();

                    if (clockSyncActive && !kb.arpNotes.empty())
                    {
                        int ppb = pulsesPerRetrig (cfg.arpRate);
                        int rem = globalPulseCounter % ppb;
                        if (rem <= ppb / 2)
                        {
                            // First half: close to last beat → fire first step now, align to grid
                            int poolSize = (int)kb.arpNotes.size();
                            int idx      = kb.arpStep % poolSize;
                            int note2    = kb.arpNotes[(size_t)idx];
                            int seqTune  = juce::jlimit (72, 96, 84 + (note2 - 72) + cfg.semitoneOffset);
                            if (kb.arpLastNote >= 0)
                            {
                                int prev = juce::jlimit (72, 96, 84 + (kb.arpLastNote - 72) + cfg.semitoneOffset);
                                outBuf.addEvent (juce::MidiMessage::noteOff (midiCh, prev),        t);
                                outBuf.addEvent (juce::MidiMessage::noteOff (midiCh, cfg.instKey), t);
                            }
                            outBuf.addEvent (juce::MidiMessage::noteOn (midiCh, seqTune,     (juce::uint8)64),   t);
                            outBuf.addEvent (juce::MidiMessage::noteOn (midiCh, cfg.instKey, kb.arpHitVelocity), t);
                            delayEchoLastFired[cfg.instKey] = totalSamples + t;
                            kb.arpLastNote = note2;
                            advanceArpStep (kb, cfg, poolSize);
                            kb.pulsesToNextArp = (rem == 0) ? ppb : (ppb - rem);
                        }
                        else
                        {
                            kb.pulsesToNextArp = ppb - rem;
                        }
                    }
                    break;
                }

                case PadMode::PatternBank:
                {
                    // Overdub: merge hit into the playing pattern at the current step
                    if (pads[padIdx].overdubEnabled && transportRunning &&
                        pads[padIdx].selectedPatternId.isNotEmpty())
                    {
                        juce::ScopedLock lk (patternBankLock);
                        const RhythmPattern* pat = patternBank.findById (pads[padIdx].selectedPatternId);
                        if (pat && !pat->steps.empty())
                        {
                            RhythmPattern updated = *pat;
                            int step = juce::jlimit (0, (int)updated.steps.size() - 1,
                                                     padKb[padIdx].patternCurrentStep);
                            updated.steps[(size_t)step].hit      = true;
                            updated.steps[(size_t)step].velocity = msg.getVelocity();
                            if (!patternBank.overwrite (pads[padIdx].selectedPatternId, updated))
                                patternBank.insert (updated);
                            overdubDirty[padIdx].store (true, std::memory_order_relaxed);
                        }
                    }
                    // DDD-1 already plays the hit; schedule delay echoes for direct pad hits
                    if (pads[padIdx].delayEnabled)
                    {
                        juce::int64 elapsed = totalSamples + t - delayEchoLastFired[note];
                        if (elapsed >= (juce::int64)(currentSampleRate * 0.03))
                            scheduleDelayEchoes (note, msg.getVelocity(), padIdx,
                                                 totalSamples + t, midiCh, bpm);
                    }
                    break;
                }

                case PadMode::GroupedTrigs:
                {
                    juce::uint8 hitVel = msg.getVelocity();
                    std::vector<GroupTarget> localTargets;
                    {
                        juce::ScopedLock lk (groupLock);
                        localTargets = pads[padIdx].groupTargets;
                    }
                    for (const auto& tgt : localTargets)
                    {
                        if (tgt.padIndex < 0 || tgt.padIndex >= numPads) continue;
                        juce::uint8 outVel = (juce::uint8)juce::jlimit (1, 127,
                            (int)hitVel * tgt.velocityScale / 100);
                        int seqTune = juce::jlimit (72, 96, 84 + tgt.tuneOffset);
                        if (tgt.offsetPulses <= 0)
                        {
                            outBuf.addEvent (juce::MidiMessage::noteOn (midiCh, seqTune,                 outVel), t);
                            outBuf.addEvent (juce::MidiMessage::noteOn (midiCh, pads[tgt.padIndex].instKey, outVel), t);
                        }
                        else
                        {
                            pendingGroupTrigs.push_back ({ tgt.padIndex, tgt.offsetPulses,
                                                          outVel, tgt.tuneOffset, midiCh });
                        }
                    }
                    break;
                }
            }
        }
        else if (msg.isMidiClock() || msg.isMidiStart() || msg.isMidiStop())
        {
            // DDD-1 can be the MIDI clock master — process transport from its MIDI OUT too.
            processTransport (msg, t);
        }
        // Active Sensing and other system messages are NOT forwarded back.
    }

    // ── Fire pending delay events ─────────────────────────────────────────────
    {
        juce::ScopedLock lk (delayLock);
        juce::int64 blockEnd = totalSamples + numSamples;
        while (!delayQueue.empty() && delayQueue.front().fireAtSample < blockEnd)
        {
            int samplePos = (int)juce::jlimit (
                (juce::int64)0, (juce::int64)(numSamples - 1),
                delayQueue.front().fireAtSample - totalSamples);
            int echoNote = delayQueue.front().msg.getNoteNumber();
            outBuf.addEvent (delayQueue.front().msg, samplePos);
            delayEchoLastFired[echoNote] = totalSamples + samplePos;
            delayQueue.erase (delayQueue.begin());
        }
    }

    // ── Free-mode retrigger for keyboard pads ────────────────────────────────
    if (!clockSyncActive)
    {
        for (int p = 0; p < numPads; ++p)
        {
            const auto& cfg = pads[p];
            auto& kb = padKb[p];

            if (cfg.mode == PadMode::Keyboard)
            {
                if (!cfg.retriggEnabled || kb.heldNotes.empty()) continue;

                double rHz  = retriggHz (cfg.retriggRate, bpm);
                double lHz  = lfoHz     (cfg.lfoRate,     bpm);
                double rInc = rHz / currentSampleRate;
                double lInc = lHz / currentSampleRate;

                for (int s = 0; s < numSamples; ++s)
                {
                    kb.retriggPhase += rInc;
                    if (kb.retriggPhase >= 1.0)
                    {
                        kb.retriggPhase -= 1.0;
                        for (auto& [note, vel] : kb.heldNotes)
                            fireKbRetrigger (p, note, vel, s, outBuf, midiCh, bpm);
                    }
                    if (cfg.lfoEnabled)
                    {
                        if (cfg.lfoMode == 0)
                        {
                            kb.lfoPhase += lInc;
                            if (kb.lfoPhase >= 1.0) kb.lfoPhase -= 1.0;
                        }
                        else if (!kb.lfoOneShotDone)
                        {
                            kb.lfoPhase += lInc;
                            if (kb.lfoPhase >= 1.0) { kb.lfoPhase = 1.0; kb.lfoOneShotDone = true; }
                        }
                    }
                }
            }
            else if (cfg.mode == PadMode::Arpeggiator)
            {
                if (kb.arpNotes.empty()) continue;
                int    poolSize = (int)kb.arpNotes.size();
                double rInc     = retriggHz (cfg.arpRate, bpm) / currentSampleRate;

                for (int s = 0; s < numSamples; ++s)
                {
                    kb.retriggPhase += rInc;
                    if (kb.retriggPhase >= 1.0)
                    {
                        kb.retriggPhase -= 1.0;

                        if (kb.arpLastNote >= 0)
                        {
                            int prevTune = juce::jlimit (72, 96, 84 + (kb.arpLastNote - 72) + cfg.semitoneOffset);
                            outBuf.addEvent (juce::MidiMessage::noteOff (midiCh, prevTune), s);
                            outBuf.addEvent (juce::MidiMessage::noteOff (midiCh, cfg.instKey), s);
                            kb.arpLastNote = -1;
                        }

                        if (!kb.arpNotes.empty())
                        {
                            int idx     = kb.arpStep % poolSize;
                            int note    = kb.arpNotes[(size_t)idx];
                            int seqTune = juce::jlimit (72, 96, 84 + (note - 72) + cfg.semitoneOffset);
                            outBuf.addEvent (juce::MidiMessage::noteOn (midiCh, seqTune,     (juce::uint8)64),   s);
                            outBuf.addEvent (juce::MidiMessage::noteOn (midiCh, cfg.instKey, kb.arpHitVelocity), s);
                            delayEchoLastFired[cfg.instKey] = totalSamples + s; // guard MIDI THRU
                            kb.arpLastNote = note;
                            advanceArpStep (kb, cfg, poolSize);
                            if (cfg.delayEnabled)
                            {
                                juce::int64 start = totalSamples + (juce::int64)s;
                                scheduleDelayEchoes (seqTune,     64,  p, start, midiCh, bpm);
                                scheduleDelayEchoes (cfg.instKey, 100, p, start, midiCh, bpm);
                            }
                        }
                    }
                }
            }
        }
    }

    // ── Send to DDD-1 ─────────────────────────────────────────────────────────
    {
        juce::ScopedLock lk (midiOutLock);
        if (midiOut && !outBuf.isEmpty())
            midiOut->sendBlockOfMessagesNow (outBuf);
    }

    totalSamples += numSamples;
    midi.clear();
}

// ── Arpeggiator helpers ───────────────────────────────────────────────────────

void DDD1HubProcessor::rebuildArpPool (PadKbState& kb, const PadConfig& cfg)
{
    kb.arpNotes.clear();
    for (auto& [note, vel] : kb.heldNotes)
        for (int oct = 0; oct < cfg.arpOctaves; ++oct)
            kb.arpNotes.push_back (juce::jlimit (0, 127, note + oct * 12));
    std::sort (kb.arpNotes.begin(), kb.arpNotes.end());
    // Down direction starts at the highest note; pool stays ascending
    kb.arpStep    = (cfg.arpDirection == 1 && !kb.arpNotes.empty())
                  ? (int)kb.arpNotes.size() - 1 : 0;
    kb.arpPingDir = 1;
}

void DDD1HubProcessor::advanceArpStep (PadKbState& kb, const PadConfig& cfg, int poolSize)
{
    if (poolSize == 0) return;
    switch (cfg.arpDirection)
    {
        case 0: // Up
            kb.arpStep = (kb.arpStep + 1) % poolSize;
            break;
        case 1: // Down
            kb.arpStep = (kb.arpStep - 1 + poolSize) % poolSize;
            break;
        case 2: // UpDown
            kb.arpStep += kb.arpPingDir;
            if (kb.arpStep >= poolSize - 1) kb.arpPingDir = -1;
            if (kb.arpStep <= 0)            kb.arpPingDir = +1;
            kb.arpStep = juce::jlimit (0, poolSize - 1, kb.arpStep);
            break;
        case 3: // Random
            kb.arpStep = juce::Random::getSystemRandom().nextInt (poolSize);
            break;
        default: break;
    }
}

// ── Pattern Bank ──────────────────────────────────────────────────────────────

void DDD1HubProcessor::loadPatternBank (const juce::File& f)
{
    juce::ScopedLock lk (patternBankLock);
    patternBankFilePath = f.getFullPathName();
    if (f.isDirectory())
        patternBank.loadDirectory (f);
    else
        patternBank.load (f);
}

void DDD1HubProcessor::savePatternBank()
{
    juce::ScopedLock lk (patternBankLock);
    patternBank.save (juce::File (patternBankFilePath));
}


void DDD1HubProcessor::updateLivePattern (const juce::String& id, const RhythmPattern& updated)
{
    juce::ScopedLock lk (patternBankLock);
    if (!patternBank.overwrite (id, updated))
        patternBank.insert (updated);   // force-add: live patterns may share hash with other empty slots
}

// ── Pattern Set Bank ─────────────────────────────────────────────────────────

void DDD1HubProcessor::loadPatternSetBank (const juce::File& f)
{
    juce::ScopedLock lk (patternSetBankLock);
    patternSetBankFilePath = f.getFullPathName();
    patternSetBank.load (f);
}

void DDD1HubProcessor::savePatternSetBank()
{
    juce::ScopedLock lk (patternSetBankLock);
    patternSetBank.save (juce::File (patternSetBankFilePath));
}

void DDD1HubProcessor::applyPatternSet (const PatternSet& s)
{
    juce::ScopedLock lk (patternBankLock);
    for (const auto& a : s.assignments)
    {
        if (a.padIndex < 0 || a.padIndex >= numPads) continue;
        pads[a.padIndex].selectedPatternId = a.patternId;
        pads[a.padIndex].patternResolution = a.resolution;
        pads[a.padIndex].patternOffset     = a.offset;
        if (pads[a.padIndex].mode != PadMode::PatternBank)
            pads[a.padIndex].mode = PadMode::PatternBank;
    }
}

// ── Rating Bank ──────────────────────────────────────────────────────────────

static juce::File ratingsFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("DDD1MidiHub/ratings.json");
}

void DDD1HubProcessor::loadRatings()  { ratingBank.load (ratingsFile()); }
void DDD1HubProcessor::saveRatings()  { ratingBank.save (ratingsFile()); }

// ── Idea Bank ─────────────────────────────────────────────────────────────────

static juce::File ideasFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("DDD1MidiHub/ideas.json");
}

void DDD1HubProcessor::loadIdeas() { ideaBank.load (ideasFile()); }
void DDD1HubProcessor::saveIdeas() { ideaBank.save (ideasFile()); }

Idea DDD1HubProcessor::captureCurrentIdea (const juce::String& name, const IdeaOrigin& origin)
{
    Idea idea;

    auto now   = juce::Time::getCurrentTime();
    idea.id    = "idea_" + now.formatted ("%Y%m%d") + "_"
               + juce::String (juce::Random::getSystemRandom().nextInt (99999)).paddedLeft ('0', 5);
    idea.name      = name;
    idea.createdAt = now.toISO8601 (true);
    idea.updatedAt = idea.createdAt;
    idea.origin    = origin;
    if (idea.origin.type.isEmpty())
        idea.origin.type = "scratch";

    // Capture current pattern set (pad assignments, no patterns yet)
    idea.patternSet        = captureCurrentPatternSet();
    idea.patternSet.source = "user";

    // Snapshot each assigned pattern; rewrite assignment IDs to idea-scoped copies
    {
        juce::ScopedLock lk (patternBankLock);
        for (auto& a : idea.patternSet.assignments)
        {
            if (const auto* p = patternBank.findById (a.patternId))
            {
                RhythmPattern snap = *p;
                snap.id     = "idea_" + idea.id + "_pad" + juce::String (a.padIndex);
                snap.source = "idea";
                a.patternId = snap.id;
                idea.patterns.push_back (std::move (snap));
            }
        }
    }

    // Snapshot all 14 pad configs (position = pad index)
    for (int i = 0; i < numPads; ++i)
        idea.padConfigs.push_back (pads[i]);

    // Align selectedPatternId in each pad config to its snapshot ID
    for (const auto& a : idea.patternSet.assignments)
        if (a.padIndex >= 0 && a.padIndex < numPads)
            idea.padConfigs[(size_t)a.padIndex].selectedPatternId = a.patternId;

    DBG ("captureCurrentIdea: \"" << name << "\"  patterns=" << (int)idea.patterns.size()
         << "  pads=" << (int)idea.padConfigs.size());
    return idea;
}

void DDD1HubProcessor::loadIdea (const juce::String& id)
{
    const Idea* idea = ideaBank.findById (id);
    if (!idea) return;

    // Inject pattern snapshots — idea version is authoritative over library
    {
        juce::ScopedLock lk (patternBankLock);
        for (const auto& p : idea->patterns)
            patternBank.insert (p);
    }

    // Restore all 14 pad configs; reset any pads not covered by saved config
    for (int i = 0; i < numPads; ++i)
        pads[i] = (i < (int)idea->padConfigs.size()) ? idea->padConfigs[(size_t)i] : PadConfig{};

    DBG ("loadIdea: \"" << idea->name << "\"  patterns=" << (int)idea->patterns.size());
}

void DDD1HubProcessor::saveIdea (Idea& idea)
{
    idea.updatedAt = juce::Time::getCurrentTime().toISO8601 (true);
    if (!ideaBank.overwrite (idea.id, idea))
        ideaBank.add (idea);
    saveIdeas();
}

// ─────────────────────────────────────────────────────────────────────────────

void DDD1HubProcessor::resetAllPatternAssignments()
{
    juce::ScopedLock lk (patternBankLock);
    for (int i = 0; i < numPads; ++i)
        pads[i].selectedPatternId = {};
}

PatternSet DDD1HubProcessor::captureCurrentPatternSet() const
{
    PatternSet s;
    s.id   = "scene_" + juce::String (juce::Random::getSystemRandom().nextInt (99999));
    s.name = "New Scene";
    for (int i = 0; i < numPads; ++i)
    {
        if (pads[i].selectedPatternId.isNotEmpty() &&
            !pads[i].selectedPatternId.startsWith ("__"))
        {
            PadAssignment a;
            a.padIndex   = i;
            a.patternId  = pads[i].selectedPatternId;
            a.resolution = pads[i].patternResolution;
            a.offset     = pads[i].patternOffset;
            s.assignments.push_back (a);
        }
    }
    return s;
}

// ── State serialization ───────────────────────────────────────────────────────

void DDD1HubProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("midiOutputId",       midiOutputId,        nullptr);
    state.setProperty ("ddd1InputId",       ddd1InputId,         nullptr);
    state.setProperty ("kbInputId",         kbInputId,           nullptr);
    state.setProperty ("patternBankPath",    patternBankFilePath,    nullptr);
    state.setProperty ("patternSetBankPath", patternSetBankFilePath, nullptr);
    state.setProperty ("patternLengthBars",  patternLengthBars,      nullptr);
    state.setProperty ("patternTotalSteps",  patternTotalSteps,      nullptr);

    auto padsTree = juce::ValueTree ("Pads");
    for (int i = 0; i < numPads; ++i)
    {
        auto pt = juce::ValueTree ("Pad");
        const auto& c = pads[i];
        pt.setProperty ("noteReceive",    c.noteReceive,    nullptr);
        pt.setProperty ("mode",           (int)c.mode,      nullptr);
        pt.setProperty ("instKey",        c.instKey,        nullptr);
        pt.setProperty ("semitoneOffset",  c.semitoneOffset, nullptr);
        pt.setProperty ("kbVelocity",     c.kbVelocity,     nullptr);
        pt.setProperty ("retriggEnabled", c.retriggEnabled, nullptr);
        pt.setProperty ("retriggRate",    c.retriggRate,    nullptr);
        pt.setProperty ("retriggMax",     c.retriggMax,     nullptr);
        pt.setProperty ("retriggDecay",   c.retriggDecay,   nullptr);
        pt.setProperty ("lfoEnabled",     c.lfoEnabled,     nullptr);
        pt.setProperty ("lfoMode",        c.lfoMode,        nullptr);
        pt.setProperty ("lfoRate",        c.lfoRate,        nullptr);
        pt.setProperty ("lfoDepth",       c.lfoDepth,       nullptr);
        pt.setProperty ("lfoShape",       c.lfoShape,       nullptr);
        pt.setProperty ("delayEnabled",        c.delayEnabled,       nullptr);
        pt.setProperty ("delayRate",          c.delayRate,          nullptr);
        pt.setProperty ("delayRepeats",       c.delayRepeats,       nullptr);
        pt.setProperty ("delayDecay",         c.delayDecay,         nullptr);
        pt.setProperty ("arpRate",            c.arpRate,            nullptr);
        pt.setProperty ("arpDirection",       c.arpDirection,       nullptr);
        pt.setProperty ("arpOctaves",         c.arpOctaves,         nullptr);
        pt.setProperty ("arpLatch",           c.arpLatch,           nullptr);
        pt.setProperty ("selectedPatternId",  c.selectedPatternId,  nullptr);
        pt.setProperty ("patternResolution",  c.patternResolution,  nullptr);
        pt.setProperty ("patternOffset",      c.patternOffset,      nullptr);
        pt.setProperty ("muted",              c.muted,              nullptr);
        pt.setProperty ("overdubEnabled",     c.overdubEnabled,     nullptr);
        auto gtTree = juce::ValueTree ("GroupTargets");
        for (const auto& tgt : c.groupTargets)
        {
            auto tt = juce::ValueTree ("T");
            tt.setProperty ("padIndex",      tgt.padIndex,      nullptr);
            tt.setProperty ("offsetPulses",  tgt.offsetPulses,  nullptr);
            tt.setProperty ("velocityScale", tgt.velocityScale, nullptr);
            tt.setProperty ("tuneOffset",    tgt.tuneOffset,    nullptr);
            gtTree.addChild (tt, -1, nullptr);
        }
        pt.addChild (gtTree, -1, nullptr);
        padsTree.addChild (pt, -1, nullptr);
    }
    state.addChild (padsTree, -1, nullptr);

    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void DDD1HubProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (!xml || !xml->hasTagName (apvts.state.getType())) return;

    auto state = juce::ValueTree::fromXml (*xml);
    apvts.replaceState (state);

    auto outId = state.getProperty ("midiOutputId", "").toString();
    if (outId.isNotEmpty()) openMidiOutput (outId);

    auto inId = state.getProperty ("ddd1InputId", "").toString();
    if (inId.isNotEmpty()) openDDD1Input (inId);

    auto kbId = state.getProperty ("kbInputId", "").toString();
    if (kbId.isNotEmpty()) openKbInput (kbId);

    auto bankPath = state.getProperty ("patternBankPath", "").toString();
    if (bankPath.isNotEmpty()) loadPatternBank (juce::File (bankPath));
    auto setBankPath = state.getProperty ("patternSetBankPath", "").toString();
    if (setBankPath.isNotEmpty()) loadPatternSetBank (juce::File (setBankPath));
    patternLengthBars = state.getProperty ("patternLengthBars", 1);
    patternTotalSteps = state.getProperty ("patternTotalSteps", 16);

    // Pad configs intentionally not restored — always start clean (PassThrough, no delay)
}

juce::AudioProcessorEditor* DDD1HubProcessor::createEditor()
    { return new DDD1HubEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
    { return new DDD1HubProcessor(); }
