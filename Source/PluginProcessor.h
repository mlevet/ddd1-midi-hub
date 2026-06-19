#pragma once
#include <JuceHeader.h>
#include "PatternBank.h"
#include "PatternSetBank.h"

enum class PadMode { PassThrough = 0, Keyboard = 1, Arpeggiator = 2, PatternBank = 3, GroupedTrigs = 4 };

struct GroupTarget
{
    int padIndex      = 0;
    int offsetPulses  = 0;    // clock pulses delay (0 = simultaneous)
    int velocityScale = 100;  // % of incoming velocity (1–100)
    int tuneOffset    = 0;    // semitone offset (-12 to +12)
};

struct PadConfig
{
    int     noteReceive    = 36;
    PadMode mode           = PadMode::PassThrough;

    // Keyboard mode
    int     instKey        = 36;
    int     semitoneOffset = 0;    // -12 to +12, shifts entire seqTune range
    int     kbVelocity     = 100;
    bool    retriggEnabled = false;
    int     retriggRate    = 1;    // 0=1/8, 1=1/16, 2=1/32, 3=1/64
    int     retriggMax     = 4;    // 0 = infinite, 1-16 = fixed count
    float   retriggDecay   = 0.f;
    bool    lfoEnabled     = false;
    int     lfoMode        = 0;    // 0=Loop, 1=One-shot
    int     lfoRate        = 0;    // 0..5 (1/4 to 1/128)
    int     lfoDepth       = 3;    // 0..12 semitones
    int     lfoShape       = 0;    // 0=Sine..4=Saw↓

    // Arpeggiator mode
    int  arpRate      = 1;     // 0=1/8, 1=1/16, 2=1/32, 3=1/64
    int  arpDirection = 0;     // 0=Up, 1=Down, 2=UpDown, 3=Random
    int  arpOctaves   = 1;     // 1–4
    bool arpLatch     = false;

    // Pattern Bank mode
    juce::String selectedPatternId  = {};
    int          patternResolution  = 1;    // 0=1/8, 1=1/16, 2=1/32
    int          patternOffset      = 0;

    // Delay effect (available for all modes)
    bool    delayEnabled   = false;
    int     delayRate      = 1;
    int     delayRepeats   = 3;
    float   delayDecay     = 50.f; // % velocity loss per repeat

    // Grouped Trigs mode
    std::vector<GroupTarget> groupTargets;

    bool muted         = false;
    bool overdubEnabled = false;
};

struct DelayEvent
{
    juce::int64       fireAtSample;
    juce::MidiMessage msg;
    int               padIdx = -1;
};


class DDD1HubProcessor : public juce::AudioProcessor,
                         private juce::MidiInputCallback
{
public:
    DDD1HubProcessor();
    ~DDD1HubProcessor() override;

    void prepareToPlay (double sampleRate, int) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor()     const override { return true; }
    const juce::String getName() const override { return "DDD-1 MIDI Hub"; }
    bool acceptsMidi()   const override { return true; }
    bool producesMidi()  const override { return false; }
    bool isMidiEffect()  const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int  getNumPrograms()                             override { return 1; }
    int  getCurrentProgram()                          override { return 0; }
    void setCurrentProgram (int)                      override {}
    const juce::String getProgramName (int)           override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int)   override;

    // ── Pad config ──────────────────────────────────────────────────────────
    static constexpr int numPads = 14;
    PadConfig pads[numPads];

    // ── Port management ─────────────────────────────────────────────────────
    void openMidiOutput (const juce::String& deviceId);
    void openDDD1Input  (const juce::String& deviceId);
    void openKbInput    (const juce::String& deviceId);

    juce::String ddd1InputId, kbInputId, midiOutputId;

    // ── Status / data readable by editor ─────────────────────────────────────
    bool  clockSyncActive  = false;
    bool  transportRunning = false;
    bool  captureActive    = false;
    float detectedBpm      = 120.f;

    // Global pattern position (readable by editor for playhead / recording)
    int  patternLengthBars  = 1;
    int  patternTotalSteps  = 16;
    int  globalPulseCounter = 0;
    int  currentGlobalStep  = 0;


    // Thread safety for pattern bank mutations (audio ↔ UI)
    juce::CriticalSection patternBankLock;

    // Thread safety for groupTargets (audio reads, UI writes)
    juce::CriticalSection groupLock;

    int getPatternStep (int padIdx) const { return padKb[padIdx].patternPlayingStep; }

    // Per-pad held-note bitmask — written audio thread, read UI timer (relaxed, OK for piano display)
    std::atomic<uint64_t>  heldBitsLo[numPads];   // notes 0–63
    std::atomic<uint64_t>  heldBitsHi[numPads];   // notes 64–127

    // Incremented on each DDD-1 hit (audio thread) so editor timer can flash the pad button
    std::atomic<uint32_t> padHitSeq[numPads];

    // Set by audio thread when an overdub write happens; editor timer clears and refreshes grid
    std::atomic<bool> overdubDirty[numPads];

    // Inject a MIDI message into the keyboard input path (for virtual piano / computer keyboard)
    void injectKbNote (const juce::MidiMessage& msg);

    PatternBank  patternBank;
    juce::String patternBankFilePath;
    void loadPatternBank (const juce::File& f);
    void savePatternBank ();
    void updateLivePattern (const juce::String& id, const RhythmPattern& updated);

    PatternSetBank       patternSetBank;
    juce::String         patternSetBankFilePath;
    juce::CriticalSection patternSetBankLock;
    void loadPatternSetBank (const juce::File& f);
    void savePatternSetBank ();
    void applyPatternSet (const PatternSet& s);
    void resetAllPatternAssignments ();
    PatternSet captureCurrentPatternSet () const;

    // ── Global params ────────────────────────────────────────────────────────
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;

    struct PadKbState
    {
        std::map<int, juce::uint8> heldNotes;
        std::map<int, int>         retrigCounts;
        std::array<int, 128>       activeSeqTunes;
        double retriggPhase       = 0.0;
        double lfoPhase           = 0.0;
        bool   lfoOneShotDone     = false;
        int    pulsesToNextRetrig = 0;

        // Arpeggiator
        std::vector<int> arpNotes;
        int              arpStep         = 0;
        int              arpPingDir      = 1;
        int              pulsesToNextArp = 0;
        int              arpLastNote     = -1;
        juce::uint8      arpHitVelocity  = 100;

        // Pattern Bank
        int patternCurrentStep = 0;
        int patternPlayingStep = 0;   // step index currently sounding (for editor playhead)
        int pulsesToNextStep   = 0;
        int patternLastNote    = -1;

        // Clock-domain delay echoes (fired by pulse count, not sample time)
        struct ClockEcho { int note; juce::uint8 vel; int midiCh; int pulsesLeft; };
        std::vector<ClockEcho> clockEchos;

        PadKbState() { activeSeqTunes.fill (-1); }
    };
    PadKbState padKb[numPads];

    static double      lfoValue    (double phase, int shape);
    static int         lfoOffset   (double phase, int depth, int shape, bool done);
    static juce::uint8 decayedVel  (juce::uint8 origVel, int idx, float decay);
    static double      retriggHz   (int rateIdx, double bpm);
    static double      lfoHz       (int rateIdx, double bpm);
    static int         pulsesPerRetrig (int rateIdx);
    static double      pulsesPerLFO    (int rateIdx);

    void sendInitialKbNote (int padIdx, int note, juce::uint8 vel, int t,
                            juce::MidiBuffer& out, int midiCh, float bpm);
    void fireKbRetrigger   (int padIdx, int note, juce::uint8 origVel, int t,
                            juce::MidiBuffer& out, int midiCh, float bpm);
    void scheduleDelayEchoes  (int note, juce::uint8 vel, int padIdx,
                               juce::int64 startSample, int midiCh, double bpm);
    void scheduleClockEchoes  (int note, juce::uint8 vel, int padIdx, int midiCh);

    static void advanceArpStep (PadKbState& kb, const PadConfig& cfg, int poolSize);
    static void rebuildArpPool (PadKbState& kb, const PadConfig& cfg);

    std::vector<DelayEvent>  delayQueue;
    juce::CriticalSection    delayLock;

    // (groupLock is now public)

    struct PendingGroupTrig
    {
        int     targetPadIdx;
        int     pulsesRemaining;
        juce::uint8 velocity;
        int     seqTune;
        int     midiCh;
    };
    std::vector<PendingGroupTrig> pendingGroupTrigs;

    juce::int64             totalSamples = 0;

    // Tracks when we last fired a delay echo per note (to ignore MIDI THRU bounces)
    juce::int64 delayEchoLastFired[128];

    juce::MidiBuffer      ddd1InBuf;
    juce::CriticalSection ddd1InLock;

    juce::MidiBuffer      kbInBuf;
    juce::CriticalSection kbInLock;

    int         samplesSinceLastClock = 0;
    double      currentSampleRate     = 44100.0;
    juce::int64 lastClockSampleTime   = -1;

    std::unique_ptr<juce::MidiOutput> midiOut;
    std::unique_ptr<juce::MidiInput>  ddd1In;
    std::unique_ptr<juce::MidiInput>  kbIn;
    juce::CriticalSection             midiOutLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DDD1HubProcessor)
};
