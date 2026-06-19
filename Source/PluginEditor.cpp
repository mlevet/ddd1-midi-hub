#include "PluginEditor.h"

namespace col {
    static const juce::Colour bg     { 0xff1e1e2e };
    static const juce::Colour panel  { 0xff313244 };
    static const juce::Colour accent { 0xff89b4fa };
    static const juce::Colour green  { 0xffa6e3a1 };
    static const juce::Colour text   { 0xffcdd6f4 };
    static const juce::Colour muted  { 0xff6c7086 };
    static const juce::Colour red    { 0xfff38ba8 };
    static const juce::Colour peach  { 0xfffab387 }; // Arpeggiator
    static const juce::Colour mauve  { 0xffcba6f7 }; // Pattern Bank
    static const juce::Colour teal   { 0xff94e2d5 }; // Grouped Trigs
}

juce::String DDD1HubEditor::noteName (int n)
{
    static const char* names[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    return juce::String (names[n % 12]) + juce::String (n / 12 - 1);
}

// ── Constructor ───────────────────────────────────────────────────────────────

DDD1HubEditor::DDD1HubEditor (DDD1HubProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setSize (720, 720);

    auto addLbl = [&](juce::Label& l, const juce::String& t, juce::Colour c = col::muted)
    {
        l.setText (t, juce::dontSendNotification);
        l.setFont (juce::Font (11.f));
        l.setColour (juce::Label::textColourId, c);
        addAndMakeVisible (l);
    };

    auto addValLbl = [&](juce::Label& l)
    {
        l.setFont (juce::Font (11.f));
        l.setColour (juce::Label::textColourId, col::text);
        l.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (l);
    };

    auto addHSlider = [&](juce::Slider& s, double lo, double hi, double step,
                          std::function<void()> cb)
    {
        s.setRange (lo, hi, step);
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setColour (juce::Slider::trackColourId,      col::accent);
        s.setColour (juce::Slider::thumbColourId,      col::accent);
        s.setColour (juce::Slider::backgroundColourId, col::panel);
        s.onValueChange = cb;
        addAndMakeVisible (s);
    };

    auto styleCombo = [](juce::ComboBox& cb)
    {
        cb.setColour (juce::ComboBox::backgroundColourId, col::panel);
        cb.setColour (juce::ComboBox::textColourId,       col::text);
        cb.setColour (juce::ComboBox::outlineColourId,    col::muted);
    };

    auto addCombo = [&](juce::ComboBox& cb, std::initializer_list<juce::String> items,
                        std::function<void()> cb2)
    {
        styleCombo (cb);
        int id = 1;
        for (auto& s : items) cb.addItem (s, id++);
        cb.setSelectedId (1, juce::dontSendNotification);
        cb.onChange = cb2;
        addAndMakeVisible (cb);
    };

    auto styleBtn = [&](juce::TextButton& b)
    {
        b.setColour (juce::TextButton::buttonColourId,  col::panel);
        b.setColour (juce::TextButton::textColourOffId, col::text);
        addAndMakeVisible (b);
    };

    auto addToggle = [&](juce::ToggleButton& tb, std::function<void()> cb)
    {
        tb.setColour (juce::ToggleButton::textColourId, col::text);
        tb.setColour (juce::ToggleButton::tickColourId, col::accent);
        tb.onClick = cb;
        addAndMakeVisible (tb);
    };

    // ── DDD-1 In / KB In / MIDI Out selectors ────────────────────────────────
    addLbl (ddd1InLbl,  "DDD-1 In:");
    addLbl (midiOutLbl, "MIDI Out:");

    styleCombo (ddd1InBox);
    ddd1InBox.addItem ("-- none --", 1);
    ddd1InBox.setSelectedId (1, juce::dontSendNotification);
    ddd1InBox.onChange = [this]
    {
        int id = ddd1InBox.getSelectedId();
        if (id < 2) { proc.openDDD1Input ({}); return; }
        auto devs = juce::MidiInput::getAvailableDevices();
        if (id - 2 < (int)devs.size())
            proc.openDDD1Input (devs[(size_t)(id - 2)].identifier);
    };
    addAndMakeVisible (ddd1InBox);

    styleCombo (midiOutBox);
    midiOutBox.addItem ("-- none --", 1);
    midiOutBox.setSelectedId (1, juce::dontSendNotification);
    midiOutBox.onChange = [this]
    {
        int id = midiOutBox.getSelectedId();
        if (id < 2) { proc.openMidiOutput ({}); return; }
        auto devs = juce::MidiOutput::getAvailableDevices();
        if (id - 2 < (int)devs.size())
            proc.openMidiOutput (devs[(size_t)(id - 2)].identifier);
    };
    addAndMakeVisible (midiOutBox);

    addLbl (kbInLbl, "KB In:");
    styleCombo (kbInBox);
    kbInBox.addItem ("-- none --", 1);
    kbInBox.setSelectedId (1, juce::dontSendNotification);
    kbInBox.onChange = [this]
    {
        int id = kbInBox.getSelectedId();
        if (id < 2) { proc.openKbInput ({}); return; }
        auto devs = juce::MidiInput::getAvailableDevices();
        if (id - 2 < (int)devs.size())
            proc.openKbInput (devs[(size_t)(id - 2)].identifier);
    };
    addAndMakeVisible (kbInBox);

    refreshDdd1Btn.onClick = [this] { refreshDdd1Inputs(); };
    refreshKbBtn.onClick   = [this] { refreshKbInputs(); };
    refreshOutBtn.onClick  = [this] { refreshMidiOutputs(); };
    styleBtn (refreshDdd1Btn); styleBtn (refreshKbBtn); styleBtn (refreshOutBtn);

    addLbl (midiChLbl, "MIDI Ch:");
    styleCombo (midiChBox);
    for (int i = 1; i <= 16; ++i) midiChBox.addItem (juce::String (i), i);
    midiChBox.setSelectedId (1, juce::dontSendNotification);
    midiChBox.onChange = [this]
    {
        if (auto* param = proc.apvts.getParameter ("midiChannel"))
            param->setValueNotifyingHost (
                param->convertTo0to1 ((float)midiChBox.getSelectedId()));
    };
    addAndMakeVisible (midiChBox);

    addLbl (bpmLbl, "BPM (free):");
    addValLbl (bpmValLbl);
    bpmSlider.setRange (40.0, 200.0, 0.1);
    bpmSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    bpmSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    bpmSlider.setColour (juce::Slider::trackColourId,      col::accent);
    bpmSlider.setColour (juce::Slider::thumbColourId,      col::accent);
    bpmSlider.setColour (juce::Slider::backgroundColourId, col::panel);
    addAndMakeVisible (bpmSlider);
    bpmAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        proc.apvts, "bpm", bpmSlider);

    addLbl (syncLbl, "FREE", col::muted);
    syncLbl.setFont (juce::Font (12.f));
    syncLbl.setJustificationType (juce::Justification::centredRight);

    // ── Pad buttons ───────────────────────────────────────────────────────────
    for (int i = 0; i < DDD1HubProcessor::numPads; ++i)
    {
        auto& b = padBtns[i];
        b.setButtonText (juce::String (i + 1));
        b.setColour (juce::TextButton::buttonColourId,  col::panel);
        b.setColour (juce::TextButton::textColourOffId, col::text);
        b.onClick = [this, i] { selectPad (i); };
        addAndMakeVisible (b);
    }

    // ── Pad config header ─────────────────────────────────────────────────────
    addLbl (noteRcvLbl, "Note:");
    addValLbl (noteRcvVal);
    addHSlider (noteRcvSlider, 0, 127, 1, [this]
    {
        if (loadingCfg) return;
        proc.pads[selectedPad].noteReceive = (int)noteRcvSlider.getValue();
        noteRcvVal.setText (juce::String (proc.pads[selectedPad].noteReceive),
                            juce::dontSendNotification);
    });
    addLbl (modeLbl, "Mode:");
    addCombo (modeBox, { "Pass-through", "Keyboard", "Arpeggiator", "Pattern", "Grouped Trigs" }, [this]
    {
        if (loadingCfg) return;
        proc.pads[selectedPad].mode = (PadMode)(modeBox.getSelectedId() - 1);
        updateBottomZoneState();
        updateVisibility();
        refreshPadColors();
        repaint();
    });

    // ── Keyboard mode ─────────────────────────────────────────────────────────
    addLbl (instKeyLbl, "Inst Key:");
    addValLbl (instKeyVal);
    addHSlider (instKeySlider, 25, 71, 1, [this]
    {
        if (loadingCfg) return;
        proc.pads[selectedPad].instKey = (int)instKeySlider.getValue();
        instKeyVal.setText (juce::String (proc.pads[selectedPad].instKey),
                            juce::dontSendNotification);
    });

    addLbl (kbVelLbl, "Vel:");
    addValLbl (kbVelVal);
    addHSlider (kbVelSlider, 1, 127, 1, [this]
    {
        if (loadingCfg) return;
        proc.pads[selectedPad].kbVelocity = (int)kbVelSlider.getValue();
        kbVelVal.setText (juce::String (proc.pads[selectedPad].kbVelocity),
                          juce::dontSendNotification);
    });

    addLbl (semitoneOffsetLbl, "Offset:");
    addValLbl (semitoneOffsetVal);
    addHSlider (semitoneOffsetSlider, -12, 12, 1, [this]
    {
        if (loadingCfg) return;
        int v = (int)semitoneOffsetSlider.getValue();
        proc.pads[selectedPad].semitoneOffset = v;
        semitoneOffsetVal.setText ((v >= 0 ? "+" : "") + juce::String (v),
                                   juce::dontSendNotification);
        int lo = 72 + v - 12, hi = 72 + v + 12;
        rangeLbl.setText ("Range  " + noteName (juce::jlimit (0, 127, lo)) +
                          "  \xe2\x80\x93  " + noteName (juce::jlimit (0, 127, hi)),
                          juce::dontSendNotification);
    });

    addLbl (rangeLbl, "", col::muted);
    rangeLbl.setFont (juce::Font (10.f));

    addLbl (retriggHdrLbl, "\xe2\x94\x80\xe2\x94\x80 RETRIGGER ", col::muted);
    addToggle (retriggToggle, [this]
    {
        if (loadingCfg) return;
        proc.pads[selectedPad].retriggEnabled = retriggToggle.getToggleState();
    });
    addLbl (retriggRateLbl, "Rate:");
    addCombo (retriggRateBox, { "1/8", "1/16", "1/32", "1/64" }, [this]
    {
        if (loadingCfg) return;
        proc.pads[selectedPad].retriggRate = retriggRateBox.getSelectedId() - 1;
    });
    addLbl (retriggMaxLbl, "Count:");
    addValLbl (retriggMaxVal);
    addHSlider (retriggMaxSlider, 0, 16, 1, [this]
    {
        if (loadingCfg) return;
        int v = (int)retriggMaxSlider.getValue();
        proc.pads[selectedPad].retriggMax = v;
        retriggMaxVal.setText (v == 0 ? juce::String ("\xe2\x88\x9e") : juce::String (v),
                               juce::dontSendNotification);
    });
    addLbl (retriggDecayLbl, "Decay:");
    addValLbl (retriggDecayVal);
    addHSlider (retriggDecaySlider, 0, 100, 0.1, [this]
    {
        if (loadingCfg) return;
        proc.pads[selectedPad].retriggDecay = (float)retriggDecaySlider.getValue();
        retriggDecayVal.setText (juce::String ((double)proc.pads[selectedPad].retriggDecay, 1),
                                 juce::dontSendNotification);
    });

    addLbl (lfoHdrLbl, "\xe2\x94\x80\xe2\x94\x80 LFO ", col::muted);
    addToggle (lfoToggle, [this]
    {
        if (loadingCfg) return;
        proc.pads[selectedPad].lfoEnabled = lfoToggle.getToggleState();
    });
    addLbl (lfoModeLbl,  "Mode:");
    addLbl (lfoRateLbl,  "Rate:");
    addLbl (lfoDepthLbl, "Depth:");
    addLbl (lfoShapeLbl, "Shape:");
    addValLbl (lfoDepthVal);
    addCombo (lfoModeBox, { "Loop", "One-shot" }, [this]
    {
        if (loadingCfg) return;
        proc.pads[selectedPad].lfoMode = lfoModeBox.getSelectedId() - 1;
    });
    addCombo (lfoRateBox, { "1/4", "1/8", "1/16", "1/32", "1/64", "1/128" }, [this]
    {
        if (loadingCfg) return;
        proc.pads[selectedPad].lfoRate = lfoRateBox.getSelectedId() - 1;
    });
    addCombo (lfoShapeBox, { "Sine", "Square", "Triangle", "Saw \xe2\x86\x91", "Saw \xe2\x86\x93" }, [this]
    {
        if (loadingCfg) return;
        proc.pads[selectedPad].lfoShape = lfoShapeBox.getSelectedId() - 1;
    });
    addHSlider (lfoDepthSlider, 0, 12, 1, [this]
    {
        if (loadingCfg) return;
        proc.pads[selectedPad].lfoDepth = (int)lfoDepthSlider.getValue();
        lfoDepthVal.setText (juce::String (proc.pads[selectedPad].lfoDepth),
                             juce::dontSendNotification);
    });

    // ── Arpeggiator mode ──────────────────────────────────────────────────────
    addLbl (arpHdrLbl, "\xe2\x94\x80\xe2\x94\x80 ARPEGGIATOR ", col::muted);
    addToggle (arpLatchToggle, [this]
    {
        if (loadingCfg) return;
        proc.pads[selectedPad].arpLatch = arpLatchToggle.getToggleState();
    });
    addLbl (arpRateLbl, "Rate:");
    addCombo (arpRateBox, { "1/8", "1/16", "1/32", "1/64" }, [this]
    {
        if (loadingCfg) return;
        proc.pads[selectedPad].arpRate = arpRateBox.getSelectedId() - 1;
    });
    addLbl (arpDirLbl, "Dir:");
    addCombo (arpDirBox, { "Up", "Down", "UpDown", "Random" }, [this]
    {
        if (loadingCfg) return;
        proc.pads[selectedPad].arpDirection = arpDirBox.getSelectedId() - 1;
    });
    addLbl (arpOctLbl, "Octaves:");
    addValLbl (arpOctVal);
    addHSlider (arpOctSlider, 1, 4, 1, [this]
    {
        if (loadingCfg) return;
        proc.pads[selectedPad].arpOctaves = (int)arpOctSlider.getValue();
        arpOctVal.setText (juce::String (proc.pads[selectedPad].arpOctaves),
                           juce::dontSendNotification);
    });

    // ── Pattern Bank mode top panel ───────────────────────────────────────────
    addLbl (patternHdrLbl, "\xe2\x94\x80\xe2\x94\x80 PATTERN ", col::muted);

    styleBtn (patternBankLoadBtn);
    patternBankLoadBtn.onClick = [this]
    {
        patternFileChooser = std::make_unique<juce::FileChooser> (
            "Load Pattern Bank — select folder or patterns.json...",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
            "*.json");
        patternFileChooser->launchAsync (
            juce::FileBrowserComponent::openMode
            | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::canSelectDirectories,
            [this] (const juce::FileChooser& fc)
            {
                juce::File f = fc.getResult();
                if (f != juce::File{})
                {
                    proc.loadPatternBank (f);
                    rebuildGenreBoxes();
                    rebuildSourceBox();
                    rebuildStyleBox();
                    rebuildPatternList();
                    rebuildSetsList();
                }
            });
    };

    addLbl (patternInstrLbl, "Instr:");
    addCombo (patternInstrBox,
        { "All", "kick", "snare", "rimshot", "clap",
          "closed_hihat", "open_hihat", "pedal_hihat",
          "high_tom", "mid_tom", "low_tom",
          "ride", "crash", "cowbell", "tambourine",
          "shaker", "claves", "conga", "bongo", "perc", "bass" },
        [this] { if (!loadingCfg) rebuildPatternList(); });

    addLbl (patternGenreLbl, "Genre:");
    addCombo (patternGenreBox, { "All" }, [this]
    {
        if (!loadingCfg) { rebuildStyleBox(); rebuildPatternList(); }
    });

    addLbl (patternStyleLbl, "Style:");
    addCombo (patternStyleBox, { "All" },
        [this] { if (!loadingCfg) rebuildPatternList(); });

    addLbl (patternResLbl, "Res:");
    addCombo (patternResBox, { "1/8", "1/16", "1/32" }, [this]
    {
        if (loadingCfg) return;
        proc.pads[selectedPad].patternResolution = patternResBox.getSelectedId() - 1;
    });

    patternListBox.setModel (this);
    patternListBox.setColour (juce::ListBox::backgroundColourId, col::panel);
    patternListBox.setColour (juce::ListBox::outlineColourId,    col::muted);
    patternListBox.setRowHeight (22);
    addAndMakeVisible (patternListBox);

    addLbl (patternOffsetLbl, "Offset:");
    addValLbl (patternOffsetVal);
    addHSlider (patternOffsetSlider, 0, 15, 1, [this]
    {
        if (loadingCfg) return;
        proc.pads[selectedPad].patternOffset = (int)patternOffsetSlider.getValue();
        patternOffsetVal.setText (juce::String (proc.pads[selectedPad].patternOffset),
                                  juce::dontSendNotification);
    });
    addToggle (overdubToggle, [this]
    {
        if (loadingCfg) return;
        bool on = overdubToggle.getToggleState();
        proc.pads[selectedPad].overdubEnabled = on;
        // In Keyboard mode: auto-create a live pattern to record into
        if (proc.pads[selectedPad].mode == PadMode::Keyboard && on)
        {
            juce::String liveId = "__live_" + juce::String (selectedPad) + "__";
            int numSteps = proc.patternLengthBars * 16;
            RhythmPattern live;
            live.id = liveId;
            live.steps.assign ((size_t)numSteps, PatternStep{});
            proc.pads[selectedPad].selectedPatternId = liveId;
            proc.updateLivePattern (liveId, live);
        }
    });

    // ── Grouped Trigs mode ────────────────────────────────────────────────────
    addLbl (groupHdrLbl, "Grouped Trigs");

    groupTableModel = std::make_unique<GroupTableModel> (*this);
    groupTable.setModel (groupTableModel.get());
    groupTable.setColour (juce::ListBox::backgroundColourId, col::bg);
    groupTable.setColour (juce::ListBox::outlineColourId,    col::muted);
    groupTable.setRowHeight (22);
    groupTable.setOutlineThickness (1);
    addAndMakeVisible (groupTable);

    styleBtn (groupAddBtn);
    groupAddBtn.onClick = [this]
    {
        auto& targets = proc.pads[selectedPad].groupTargets;
        {
            juce::ScopedLock lk (proc.groupLock);
            targets.push_back ({});
        }
        groupSelectedRow = (int)targets.size() - 1;
        groupTable.updateContent();
        groupTable.selectRow (groupSelectedRow);
        updateVisibility();
    };

    styleBtn (groupRemoveBtn);
    groupRemoveBtn.onClick = [this]
    {
        auto& targets = proc.pads[selectedPad].groupTargets;
        if (groupSelectedRow >= 0 && groupSelectedRow < (int)targets.size())
        {
            {
                juce::ScopedLock lk (proc.groupLock);
                targets.erase (targets.begin() + groupSelectedRow);
            }
            groupSelectedRow = juce::jlimit (-1, (int)targets.size() - 1, groupSelectedRow - 1);
            groupTable.updateContent();
            if (groupSelectedRow >= 0) groupTable.selectRow (groupSelectedRow);
            updateVisibility();
        }
    };

    addLbl (groupPadLbl,    "Pad:");
    addLbl (groupOffsetLbl, "Delay:");
    addLbl (groupVelLbl,    "Vel%:");
    addLbl (groupTuneLbl,   "Tune:");
    addValLbl (groupPadVal);
    addValLbl (groupOffsetVal);
    addValLbl (groupVelVal);
    addValLbl (groupTuneVal);

    addHSlider (groupPadSlider, 1, DDD1HubProcessor::numPads, 1, [this]
    {
        if (loadingCfg) return;
        auto& targets = proc.pads[selectedPad].groupTargets;
        if (groupSelectedRow < 0 || groupSelectedRow >= (int)targets.size()) return;
        juce::ScopedLock lk (proc.groupLock);
        targets[(size_t)groupSelectedRow].padIndex = (int)groupPadSlider.getValue() - 1;
        groupPadVal.setText (juce::String ((int)groupPadSlider.getValue()), juce::dontSendNotification);
        groupTable.repaintRow (groupSelectedRow);
    });

    addHSlider (groupOffsetSlider, 0, 96, 1, [this]
    {
        if (loadingCfg) return;
        auto& targets = proc.pads[selectedPad].groupTargets;
        if (groupSelectedRow < 0 || groupSelectedRow >= (int)targets.size()) return;
        juce::ScopedLock lk (proc.groupLock);
        targets[(size_t)groupSelectedRow].offsetPulses = (int)groupOffsetSlider.getValue();
        groupOffsetVal.setText (juce::String ((int)groupOffsetSlider.getValue()), juce::dontSendNotification);
        groupTable.repaintRow (groupSelectedRow);
    });

    addHSlider (groupVelSlider, 1, 200, 1, [this]
    {
        if (loadingCfg) return;
        auto& targets = proc.pads[selectedPad].groupTargets;
        if (groupSelectedRow < 0 || groupSelectedRow >= (int)targets.size()) return;
        juce::ScopedLock lk (proc.groupLock);
        targets[(size_t)groupSelectedRow].velocityScale = (int)groupVelSlider.getValue();
        groupVelVal.setText (juce::String ((int)groupVelSlider.getValue()) + "%", juce::dontSendNotification);
        groupTable.repaintRow (groupSelectedRow);
    });

    addHSlider (groupTuneSlider, -12, 12, 1, [this]
    {
        if (loadingCfg) return;
        auto& targets = proc.pads[selectedPad].groupTargets;
        if (groupSelectedRow < 0 || groupSelectedRow >= (int)targets.size()) return;
        juce::ScopedLock lk (proc.groupLock);
        int v = (int)groupTuneSlider.getValue();
        targets[(size_t)groupSelectedRow].tuneOffset = v;
        juce::String ts = v == 0 ? "0" : (v > 0 ? "+" + juce::String (v) : juce::String (v));
        groupTuneVal.setText (ts, juce::dontSendNotification);
        groupTable.repaintRow (groupSelectedRow);
    });

    // ── Delay effect (all modes) ─────────────────────────────────────────────
    addToggle (delayToggle, [this]
    {
        if (loadingCfg) return;
        proc.pads[selectedPad].delayEnabled = delayToggle.getToggleState();
        updateVisibility();
    });
    addLbl (delayRateLbl,    "Rate:");
    addLbl (delayRepeatsLbl, "Repeats:");
    addLbl (delayDecayLbl,   "Decay %:");
    addValLbl (delayRepeatsVal);
    addValLbl (delayDecayVal);
    addCombo (delayRateBox, { "1/8", "1/16", "1/32", "1/64" }, [this]
    {
        if (loadingCfg) return;
        proc.pads[selectedPad].delayRate = delayRateBox.getSelectedId() - 1;
    });
    addHSlider (delayRepeatsSlider, 1, 16, 1, [this]
    {
        if (loadingCfg) return;
        proc.pads[selectedPad].delayRepeats = (int)delayRepeatsSlider.getValue();
        delayRepeatsVal.setText (juce::String (proc.pads[selectedPad].delayRepeats),
                                 juce::dontSendNotification);
    });
    addHSlider (delayDecaySlider, 0, 100, 1, [this]
    {
        if (loadingCfg) return;
        proc.pads[selectedPad].delayDecay = (float)delayDecaySlider.getValue();
        delayDecayVal.setText (juce::String ((double)proc.pads[selectedPad].delayDecay, 0),
                               juce::dontSendNotification);
    });

    // ── Pattern Sets / Scenes panel ───────────────────────────────────────────
    addLbl (setsHdrLbl, "Scenes");

    setsListModel = std::make_unique<SetsListModel> (*this);
    setsListBox.setModel (setsListModel.get());
    setsListBox.setColour (juce::ListBox::backgroundColourId, col::bg);
    setsListBox.setColour (juce::ListBox::outlineColourId,    col::muted);
    setsListBox.setRowHeight (20);
    setsListBox.setOutlineThickness (1);
    addAndMakeVisible (setsListBox);

    addLbl (setsGenreLbl, "Genre:");
    addCombo (setsGenreBox, { "All" }, [this] { rebuildSetsList(); });

    addLbl (setsSourceLbl, "Source:");
    addCombo (setsSourceBox, { "All" }, [this] { rebuildSetsList(); });

    styleBtn (setsFillBtn);
    styleBtn (setsGrooveBtn);
    addAndMakeVisible (setsFillBtn);
    addAndMakeVisible (setsGrooveBtn);
    setsFillBtn.onClick = [this] {
        setsTypeFilter = (setsTypeFilter == 2) ? 0 : 2;
        if (setsTypeFilter == 2) setsGrooveBtn.setToggleState (false, juce::dontSendNotification);
        setsFillBtn.setToggleState   (setsTypeFilter == 2, juce::dontSendNotification);
        rebuildSetsList();
    };
    setsGrooveBtn.onClick = [this] {
        setsTypeFilter = (setsTypeFilter == 1) ? 0 : 1;
        if (setsTypeFilter == 1) setsFillBtn.setToggleState (false, juce::dontSendNotification);
        setsGrooveBtn.setToggleState (setsTypeFilter == 1, juce::dontSendNotification);
        rebuildSetsList();
    };

    styleBtn (setsSaveSceneBtn);
    setsSaveSceneBtn.onClick = [this]
    {
        auto scene = proc.captureCurrentPatternSet();
        if (scene.assignments.empty())
        {
            juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                "No patterns assigned",
                "Assign patterns to pads before saving a scene.");
            return;
        }
        auto* aw = new juce::AlertWindow ("Save Scene", "Scene name:", juce::AlertWindow::NoIcon);
        aw->addTextEditor ("name", "New Scene");
        aw->addButton ("Save",   1);
        aw->addButton ("Cancel", 0);
        struct Cb : public juce::ModalComponentManager::Callback
        {
            DDD1HubEditor& ed;
            juce::AlertWindow* win;
            PatternSet sc;
            Cb (DDD1HubEditor& e, juce::AlertWindow* w, PatternSet s)
                : ed (e), win (w), sc (std::move (s)) {}
            void modalStateFinished (int result) override
            {
                if (result == 1)
                {
                    sc.name = win->getTextEditorContents ("name");
                    {
                        juce::ScopedLock lk (ed.proc.patternSetBankLock);
                        ed.proc.patternSetBank.add (sc);
                    }
                    ed.proc.savePatternSetBank();
                    ed.rebuildSetsList();
                }
                delete win;
            }
        };
        aw->enterModalState (true, new Cb (*this, aw, std::move (scene)), false);
    };

    styleBtn (setsResetBtn);
    setsResetBtn.onClick = [this]
    {
        proc.resetAllPatternAssignments();
        loadPadConfig();
        repaint();
    };

    styleBtn (setsRefreshBtn);
    setsRefreshBtn.onClick = [this] { rebuildSetsList(); };

    styleBtn (captureToggleBtn);
    captureToggleBtn.onClick = [this]
    {
        captureActive = !captureActive;
        proc.captureActive = captureActive;
        if (captureActive)
        {
            // Steps = bars × 16 (1/16 grid) so 4 bars → 64 steps, no early overwrite
            int numSteps = proc.patternLengthBars * 16;
            proc.patternTotalSteps = numSteps;
            // Sync steps combobox (1→16, 2→32, 3→64)
            int stepsId = (numSteps <= 16) ? 1 : (numSteps <= 32) ? 2 : 3;
            globalStepsBox.setSelectedId (stepsId, juce::dontSendNotification);

            for (int i = 0; i < DDD1HubProcessor::numPads; ++i)
            {
                proc.pads[i].mode              = PadMode::PatternBank;
                proc.pads[i].overdubEnabled    = true;
                proc.pads[i].patternResolution = 1;  // force 1/16 to match numSteps calculation
                juce::String liveId = "__live_" + juce::String (i) + "__";
                RhythmPattern live;
                live.id    = liveId;
                live.steps.assign ((size_t)numSteps, PatternStep{});
                proc.pads[i].selectedPatternId = liveId;
                proc.updateLivePattern (liveId, live);
            }
        }
        else
        {
            // Stop capture: keep PatternBank mode + patterns, just stop overdub and start playback
            for (int i = 0; i < DDD1HubProcessor::numPads; ++i)
                proc.pads[i].overdubEnabled = false;
        }
        updateCaptureToggle();
        loadPadConfig();
        refreshPadColors();
        repaint();
    };

    rebuildGenreBoxes();
    rebuildSourceBox();
    rebuildStyleBox();
    rebuildSetsList();

    // ── Bottom zone ───────────────────────────────────────────────────────────

    addLbl (globalLengthLbl, "Length:");
    addCombo (globalLengthBox, { "1 bar", "2 bars", "4 bars" }, [this]
    {
        const int bars[] = { 1, 2, 4 };
        proc.patternLengthBars = bars[globalLengthBox.getSelectedId() - 1];
    });
    // Sync combobox to restored processor value (setStateInformation runs before editor is created)
    globalLengthBox.setSelectedId (proc.patternLengthBars == 4 ? 3 :
                                   proc.patternLengthBars == 2 ? 2 : 1,
                                   juce::dontSendNotification);

    addLbl (globalStepsLbl, "Steps:");
    addCombo (globalStepsBox, { "16", "32", "64" }, [this]
    {
        const int opts[] = { 16, 32, 64 };
        proc.patternTotalSteps = opts[globalStepsBox.getSelectedId() - 1];
    });
    globalStepsBox.setSelectedId (proc.patternTotalSteps >= 64 ? 3 :
                                  proc.patternTotalSteps >= 32 ? 2 : 1,
                                  juce::dontSendNotification);

    addLbl (patternNameLbl, "", col::text);
    patternNameLbl.setFont (juce::Font (12.f));

    gridViewBtn.setColour (juce::TextButton::buttonColourId,  col::accent.withAlpha (0.3f));
    gridViewBtn.setColour (juce::TextButton::textColourOffId, col::accent);
    gridViewBtn.onClick = [this]
    {
        gridShowTune = !gridShowTune;
        gridViewBtn.setButtonText (gridShowTune ? "Tune" : "Vel");
        gridViewBtn.setColour (juce::TextButton::buttonColourId,
                               gridShowTune ? col::green.withAlpha (0.3f) : col::accent.withAlpha (0.3f));
        gridViewBtn.setColour (juce::TextButton::textColourOffId,
                               gridShowTune ? col::green : col::accent);
        repaintBottomZone();
    };
    addAndMakeVisible (gridViewBtn);

    styleBtn (saveBtn);
    styleBtn (saveAsBtn);
    styleBtn (clearStepsBtn);
    styleBtn (clearPadBtn);
    styleBtn (undoBtn);
    addAndMakeVisible (saveBtn);
    addAndMakeVisible (saveAsBtn);
    addAndMakeVisible (clearStepsBtn);
    addAndMakeVisible (clearPadBtn);
    addAndMakeVisible (undoBtn);

    saveBtn.onClick = [this]
    {
        if (editingPattern.id.isEmpty()) return;
        pushEditingPatternToProcessor();
        proc.savePatternBank();
        editingDirty = false;
        updateVisibility();
    };

    saveAsBtn.onClick = [this]
    {
        auto* w = new juce::AlertWindow ("Save Pattern As", {},
                                         juce::AlertWindow::NoIcon, this);

        w->addTextEditor ("name",
                          editingPattern.name.isNotEmpty() ? editingPattern.name : "New Pattern",
                          "Name:");

        const juce::StringArray instrItems { "kick", "snare", "rimshot", "clap",
                                              "closed_hihat", "open_hihat", "pedal_hihat",
                                              "high_tom", "mid_tom", "low_tom",
                                              "ride", "crash", "cowbell", "tambourine",
                                              "shaker", "claves", "conga", "bongo", "perc", "bass" };
        w->addComboBox ("instr", instrItems, "Instrument:");
        if (auto* cb = w->getComboBoxComponent ("instr"))
        {
            // Use pattern's own instrument if set (existing pattern); else default to kick
            juce::String preInstr = editingPattern.instrument.isNotEmpty()
                                    ? editingPattern.instrument : "kick";
            int idx = instrItems.indexOf (preInstr);
            cb->setSelectedItemIndex (idx >= 0 ? idx : 0, juce::dontSendNotification);
        }

        const juce::StringArray styleItems { "(none)", "dub", "reggae", "trap",
                                             "house", "techno", "funk", "afro" };
        w->addComboBox ("style", styleItems, "Style:");
        if (auto* cb = w->getComboBoxComponent ("style"))
        {
            juce::String first = editingPattern.styles.size() > 0
                                 ? editingPattern.styles[0] : "";
            int idx = styleItems.indexOf (first);
            cb->setSelectedItemIndex (idx >= 0 ? idx : 0, juce::dontSendNotification);
        }

        w->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        w->enterModalState (true,
            juce::ModalCallbackFunction::create ([this, w] (int result)
            {
                if (result != 1) return;
                juce::String newName = w->getTextEditorContents ("name").trim();
                if (newName.isEmpty()) return;

                juce::String instr = "kick";
                if (auto* cb = w->getComboBoxComponent ("instr"))
                    instr = cb->getText();

                juce::String style;
                if (auto* cb = w->getComboBoxComponent ("style"))
                    if (cb->getSelectedItemIndex() > 0)
                        style = cb->getText();

                RhythmPattern newPat = editingPattern;
                newPat.name       = newName;
                newPat.instrument = instr;
                newPat.styles.clear();
                if (style.isNotEmpty()) newPat.styles.add (style);
                newPat.id = newName.toLowerCase().replaceCharacters (" ", "_") + "_"
                          + juce::String (juce::Random::getSystemRandom().nextInt (9999));

                {
                    juce::ScopedLock lk (proc.patternBankLock);
                    proc.patternBank.insert (newPat);
                }
                proc.pads[selectedPad].selectedPatternId = newPat.id;
                editingPattern = newPat;
                editingDirty   = false;
                proc.savePatternBank();
                rebuildPatternList();
                updateVisibility();
            }),
            true);
    };

    clearStepsBtn.onClick = [this]
    {
        if (editingPattern.id.isEmpty()) return;
        undoPattern = editingPattern;
        for (auto& s : editingPattern.steps) { s.hit = false; s.velocity = 100; s.tune = 0; }
        editingDirty = true;
        pushEditingPatternToProcessor();
        proc.savePatternBank();
        repaintBottomZone();
        updateVisibility();
    };

    undoBtn.onClick = [this]
    {
        if (!undoPattern.has_value()) return;
        editingPattern = undoPattern.value();
        undoPattern.reset();
        editingDirty = true;
        pushEditingPatternToProcessor();
        proc.savePatternBank();
        repaintBottomZone();
        updateVisibility();
    };

    clearPadBtn.onClick = [this]
    {
        proc.pads[selectedPad].selectedPatternId = {};
        editingPattern = {};
        editingDirty   = false;
        undoPattern.reset();
        updateBottomZoneState();
        updateVisibility();
        repaintBottomZone();
    };

    refreshDdd1Inputs();
    refreshKbInputs();
    refreshMidiOutputs();
    selectPad (0);
    startTimerHz (15);
}

DDD1HubEditor::~DDD1HubEditor() { stopTimer(); }

// ── MIDI port refresh ─────────────────────────────────────────────────────────

static void populateMidiInBox (juce::ComboBox& box, const juce::String& currentId)
{
    box.clear (juce::dontSendNotification);
    box.addItem ("-- none --", 1);
    auto devs = juce::MidiInput::getAvailableDevices();
    for (int i = 0; i < (int)devs.size(); ++i)
    {
        box.addItem (devs[(size_t)i].name, i + 2);
        if (devs[(size_t)i].identifier == currentId)
            box.setSelectedId (i + 2, juce::dontSendNotification);
    }
    if (box.getSelectedId() == 0)
        box.setSelectedId (1, juce::dontSendNotification);
}

void DDD1HubEditor::refreshDdd1Inputs() { populateMidiInBox (ddd1InBox, proc.ddd1InputId); }
void DDD1HubEditor::refreshKbInputs()   { populateMidiInBox (kbInBox,   proc.kbInputId);   }

void DDD1HubEditor::refreshMidiOutputs()
{
    midiOutBox.clear (juce::dontSendNotification);
    midiOutBox.addItem ("-- none --", 1);
    auto devs = juce::MidiOutput::getAvailableDevices();
    for (int i = 0; i < (int)devs.size(); ++i)
    {
        midiOutBox.addItem (devs[(size_t)i].name, i + 2);
        if (devs[(size_t)i].identifier == proc.midiOutputId)
            midiOutBox.setSelectedId (i + 2, juce::dontSendNotification);
    }
    if (midiOutBox.getSelectedId() == 0)
        midiOutBox.setSelectedId (1, juce::dontSendNotification);
}

// ── ListBoxModel (top panel pattern list) ─────────────────────────────────────

int DDD1HubEditor::getNumRows() { return (int)filteredPatterns.size(); }

void DDD1HubEditor::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (selected) g.fillAll (col::accent.withAlpha (0.3f));
    if (row < 0 || row >= (int)filteredPatterns.size()) return;
    const auto* pat = filteredPatterns[(size_t)row];
    juce::String styles = pat->styles.joinIntoString (", ");
    juce::String txt = pat->name + "  [" + pat->instrument
                     + (styles.isNotEmpty() ? " / " + styles : "") + "]";
    g.setColour (col::text);
    g.setFont (juce::Font (11.f));
    g.drawText (txt, 6, 0, w - 6, h, juce::Justification::centredLeft, true);
}

void DDD1HubEditor::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    if (row < 0 || row >= (int)filteredPatterns.size()) return;
    const RhythmPattern* pat = filteredPatterns[(size_t)row];
    proc.pads[selectedPad].selectedPatternId = pat->id;
    editingPattern = *pat;
    editingDirty   = false;
    updateBottomZoneState();
    updateVisibility();
    repaintBottomZone();
}

void DDD1HubEditor::rebuildSourceBox()
{
    juce::String cur = setsSourceBox.getText();
    setsSourceBox.clear (juce::dontSendNotification);
    setsSourceBox.addItem ("All", 1);
    juce::StringArray sources;
    {
        juce::ScopedLock lk (proc.patternBankLock);
        sources = proc.patternBank.getSources();
    }
    int id = 2;
    for (const auto& s : sources) setsSourceBox.addItem (s, id++);
    bool found = false;
    for (int i = 0; i < setsSourceBox.getNumItems(); ++i)
        if (setsSourceBox.getItemText (i) == cur) { setsSourceBox.setSelectedItemIndex (i, juce::dontSendNotification); found = true; break; }
    if (!found) setsSourceBox.setSelectedItemIndex (0, juce::dontSendNotification);
}

void DDD1HubEditor::rebuildGenreBoxes()
{
    juce::StringArray genres;
    {
        juce::ScopedLock lk (proc.patternBankLock);
        genres = proc.patternBank.getGenres();
    }
    genres.sort (false);

    auto repopulate = [&](juce::ComboBox& box)
    {
        juce::String cur = box.getText();
        box.clear (juce::dontSendNotification);
        box.addItem ("All", 1);
        int id = 2;
        for (const auto& g : genres) box.addItem (g, id++);
        // Restore previous selection
        bool found = false;
        for (int i = 0; i < box.getNumItems(); ++i)
            if (box.getItemText (i) == cur) { box.setSelectedItemIndex (i, juce::dontSendNotification); found = true; break; }
        if (!found) box.setSelectedItemIndex (0, juce::dontSendNotification);
    };

    repopulate (patternGenreBox);
    repopulate (setsGenreBox);
}

void DDD1HubEditor::rebuildStyleBox()
{
    // Populate specific styles filtered by selected genre
    juce::String selectedGenre = patternGenreBox.getSelectedId() > 1 ? patternGenreBox.getText() : "";
    juce::String current = patternStyleBox.getText();
    patternStyleBox.clear (juce::dontSendNotification);
    patternStyleBox.addItem ("All", 1);

    juce::StringArray styles;
    {
        juce::ScopedLock lk (proc.patternBankLock);
        for (const auto& pat : proc.patternBank.getAll())
        {
            if (selectedGenre.isNotEmpty() && (pat.styles.size() == 0 || pat.styles[0] != selectedGenre))
                continue;
            for (int i = 1; i < pat.styles.size(); ++i)  // styles[1] = specific style
                if (!styles.contains (pat.styles[i])) styles.add (pat.styles[i]);
        }
    }
    styles.sort (false);

    int id = 2;
    for (const auto& s : styles) patternStyleBox.addItem (s, id++);

    bool found = false;
    for (int i = 0; i < patternStyleBox.getNumItems(); ++i)
        if (patternStyleBox.getItemText (i) == current) { patternStyleBox.setSelectedItemIndex (i, juce::dontSendNotification); found = true; break; }
    if (!found) patternStyleBox.setSelectedItemIndex (0, juce::dontSendNotification);
}

void DDD1HubEditor::rebuildPatternList()
{
    juce::String instr = patternInstrBox.getSelectedId() > 1
                       ? patternInstrBox.getText() : "All";
    juce::String genre = patternGenreBox.getSelectedId() > 1
                       ? patternGenreBox.getText() : "All";
    juce::String style = patternStyleBox.getSelectedId() > 1
                       ? patternStyleBox.getText() : "All";
    {
        juce::ScopedLock lk (proc.patternBankLock);
        filteredPatterns = proc.patternBank.filter (instr, genre, style);
    }
    patternListBox.updateContent();

    const juce::String& id = proc.pads[selectedPad].selectedPatternId;
    bool found = false;
    if (id.isNotEmpty())
    {
        for (int r = 0; r < (int)filteredPatterns.size(); ++r)
            if (filteredPatterns[(size_t)r]->id == id)
            { patternListBox.selectRow (r, false, true); found = true; break; }
    }
    if (!found)
        patternListBox.deselectAllRows();
}

// ── Bottom zone state machine ─────────────────────────────────────────────────

void DDD1HubEditor::updateBottomZoneState()
{

    PadMode m = proc.pads[selectedPad].mode;
    if (m == PadMode::PatternBank)
    {
        const juce::String& id = proc.pads[selectedPad].selectedPatternId;
        if (id.isNotEmpty())
        {
            const RhythmPattern* pat = nullptr;
            {
                juce::ScopedLock lk (proc.patternBankLock);
                pat = proc.patternBank.findById (id);
            }
            if (pat)
            {
                if (editingPattern.id != id)
                {
                    editingPattern = *pat;
                    editingDirty   = false;
                }
                patternNameLbl.setText (editingPattern.name, juce::dontSendNotification);
                bottomState = BottomZoneState::Grid;
                return;
            }
        }

        // No pattern selected yet: show blank drawable grid.
        // Reset unless editingPattern already belongs to this pad's live slot.
        juce::String myLiveId = "__live_" + juce::String (selectedPad) + "__";
        if (editingPattern.id != myLiveId && editingPattern.id.isNotEmpty())
        {
            editingPattern = RhythmPattern{};
            editingDirty   = false;
        }
        int numSteps = proc.patternTotalSteps;
        if ((int)editingPattern.steps.size() != numSteps)
            editingPattern.steps.assign ((size_t)numSteps, PatternStep{});
        patternNameLbl.setText ("unsaved \xe2\x80\x94 use Save As to keep it",
                                juce::dontSendNotification);
        // Push immediately so the processor has a slot and the playhead animates
        pushEditingPatternToProcessor();
        bottomState = BottomZoneState::Grid;
        return;
    }
    if (m == PadMode::Keyboard)
    {
        bottomState = BottomZoneState::Piano;
        return;
    }
    bottomState = BottomZoneState::Hidden;
}

void DDD1HubEditor::repaintBottomZone()
{
    repaint (0, 500, getWidth(), getHeight() - 500);
}

// ── Virtual piano ─────────────────────────────────────────────────────────────

// 2-octave piano: C4 (60) to B5 (83)
static constexpr int kPianoBase    = 60;
static constexpr int kNumWhiteKeys = 15;  // C4 to C6

// White MIDI notes (indices 0-14)
static const int kWhiteNotes[kNumWhiteKeys] = {
    60,62,64,65,67,69,71,  // C4 D4 E4 F4 G4 A4 B4
    72,74,76,77,79,81,83,  // C5 D5 E5 F5 G5 A5 B5
    84                     // C6
};

// Black keys: {midiNote, leftWhiteKeyIndex}  (10 black keys, unchanged)
static const int kBlackNotes[10] = { 61,63,66,68,70, 73,75,78,80,82 };
static const int kBlackAfter[10] = {  0, 1, 3, 4, 5,  7, 8,10,11,12 };

// Computer-keyboard → MIDI note mapping (AZERTY layout)
// Physical-key equivalents of the standard QWERTY piano layout:
//   QWERTY A→AZERTY Q,  QWERTY W→AZERTY Z,  QWERTY ;→AZERTY M
//   S D E F T G Y H U J K O L P unchanged
// Middle row = white keys, top row = black keys, bottom row = upper range
// Range shifted to D4–F5 so M lands on the highest DDD-1 note (F5)
// C4 and C#4 are mouse-only at the low end; everything above F5 is mouse-only
static const std::pair<int,int> kKeyMap[] = {
    // D4–B4 (middle row white, top row black — E and U unused: no black between E-F and B-C)
    { 'q', 62 }, { 'z', 63 }, { 's', 64 },
    { 'd', 65 }, { 'r', 66 }, { 'f', 67 }, { 't', 68 }, { 'g', 69 }, { 'y', 70 }, { 'h', 71 },
    // C5–F5 (middle row white, top row black — P unused: no black between E-F)
    { 'j', 72 }, { 'i', 73 }, { 'k', 74 }, { 'o', 75 }, { 'l', 76 }, { 'm', 77 }
};
static const char* kKeyLabel[kNumWhiteKeys] = {
    "" ,"Q","S","D","F","G","H",  // C4(mouse only), D4–B4
    "J","K","L","M","","","",""   // C5–F5, G5–C6 mouse only
};
static const char* kBlackLabel[10] = { "","Z","R","T","Y", "I","O","","","" };

void DDD1HubEditor::checkPianoKeys()
{
    bool isKb = (proc.pads[selectedPad].mode == PadMode::Keyboard);
    for (auto& [key, note] : kKeyMap)
    {
        bool down = isKb && juce::KeyPress::isKeyCurrentlyDown (key);
        if (down != pianoKeyDown[note])
        {
            pianoKeyDown[note] = down;
            injectPianoNote (note, down);
        }
    }
    // Release mouse note if mode changed away
    if (!isKb && pianoMouseNote >= 0)
    {
        injectPianoNote (pianoMouseNote, false);
        pianoMouseNote = -1;
    }
}

void DDD1HubEditor::injectPianoNote (int note, bool on)
{
    auto msg = on ? juce::MidiMessage::noteOn  (1, note, (juce::uint8) 100)
                  : juce::MidiMessage::noteOff (1, note);
    proc.injectKbNote (msg);
}

int DDD1HubEditor::pianoHitTest (int mx, int my) const
{
    const int bz = 500;
    const int px = 16, py = bz + 8;
    const int pw = getWidth() - 32, ph = getHeight() - bz - 16;
    if (mx < px || mx >= px + pw || my < py || my >= py + ph) return -1;

    const int keyW   = pw / kNumWhiteKeys;
    const int blackW = keyW * 60 / 100;
    const int blackH = ph * 60 / 100;

    // Black keys are visually on top — check them first
    if (my < py + blackH)
    {
        for (int i = 0; i < 10; ++i)
        {
            int bx = px + (kBlackAfter[i] + 1) * keyW - blackW / 2;
            if (mx >= bx && mx < bx + blackW)
                return kBlackNotes[i];
        }
    }
    // White key
    int idx = (mx - px) / keyW;
    if (idx >= 0 && idx < kNumWhiteKeys) return kWhiteNotes[idx];
    return -1;
}

void DDD1HubEditor::drawVirtualPiano (juce::Graphics& g, int px, int py, int pw, int ph)
{
    const int keyW   = pw / kNumWhiteKeys;
    const int blackW = keyW * 60 / 100;
    const int blackH = ph * 60 / 100;

    auto isHeld = [&](int note) -> bool {
        if (note < 64) return (proc.heldBitsLo[selectedPad].load (std::memory_order_relaxed) >> note) & 1;
        return (proc.heldBitsHi[selectedPad].load (std::memory_order_relaxed) >> (note - 64)) & 1;
    };

    // White keys
    for (int i = 0; i < kNumWhiteKeys; ++i)
    {
        int note = kWhiteNotes[i];
        bool held = isHeld (note);
        int kx = px + i * keyW;

        g.setColour (held ? col::accent.withAlpha (0.85f) : juce::Colour (0xfff0f0f0));
        g.fillRect  (kx, py, keyW - 1, ph);
        g.setColour (col::muted.withAlpha (0.5f));
        g.drawRect  (kx, py, keyW - 1, ph, 1);

        // C label
        if (note == 60 || note == 72)
        {
            g.setFont (juce::Font (9.f));
            g.setColour (held ? col::bg : col::muted);
            g.drawText (note == 60 ? "C4" : "C5", kx + 2, py + ph - 14, keyW - 4, 12,
                        juce::Justification::centred);
        }
        // Computer key hint
        if (kKeyLabel[i][0] != '\0')
        {
            g.setFont  (juce::Font (8.f));
            g.setColour (held ? col::bg.withAlpha (0.8f) : col::muted.withAlpha (0.7f));
            g.drawText (kKeyLabel[i], kx, py + ph - 28, keyW - 1, 12,
                        juce::Justification::centred);
        }
    }

    // Black keys (drawn on top)
    for (int i = 0; i < 10; ++i)
    {
        int note = kBlackNotes[i];
        bool held = isHeld (note);
        int bx = px + (kBlackAfter[i] + 1) * keyW - blackW / 2;

        g.setColour (held ? col::accent : juce::Colour (0xff222222));
        g.fillRect  (bx, py, blackW, blackH);
        g.setColour (col::muted.withAlpha (0.4f));
        g.drawRect  (bx, py, blackW, blackH, 1);

        if (kBlackLabel[i][0] != '\0')
        {
            g.setFont  (juce::Font (8.f));
            g.setColour (held ? col::bg.withAlpha (0.8f) : col::muted.withAlpha (0.6f));
            g.drawText (kBlackLabel[i], bx, py + blackH - 14, blackW, 12,
                        juce::Justification::centred);
        }
    }
}

void DDD1HubEditor::pushEditingPatternToProcessor()
{
    // Assign a per-pad live slot ID if this pattern isn't in the bank yet
    if (editingPattern.id.isEmpty())
    {
        editingPattern.id = "__live_" + juce::String (selectedPad) + "__";
        proc.pads[selectedPad].selectedPatternId = editingPattern.id;
    }
    proc.updateLivePattern (editingPattern.id, editingPattern);
}

// ── Grid hit test ─────────────────────────────────────────────────────────────

int DDD1HubEditor::hitTestGridStep (int x, int y) const
{
    const int bz    = 500;
    const int gridY = bz + 54;
    const int gridH = 96;
    if (y < gridY || y >= gridY + gridH) return -1;
    int numSteps = (int)editingPattern.steps.size();
    if (numSteps == 0) return -1;
    const int cellW = (getWidth() - 32) / numSteps;
    int step = (x - 16) / cellW;
    if (step < 0 || step >= numSteps) return -1;
    return step;
}

// ── Mouse interaction for grid editing ───────────────────────────────────────

void DDD1HubEditor::mouseDown (const juce::MouseEvent& e)
{
    if (bottomState == BottomZoneState::Piano)
    {
        int note = pianoHitTest (e.x, e.y);
        if (note >= 0)
        {
            if (pianoMouseNote >= 0 && pianoMouseNote != note)
                injectPianoNote (pianoMouseNote, false);
            pianoMouseNote = note;
            injectPianoNote (note, true);
        }
        return;
    }
    if (bottomState != BottomZoneState::Grid) return;
    if ((int)editingPattern.steps.size() < 16) return;

    int step = hitTestGridStep (e.x, e.y);
    if (step < 0) return;

    auto& s = editingPattern.steps[(size_t)step];
    dragStartStep = step;

    s.hit = !s.hit;
    if (s.hit && s.velocity == 0) s.velocity = 100;
    dragStartVel = gridShowTune ? s.tune : (int)s.velocity;
    editingDirty = true;
    pushEditingPatternToProcessor();
    repaintBottomZone();
}

void DDD1HubEditor::mouseUp (const juce::MouseEvent&)
{
    if (bottomState == BottomZoneState::Piano && pianoMouseNote >= 0)
    {
        injectPianoNote (pianoMouseNote, false);
        pianoMouseNote = -1;
    }
}

void DDD1HubEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (bottomState == BottomZoneState::Piano)
    {
        int note = pianoHitTest (e.x, e.y);
        if (note >= 0 && note != pianoMouseNote)
        {
            if (pianoMouseNote >= 0) injectPianoNote (pianoMouseNote, false);
            pianoMouseNote = note;
            injectPianoNote (note, true);
        }
        return;
    }
    if (bottomState != BottomZoneState::Grid) return;
    if (dragStartStep < 0 || dragStartStep >= (int)editingPattern.steps.size()) return;

    auto& s = editingPattern.steps[(size_t)dragStartStep];
    if (!s.hit) return;

    int delta = -e.getDistanceFromDragStartY() / 2;

    if (gridShowTune)
        s.tune     = juce::jlimit (-12, 12, dragStartVel + delta);
    else
        s.velocity = (juce::uint8)juce::jlimit (1, 127, dragStartVel + delta);

    editingDirty = true;
    pushEditingPatternToProcessor();
    repaintBottomZone();
}

// ── Pad selection & config ────────────────────────────────────────────────────

void DDD1HubEditor::updateCaptureToggle()
{
    captureToggleBtn.setColour (juce::TextButton::buttonColourId, captureActive ? col::red : col::panel);
}

void DDD1HubEditor::refreshPadColors()
{
    for (int i = 0; i < DDD1HubProcessor::numPads; ++i)
    {
        bool lit = (i == selectedPad) || (padFlashTicks[i] > 0);
        juce::Colour base;
        switch (proc.pads[i].mode)
        {
            case PadMode::Keyboard:     base = col::green.withAlpha (lit ? 1.0f : 0.70f); break;
            case PadMode::Arpeggiator:  base = col::peach.withAlpha (lit ? 1.0f : 0.70f); break;
            case PadMode::PatternBank:
                base = (proc.pads[i].overdubEnabled ? col::red : col::mauve).withAlpha (lit ? 1.0f : 0.70f);
                break;
            case PadMode::GroupedTrigs: base = col::teal .withAlpha (lit ? 1.0f : 0.70f); break;
            default:                    base = lit ? col::accent.withAlpha (0.35f) : col::panel; break;
        }
        padBtns[i].setColour (juce::TextButton::buttonColourId, base);

        padBtns[i].setButtonText (proc.pads[i].muted ? "M" : juce::String (i + 1));
    }
}

void DDD1HubEditor::selectPad (int idx)
{
    const uint32_t now     = juce::Time::getMillisecondCounter();
    const uint32_t elapsed = now - padLastClickMs[idx];
    padLastClickMs[idx]    = now;

    // Reset count if this is a different pad or too slow (> 400 ms since last click on same pad)
    if (idx != selectedPad || elapsed > 400)
        padClickCount[idx] = 0;

    ++padClickCount[idx];
    selectedPad = idx;

    if (padClickCount[idx] == 2)
    {
        // Double-click: toggle mute
        proc.pads[idx].muted = !proc.pads[idx].muted;
        refreshPadColors();
    }
    else if (padClickCount[idx] >= 3)
    {
        // Triple-click: full reset
        proc.pads[idx].mode         = PadMode::PassThrough;
        proc.pads[idx].delayEnabled = false;
        proc.pads[idx].muted        = false;
        {
            juce::ScopedLock lk (proc.groupLock);
            proc.pads[idx].groupTargets.clear();
        }
        padClickCount[idx] = 0;
        loadPadConfig();
    }
    else
    {
        // Single click: just select
        loadPadConfig();
    }
}

// ── SetsListModel ─────────────────────────────────────────────────────────────

int DDD1HubEditor::SetsListModel::getNumRows()
{
    return (int)owner.setsEntries.size();
}

void DDD1HubEditor::SetsListModel::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool sel)
{
    if (row < 0 || row >= (int)owner.setsEntries.size()) return;
    const auto& e = owner.setsEntries[(size_t)row];

    if (sel)            g.fillAll (col::accent.withAlpha (0.3f));
    else if (row % 2)   g.fillAll (col::bg);
    else                g.fillAll (col::panel);

    g.setFont (12.f);
    if (e.isAuto)
    {
        // right side: source tag
        g.setColour (col::muted);
        g.drawText (e.source, w - 90, 0, 82, h, juce::Justification::centredRight, true);
        g.setColour (col::text);
        g.drawText (e.name, 8, 0, w - 100, h, juce::Justification::centredLeft, true);
    }
    else
    {
        g.setColour (col::green);
        g.drawText ("\xe2\x98\x85", 4, 0, 14, h, juce::Justification::centred);
        g.setColour (col::text);
        g.drawText (e.name, 20, 0, w - 28, h, juce::Justification::centredLeft, true);
    }
}

void DDD1HubEditor::SetsListModel::listBoxItemDoubleClicked (int, const juce::MouseEvent&) {}

void DDD1HubEditor::SetsListModel::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    owner.applySetEntry (row);
}

void DDD1HubEditor::rebuildSetsList()
{
    juce::String genre  = setsGenreBox.getSelectedId()  > 1 ? setsGenreBox.getText()  : "";
    juce::String source = setsSourceBox.getSelectedId() > 1 ? setsSourceBox.getText() : "";

    setsEntries.clear();

    // Saved scenes always shown (no filter)
    {
        juce::ScopedLock lk (proc.patternSetBankLock);
        const auto& all = proc.patternSetBank.getAll();
        for (int i = 0; i < (int)all.size(); ++i)
            setsEntries.push_back ({ false, i, all[(size_t)i].name, all[(size_t)i].style });
    }

    // Auto-sets filtered by genre, source, fill/groove
    int noteReceive[DDD1HubProcessor::numPads];
    for (int i = 0; i < DDD1HubProcessor::numPads; ++i)
        noteReceive[i] = proc.pads[i].noteReceive;
    auto autoSets = proc.patternSetBank.buildAutoSets (proc.patternBank, noteReceive, DDD1HubProcessor::numPads);
    int autoBase = (int)proc.patternSetBank.getAll().size();
    for (int i = 0; i < (int)autoSets.size(); ++i)
    {
        const auto& s = autoSets[(size_t)i];
        if (!genre.isEmpty()  && s.style  != genre)  continue;
        if (!source.isEmpty() && s.source != source)  continue;
        if (setsTypeFilter == 1 &&  s.isFill) continue; // groove only
        if (setsTypeFilter == 2 && !s.isFill) continue; // fill only
        setsEntries.push_back ({ true, autoBase + i, s.name, s.style, s.source });
    }

    setsListBox.updateContent();
    setsListBox.repaint();
}

void DDD1HubEditor::applySetEntry (int idx)
{
    if (idx < 0 || idx >= (int)setsEntries.size()) return;
    const auto& e = setsEntries[(size_t)idx];

    if (!e.isAuto)
    {
        juce::ScopedLock lk (proc.patternSetBankLock);
        const auto& all = proc.patternSetBank.getAll();
        if (e.idx < (int)all.size())
            proc.applyPatternSet (all[(size_t)e.idx]);
    }
    else
    {
        int noteReceive[DDD1HubProcessor::numPads];
        for (int i = 0; i < DDD1HubProcessor::numPads; ++i)
            noteReceive[i] = proc.pads[i].noteReceive;
        auto autoSets = proc.patternSetBank.buildAutoSets (proc.patternBank, noteReceive, DDD1HubProcessor::numPads);
        int localIdx = e.idx - (int)proc.patternSetBank.getAll().size();
        if (localIdx >= 0 && localIdx < (int)autoSets.size())
            proc.applyPatternSet (autoSets[(size_t)localIdx]);
    }

    loadPadConfig();
    refreshPadColors();
    repaint();
}

// ── GroupTableModel ───────────────────────────────────────────────────────────

int DDD1HubEditor::GroupTableModel::getNumRows()
{
    return (int)owner.proc.pads[owner.selectedPad].groupTargets.size();
}

void DDD1HubEditor::GroupTableModel::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool sel)
{
    const auto& targets = owner.proc.pads[owner.selectedPad].groupTargets;
    if (row < 0 || row >= (int)targets.size()) return;
    const auto& tgt = targets[(size_t)row];

    if (sel)            g.fillAll (col::accent.withAlpha (0.3f));
    else if (row % 2)   g.fillAll (col::bg);
    else                g.fillAll (col::panel);

    g.setColour (col::text);
    g.setFont (12.f);
    juce::String ofs = tgt.offsetPulses == 0 ? "0" : juce::String (tgt.offsetPulses) + "p";
    juce::String tun = tgt.tuneOffset == 0 ? "0" : (tgt.tuneOffset > 0 ? "+" : "") + juce::String (tgt.tuneOffset);
    juce::String line = "Pad " + juce::String (tgt.padIndex + 1)
                      + "   delay " + ofs
                      + "   vel " + juce::String (tgt.velocityScale) + "%"
                      + "   tune " + tun;
    g.drawText (line, 8, 0, w - 16, h, juce::Justification::centredLeft);
}

void DDD1HubEditor::GroupTableModel::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    owner.groupSelectedRow = row;
    const auto& targets = owner.proc.pads[owner.selectedPad].groupTargets;
    if (row >= 0 && row < (int)targets.size())
    {
        const auto& tgt = targets[(size_t)row];
        owner.groupPadSlider.setValue (tgt.padIndex + 1, juce::dontSendNotification);
        owner.groupPadVal.setText (juce::String (tgt.padIndex + 1), juce::dontSendNotification);
        owner.groupOffsetSlider.setValue (tgt.offsetPulses, juce::dontSendNotification);
        owner.groupOffsetVal.setText (juce::String (tgt.offsetPulses), juce::dontSendNotification);
        owner.groupVelSlider.setValue (tgt.velocityScale, juce::dontSendNotification);
        owner.groupVelVal.setText (juce::String (tgt.velocityScale) + "%", juce::dontSendNotification);
        owner.groupTuneSlider.setValue (tgt.tuneOffset, juce::dontSendNotification);
        juce::String ts = tgt.tuneOffset == 0 ? "0" : (tgt.tuneOffset > 0 ? "+" : "") + juce::String (tgt.tuneOffset);
        owner.groupTuneVal.setText (ts, juce::dontSendNotification);
    }
    owner.updateVisibility();
}

// ─────────────────────────────────────────────────────────────────────────────

void DDD1HubEditor::loadPadConfig()
{
    loadingCfg = true;
    const auto& cfg = proc.pads[selectedPad];

    noteRcvSlider.setValue (cfg.noteReceive, juce::dontSendNotification);
    noteRcvVal.setText (juce::String (cfg.noteReceive), juce::dontSendNotification);
    modeBox.setSelectedId ((int)cfg.mode + 1, juce::dontSendNotification);

    instKeySlider.setValue (cfg.instKey, juce::dontSendNotification);
    instKeyVal.setText (juce::String (cfg.instKey), juce::dontSendNotification);
    kbVelSlider.setValue (cfg.kbVelocity, juce::dontSendNotification);
    kbVelVal.setText (juce::String (cfg.kbVelocity), juce::dontSendNotification);
    semitoneOffsetSlider.setValue (cfg.semitoneOffset, juce::dontSendNotification);
    int v = cfg.semitoneOffset;
    semitoneOffsetVal.setText ((v >= 0 ? "+" : "") + juce::String (v),
                               juce::dontSendNotification);
    int lo = 72 + v - 12, hi = 72 + v + 12;
    rangeLbl.setText ("Range  " + noteName (juce::jlimit (0, 127, lo)) +
                      "  \xe2\x80\x93  " + noteName (juce::jlimit (0, 127, hi)),
                      juce::dontSendNotification);

    retriggToggle.setToggleState (cfg.retriggEnabled, juce::dontSendNotification);
    retriggRateBox.setSelectedId (cfg.retriggRate + 1, juce::dontSendNotification);
    retriggMaxSlider.setValue (cfg.retriggMax, juce::dontSendNotification);
    retriggMaxVal.setText (cfg.retriggMax == 0 ? juce::String ("\xe2\x88\x9e")
                                               : juce::String (cfg.retriggMax),
                           juce::dontSendNotification);
    retriggDecaySlider.setValue (cfg.retriggDecay, juce::dontSendNotification);
    retriggDecayVal.setText (juce::String ((double)cfg.retriggDecay, 1), juce::dontSendNotification);

    lfoToggle.setToggleState (cfg.lfoEnabled, juce::dontSendNotification);
    lfoModeBox.setSelectedId (cfg.lfoMode + 1, juce::dontSendNotification);
    lfoRateBox.setSelectedId (cfg.lfoRate + 1, juce::dontSendNotification);
    lfoDepthSlider.setValue (cfg.lfoDepth, juce::dontSendNotification);
    lfoDepthVal.setText (juce::String (cfg.lfoDepth), juce::dontSendNotification);
    lfoShapeBox.setSelectedId (cfg.lfoShape + 1, juce::dontSendNotification);

    arpRateBox.setSelectedId (cfg.arpRate + 1, juce::dontSendNotification);
    arpDirBox.setSelectedId (cfg.arpDirection + 1, juce::dontSendNotification);
    arpOctSlider.setValue (cfg.arpOctaves, juce::dontSendNotification);
    arpOctVal.setText (juce::String (cfg.arpOctaves), juce::dontSendNotification);
    arpLatchToggle.setToggleState (cfg.arpLatch, juce::dontSendNotification);

    patternResBox.setSelectedId (cfg.patternResolution + 1, juce::dontSendNotification);
    patternOffsetSlider.setValue (cfg.patternOffset, juce::dontSendNotification);
    patternOffsetVal.setText (juce::String (cfg.patternOffset), juce::dontSendNotification);
    overdubToggle.setToggleState     (cfg.overdubEnabled,   juce::dontSendNotification);

    delayToggle.setToggleState (cfg.delayEnabled, juce::dontSendNotification);
    delayRateBox.setSelectedId (cfg.delayRate + 1, juce::dontSendNotification);
    delayRepeatsSlider.setValue (cfg.delayRepeats, juce::dontSendNotification);
    delayRepeatsVal.setText (juce::String (cfg.delayRepeats), juce::dontSendNotification);
    delayDecaySlider.setValue (cfg.delayDecay, juce::dontSendNotification);
    delayDecayVal.setText (juce::String ((double)cfg.delayDecay, 0), juce::dontSendNotification);

    groupSelectedRow = -1;
    groupTable.updateContent();
    groupTable.deselectAllRows();

    loadingCfg = false;

    updateBottomZoneState();
    rebuildPatternList();
    updateVisibility();
    refreshPadColors();
    repaint();
}

void DDD1HubEditor::updateVisibility()
{
    PadMode m      = proc.pads[selectedPad].mode;
    bool isKb      = (m == PadMode::Keyboard);
    bool isArp     = (m == PadMode::Arpeggiator);
    bool isPat     = (m == PadMode::PatternBank);
    bool isGrp     = (m == PadMode::GroupedTrigs);
    bool needsKey  = isKb || isArp || isPat;

    instKeyLbl.setVisible (needsKey);    instKeySlider.setVisible (needsKey);
    instKeyVal.setVisible (needsKey);
    kbVelLbl.setVisible (isKb);  kbVelSlider.setVisible (isKb);  kbVelVal.setVisible (isKb);

    semitoneOffsetLbl.setVisible (isKb);   semitoneOffsetSlider.setVisible (isKb);
    semitoneOffsetVal.setVisible (isKb);   rangeLbl.setVisible (isKb);
    retriggHdrLbl.setVisible (isKb);  retriggToggle.setVisible (isKb);
    retriggRateLbl.setVisible (isKb); retriggRateBox.setVisible (isKb);
    retriggMaxLbl.setVisible (isKb);  retriggMaxSlider.setVisible (isKb);
    retriggMaxVal.setVisible (isKb);  retriggDecayLbl.setVisible (isKb);
    retriggDecaySlider.setVisible (isKb); retriggDecayVal.setVisible (isKb);
    lfoHdrLbl.setVisible (isKb);     lfoToggle.setVisible (isKb);
    lfoModeLbl.setVisible (isKb);    lfoModeBox.setVisible (isKb);
    lfoRateLbl.setVisible (isKb);    lfoRateBox.setVisible (isKb);
    lfoDepthLbl.setVisible (isKb);   lfoDepthSlider.setVisible (isKb);
    lfoDepthVal.setVisible (isKb);   lfoShapeLbl.setVisible (isKb);
    lfoShapeBox.setVisible (isKb);

    // Delay toggle always visible; detail controls only when enabled
    bool dly = proc.pads[selectedPad].delayEnabled;
    delayToggle.setVisible (true);
    delayRateLbl.setVisible (dly);       delayRateBox.setVisible (dly);
    delayRepeatsLbl.setVisible (dly);    delayRepeatsSlider.setVisible (dly);
    delayRepeatsVal.setVisible (dly);    delayDecayLbl.setVisible (dly);
    delayDecaySlider.setVisible (dly);   delayDecayVal.setVisible (dly);

    arpHdrLbl.setVisible (isArp);     arpLatchToggle.setVisible (isArp);
    arpRateLbl.setVisible (isArp);    arpRateBox.setVisible (isArp);
    arpDirLbl.setVisible (isArp);     arpDirBox.setVisible (isArp);
    arpOctLbl.setVisible (isArp);     arpOctSlider.setVisible (isArp);
    arpOctVal.setVisible (isArp);

    patternHdrLbl.setVisible (isPat);
    patternInstrLbl.setVisible (isPat);     patternInstrBox.setVisible (isPat);
    patternGenreLbl.setVisible (isPat);     patternGenreBox.setVisible (isPat);
    patternStyleLbl.setVisible (isPat);     patternStyleBox.setVisible (isPat);
    patternResLbl.setVisible (isPat);       patternResBox.setVisible (isPat);
    patternListBox.setVisible (isPat);
    patternOffsetLbl.setVisible (isPat);    patternOffsetSlider.setVisible (isPat);
    patternOffsetVal.setVisible (isPat);
    overdubToggle.setVisible (isPat || isKb);

    bool grpRowSel = isGrp && groupSelectedRow >= 0
                     && groupSelectedRow < (int)proc.pads[selectedPad].groupTargets.size();
    groupHdrLbl.setVisible (isGrp);
    groupTable.setVisible (isGrp);
    groupAddBtn.setVisible (isGrp);
    groupRemoveBtn.setVisible (isGrp && !proc.pads[selectedPad].groupTargets.empty());
    groupPadLbl.setVisible (grpRowSel);    groupPadSlider.setVisible (grpRowSel);    groupPadVal.setVisible (grpRowSel);
    groupOffsetLbl.setVisible (grpRowSel); groupOffsetSlider.setVisible (grpRowSel); groupOffsetVal.setVisible (grpRowSel);
    groupVelLbl.setVisible (grpRowSel);    groupVelSlider.setVisible (grpRowSel);    groupVelVal.setVisible (grpRowSel);
    groupTuneLbl.setVisible (grpRowSel);   groupTuneSlider.setVisible (grpRowSel);   groupTuneVal.setVisible (grpRowSel);

    // Bottom zone
    bool hasGrid   = (bottomState == BottomZoneState::Grid);
    bool showZone  = hasGrid;

    globalLengthLbl.setVisible (true); globalLengthBox.setVisible (true);
    globalStepsLbl.setVisible  (true); globalStepsBox.setVisible  (true);
    patternNameLbl.setVisible  (showZone);
    gridViewBtn.setVisible    (hasGrid);
    saveBtn.setVisible        (hasGrid && editingDirty);
    saveAsBtn.setVisible      (hasGrid);
    clearStepsBtn.setVisible  (hasGrid && !editingPattern.id.isEmpty());
    undoBtn.setVisible        (hasGrid && undoPattern.has_value());
    clearPadBtn.setVisible    (isPat);
}

// ── Timer ─────────────────────────────────────────────────────────────────────

void DDD1HubEditor::timerCallback()
{
    if (proc.clockSyncActive)
    {
        syncLbl.setText ("SYNC \xe2\x97\x8f " + juce::String (proc.detectedBpm, 1),
                         juce::dontSendNotification);
        syncLbl.setColour (juce::Label::textColourId, col::green);
        bpmSlider.setEnabled (false);

        if (bottomState == BottomZoneState::Grid)
            repaintBottomZone();
    }
    else
    {
        syncLbl.setText ("FREE", juce::dontSendNotification);
        syncLbl.setColour (juce::Label::textColourId, col::muted);
        bpmSlider.setEnabled (true);
    }
    bpmValLbl.setText (juce::String ((double)bpmSlider.getValue(), 1), juce::dontSendNotification);

    // Overdub: refresh grid when audio thread has written a new hit
    if (proc.pads[selectedPad].mode == PadMode::PatternBank &&
        proc.overdubDirty[selectedPad].exchange (false, std::memory_order_relaxed))
    {
        juce::ScopedLock lk (proc.patternBankLock);
        const RhythmPattern* pat = proc.patternBank.findById (proc.pads[selectedPad].selectedPatternId);
        if (pat) { editingPattern = *pat; repaintBottomZone(); }
    }

    // Pad hit flash — detect new hits from audio thread, tick down existing flashes
    bool needColorRefresh = false;
    for (int i = 0; i < DDD1HubProcessor::numPads; ++i)
    {
        uint32_t seq = proc.padHitSeq[i].load (std::memory_order_relaxed);
        if (seq != padHitSeqLast[i])
        {
            padHitSeqLast[i] = seq;
            padFlashTicks[i] = 3;   // ~150 ms at 50 ms timer interval
            needColorRefresh = true;
        }
        else if (padFlashTicks[i] > 0)
        {
            --padFlashTicks[i];
            needColorRefresh = true;
        }
    }
    if (needColorRefresh)
        refreshPadColors();

    checkPianoKeys();
    if (bottomState == BottomZoneState::Piano)
        repaintBottomZone();

}

// ── Paint ─────────────────────────────────────────────────────────────────────

void DDD1HubEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);

    // Top config panel
    g.setColour (col::panel);
    g.fillRoundedRectangle (8.f, 58.f, (float)getWidth() - 16.f, 40.f, 5.f);
    g.fillRoundedRectangle (8.f, 104.f, (float)getWidth() - 16.f, 388.f, 5.f);

    g.setColour (col::muted.withAlpha (0.3f));
    g.drawHorizontalLine (132, 16.f, (float)getWidth() - 16.f);

    g.setFont (juce::Font (10.f));
    g.setColour (col::accent);
    g.drawText ("PAD " + juce::String (selectedPad + 1),
                16, 108, 50, 22, juce::Justification::centredLeft);

    // Bottom zone background (always drawn)
    const int bz = 500;
    g.setColour (col::panel);
    g.fillRoundedRectangle (8.f, (float)(bz - 4), (float)getWidth() - 16.f,
                            (float)(getHeight() - bz), 5.f);

    bool showGrid  = (bottomState == BottomZoneState::Grid);

    if (!showGrid)
    {
        if (bottomState == BottomZoneState::Piano)
        {
            drawVirtualPiano (g, 16, bz + 8, getWidth() - 32, getHeight() - bz - 16);
            return;
        }
        g.setFont (juce::Font (11.f));
        g.setColour (col::muted);
        g.drawText ("Set this pad to Pattern mode to use the pattern editor",
                    16, bz + 10, getWidth() - 32, 30, juce::Justification::centredLeft);
        return;
    }

    // ── Grid drawing ──────────────────────────────────────────────────────────
    int numSteps = (int)editingPattern.steps.size();
    if (numSteps == 0) return;

    const int gridY  = bz + 54;
    const int cellW  = (getWidth() - 32) / numSteps;
    const int cellH  = 96;
    const int barMax = cellH - 16;

    int playheadStep = proc.transportRunning ? proc.getPatternStep (selectedPad) : -1;

    for (int i = 0; i < numSteps; ++i)
    {
        int x = 16 + i * cellW;
        bool isPlay = (playheadStep == i);

        // Cell background
        g.setColour (isPlay ? col::accent.withAlpha (0.18f) : col::bg);
        g.fillRect (x, gridY, cellW - 2, cellH);

        // Step number
        g.setFont (juce::Font (9.f));
        g.setColour (isPlay ? col::accent : col::muted);
        g.drawText (juce::String (i + 1), x + 2, gridY + 1, cellW - 4, 10,
                    juce::Justification::centredLeft);

        const auto& step = editingPattern.steps[(size_t)i];

        if (step.hit)
        {
            if (gridShowTune)
            {
                // ── Tune view: green bar, height = pitch offset ───────────
                // -12 → tiny bar (4px),  0 → half,  +12 → full
                int barH = 4 + (int)((float)(step.tune + 12) / 24.f * (barMax - 4));
                g.setColour (isPlay ? col::green : col::green.withAlpha (0.8f));
                g.fillRect (x + 2, gridY + cellH - barH, cellW - 6, barH);

                // Tune value label
                g.setFont (juce::Font (8.f));
                g.setColour (col::green.withAlpha (0.9f));
                juce::String tv = (step.tune > 0 ? "+" : "") + juce::String (step.tune);
                g.drawText (tv, x + 2, gridY + 1, cellW - 4, 10,
                            juce::Justification::centredRight);
            }
            else
            {
                // ── Velocity view: blue bar, height = velocity ────────────
                if (step.velocity > 0)
                {
                    int barH = 4 + (int)((float)step.velocity / 127.f * (barMax - 4));
                    g.setColour (isPlay ? col::accent : col::accent.withAlpha (0.75f));
                    g.fillRect (x + 2, gridY + cellH - barH, cellW - 6, barH);
                }
                // velocity==0: hit is true but bar invisible (trig present but silent)

                // Show tune hint dot if tune is non-zero
                if (step.tune != 0)
                {
                    g.setColour (col::green.withAlpha (0.7f));
                    g.fillEllipse ((float)(x + cellW - 7), (float)(gridY + 3), 4.f, 4.f);
                }
            }
        }
        else
        {
            // Empty step
            g.setColour (col::muted.withAlpha (0.3f));
            g.drawRect (x + 2, gridY + 14, cellW - 6, cellH - 18, 1);
        }

        // Cell divider
        g.setColour (col::muted.withAlpha (0.15f));
        g.drawVerticalLine (x + cellW - 2, (float)gridY, (float)(gridY + cellH));
    }

    // Grid border
    g.setColour (col::muted.withAlpha (0.4f));
    g.drawRect (16, gridY, cellW * numSteps, cellH, 1);

}

// ── Layout ────────────────────────────────────────────────────────────────────

void DDD1HubEditor::resized()
{
    const int W = getWidth();
    const int M = 16;

    // Row 1
    int y = 6;
    ddd1InLbl.setBounds      (M,        y, 52, 22);
    ddd1InBox.setBounds      (M + 54,   y, 164, 22);
    refreshDdd1Btn.setBounds (M + 221,  y, 20, 22);
    kbInLbl.setBounds        (M + 248,  y, 40, 22);
    kbInBox.setBounds        (M + 290,  y, 164, 22);
    refreshKbBtn.setBounds   (M + 457,  y, 20, 22);
    midiOutLbl.setBounds     (M + 484,  y, 52, 22);
    midiOutBox.setBounds     (M + 538,  y, 140, 22);
    refreshOutBtn.setBounds  (M + 681,  y, 20, 22);
    syncLbl.setBounds        (W - M - 60, y, 60, 22);

    // Row 2
    y = 32;
    midiChLbl.setBounds  (M, y, 48, 22);
    midiChBox.setBounds  (M + 50, y, 56, 22);
    bpmLbl.setBounds     (M + 118, y, 68, 22);
    bpmSlider.setBounds  (M + 188, y, 160, 22);
    bpmValLbl.setBounds  (M + 352, y, 44, 22);

    // Pad buttons — one extra slot at the left for the capture toggle
    y = 60;
    const int padW = (W - 2 * M) / (DDD1HubProcessor::numPads + 1);
    captureToggleBtn.setBounds (M, y, padW - 2, 36);
    for (int i = 0; i < DDD1HubProcessor::numPads; ++i)
        padBtns[i].setBounds (M + (i + 1) * padW, y, padW - 2, 36);

    // Config header row
    y = 106;
    noteRcvLbl.setBounds    (M + 56, y, 38, 22);
    noteRcvSlider.setBounds (M + 96, y, 80, 22);
    noteRcvVal.setBounds    (M + 180, y, 28, 22);
    modeLbl.setBounds       (M + 218, y, 38, 22);
    modeBox.setBounds       (M + 258, y, 140, 22);

    // ── y=138: mode-specific header rows (exclusive per mode) ────────────────
    // Arpeggiator header
    arpHdrLbl.setBounds      (M, 138, 200, 22);
    arpLatchToggle.setBounds (M + 210, 138, 80, 22);
    // Pattern Bank header
    patternHdrLbl.setBounds      (M, 138, 200, 22);
    // Grouped Trigs header + buttons
    groupHdrLbl.setBounds    (M, 138, 160, 22);
    groupAddBtn.setBounds    (M + 168, 138, 28, 22);
    groupRemoveBtn.setBounds (M + 200, 138, 28, 22);

    // ── y=160: shared instKey + mode-specific extras ──────────────────────────
    // instKey is visible for KB / Arp / PatternBank modes
    instKeyLbl.setBounds    (M, 160, 52, 22);
    instKeySlider.setBounds (M + 54, 160, 90, 22);
    instKeyVal.setBounds    (M + 148, 160, 28, 22);

    // Keyboard: semitoneOffset on same row as instKey
    semitoneOffsetLbl.setBounds    (M + 186, 160, 46, 22);
    semitoneOffsetSlider.setBounds (M + 234, 160, 120, 22);
    semitoneOffsetVal.setBounds    (M + 358, 160, 36, 22);

    // Arpeggiator: rate/dir/oct on same row as instKey
    arpRateLbl.setBounds   (M + 186, 160, 30, 22);
    arpRateBox.setBounds   (M + 218, 160, 70, 22);
    arpDirLbl.setBounds    (M + 298, 160, 28, 22);
    arpDirBox.setBounds    (M + 328, 160, 90, 22);
    arpOctLbl.setBounds    (M + 428, 160, 44, 22);
    arpOctSlider.setBounds (M + 474, 160, 70, 22);
    arpOctVal.setBounds    (M + 548, 160, 24, 22);

    // Pattern Bank: instrument + genre + style filters on same row as instKey
    patternInstrLbl.setBounds  (M + 186, 160, 38,  22);
    patternInstrBox.setBounds  (M + 226, 160, 110, 22);
    patternGenreLbl.setBounds  (M + 346, 160, 38,  22);
    patternGenreBox.setBounds  (M + 386, 160, 110, 22);
    patternStyleLbl.setBounds  (M + 504, 160, 34,  22);
    patternStyleBox.setBounds  (M + 540, 160, 110, 22);

    // Delay: always visible — row 1 (toggle + rate + repeats)
    delayToggle.setBounds        (M,        350, 60,  22);
    delayRateLbl.setBounds       (M + 68,   350, 30,  22);
    delayRateBox.setBounds       (M + 100,  350, 70,  22);
    delayRepeatsLbl.setBounds    (M + 180,  350, 54,  22);
    delayRepeatsSlider.setBounds (M + 236,  350, 100, 22);
    delayRepeatsVal.setBounds    (M + 340,  350, 28,  22);

    // ── y=184: per-mode second content row ───────────────────────────────────
    // Keyboard: range label + velocity
    rangeLbl.setBounds (M, 184, 160, 18);
    kbVelLbl.setBounds    (M + 170, 184, 20, 22);
    kbVelSlider.setBounds (M + 192, 184, 100, 22);
    kbVelVal.setBounds    (M + 296, 184, 28, 22);

    // Pattern Bank: resolution + offset
    patternResLbl.setBounds      (M, 184, 28, 22);
    patternResBox.setBounds      (M + 30, 184, 70, 22);
    patternOffsetLbl.setBounds   (M + 110, 184, 44, 22);
    patternOffsetSlider.setBounds(M + 156, 184, 100, 22);
    patternOffsetVal.setBounds   (M + 260, 184, 28, 22);
    overdubToggle.setBounds      (M + 362, 184, 80, 22);

    // Delay: row 2 (decay)
    delayDecayLbl.setBounds    (M,       374, 52,  22);
    delayDecaySlider.setBounds (M + 54,  374, 160, 22);
    delayDecayVal.setBounds    (M + 218, 374, 40,  22);

    // Pattern Bank: list (y=210, height=130 → ends at y=340)
    patternListBox.setBounds (M, 210, W - 2 * M, 130);

    // Grouped Trigs: table (y=164, height=130 → ends at y=294) + row editor (y=298)
    groupTable.setBounds (M, 164, W - 2 * M, 130);
    groupPadLbl.setBounds       (M,        298, 32, 22);
    groupPadSlider.setBounds    (M +  34,  298, 60, 22);
    groupPadVal.setBounds       (M +  98,  298, 28, 22);
    groupOffsetLbl.setBounds    (M + 136,  298, 44, 22);
    groupOffsetSlider.setBounds (M + 182,  298, 70, 22);
    groupOffsetVal.setBounds    (M + 256,  298, 32, 22);
    groupVelLbl.setBounds       (M + 298,  298, 32, 22);
    groupVelSlider.setBounds    (M + 332,  298, 70, 22);
    groupVelVal.setBounds       (M + 406,  298, 40, 22);
    groupTuneLbl.setBounds      (M + 456,  298, 38, 22);
    groupTuneSlider.setBounds   (M + 496,  298, 80, 22);
    groupTuneVal.setBounds      (M + 580,  298, 36, 22);

    // ── Keyboard-only controls (y=206+) ───────────────────────────────────────
    retriggHdrLbl.setBounds      (M, 206, 240, 16);
    retriggToggle.setBounds      (M, 226, 82, 22);
    retriggRateLbl.setBounds     (M + 90,  226, 30, 22);
    retriggRateBox.setBounds     (M + 122, 226, 66, 22);
    retriggMaxLbl.setBounds      (M + 196, 226, 38, 22);
    retriggMaxSlider.setBounds   (M + 236, 226, 70, 22);
    retriggMaxVal.setBounds      (M + 310, 226, 24, 22);
    retriggDecayLbl.setBounds    (M + 342, 226, 40, 22);
    retriggDecaySlider.setBounds (M + 384, 226, 90, 22);
    retriggDecayVal.setBounds    (M + 478, 226, 36, 22);
    lfoHdrLbl.setBounds   (M, 254, 240, 16);
    lfoToggle.setBounds   (M, 274, 50, 22);
    lfoModeLbl.setBounds  (M + 58, 274, 34, 22);
    lfoModeBox.setBounds  (M + 94, 274, 80, 22);
    lfoRateLbl.setBounds  (M + 184, 274, 30, 22);
    lfoRateBox.setBounds  (M + 216, 274, 86, 22);
    lfoDepthLbl.setBounds    (M, 300, 40, 22);
    lfoDepthSlider.setBounds (M + 42, 300, 110, 22);
    lfoDepthVal.setBounds    (M + 156, 300, 28, 22);
    lfoShapeLbl.setBounds    (M + 196, 300, 42, 22);
    lfoShapeBox.setBounds    (M + 240, 300, 100, 22);

    // ── Pattern Sets / Scenes panel (y=398–490) ──────────────────────────────
    setsHdrLbl.setBounds      (M,        398, 60, 20);
    setsRefreshBtn.setBounds  (M + 64,   398, 22, 20);
    setsSaveSceneBtn.setBounds(M + 94,   398, 84, 20);
    setsResetBtn.setBounds    (M + 184,  398, 72, 20);
    patternBankLoadBtn.setBounds (M + 268, 398, 90,  20);
    // Row 1: header + save/reset/load
    // Row 2 (y=420): genre + source + fill/groove toggles
    setsGenreLbl.setBounds    (M,        420, 36,  20);
    setsGenreBox.setBounds    (M + 38,   420, 120, 20);
    setsSourceLbl.setBounds   (M + 168,  420, 44,  20);
    setsSourceBox.setBounds   (M + 214,  420, 90,  20);
    setsFillBtn.setBounds     (M + 314,  420, 44,  20);
    setsGrooveBtn.setBounds   (M + 362,  420, 58,  20);
    setsListBox.setBounds     (M,        444, W - 2 * M, 50);

    // ── Bottom zone ───────────────────────────────────────────────────────────
    const int bz = 500;
    globalLengthLbl.setBounds (M,       bz + 4, 52, 22);
    globalLengthBox.setBounds (M + 54,  bz + 4, 78, 22);
    globalStepsLbl.setBounds  (M + 142, bz + 4, 44, 22);
    globalStepsBox.setBounds  (M + 188, bz + 4, 60, 22);
    gridViewBtn.setBounds   (M + 260,  bz + 4, 50, 22);
    clearStepsBtn.setBounds (M + 320,  bz + 4, 50, 22);
    undoBtn.setBounds       (M + 378,  bz + 4, 50, 22);
    saveBtn.setBounds       (W - 130,  bz + 4, 56, 22);
    saveAsBtn.setBounds     (W - 68,   bz + 4, 52, 22);
    patternNameLbl.setBounds (M, bz + 30, W - 2 * M - 220, 20);

    // PatternBank pad clear — row with overdubToggle
    clearPadBtn.setBounds (M + 450, 184, 80, 22);
}
