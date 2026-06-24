#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PatternBank.h"

class DDD1HubEditor : public juce::AudioProcessorEditor,
                      private juce::Timer,
                      public juce::ListBoxModel
{
public:
    explicit DDD1HubEditor (DDD1HubProcessor&);
    ~DDD1HubEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp   (const juce::MouseEvent&) override;

    // ListBoxModel — services patternListBox in top panel
    int  getNumRows () override;
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void refreshDdd1Inputs();
    void refreshMidiOutputs();
    void refreshKbInputs();
    void selectPad (int idx);
    void loadPadConfig();
    void updateVisibility();
    void rebuildPatternList();
    void rebuildPatternSourceBox();
    void rebuildStyleBox();
    void updateBottomZoneState();
    void repaintBottomZone();
    void pushEditingPatternToProcessor();
    int  hitTestGridStep (int x, int y) const;
    static juce::String noteName (int n);

    DDD1HubProcessor& proc;
    int      selectedPad  = 0;
    bool     loadingCfg   = false;
    int      padClickCount[DDD1HubProcessor::numPads]  = {};
    uint32_t padLastClickMs[DDD1HubProcessor::numPads] = {};
    int      padFlashTicks[DDD1HubProcessor::numPads]  = {};
    uint32_t padHitSeqLast[DDD1HubProcessor::numPads]  = {};
    std::vector<const RhythmPattern*> filteredPatterns;
    std::unique_ptr<juce::FileChooser> patternFileChooser;

    // ── Bottom zone ───────────────────────────────────────────────────────────
    enum class BottomZoneState { Grid, Hidden, Piano };
    BottomZoneState bottomState   = BottomZoneState::Hidden;
    RhythmPattern   editingPattern;
    bool            editingDirty  = false;
    int             dragStartVel    = 100;
    int             dragStartStep   = -1;
    bool            gridDragWasHit  = false;  // step was already hit when mouseDown fired
    bool            gridDragMoved   = false;  // mouse moved enough to count as a drag

    // Bottom zone – global settings (always visible)
    juce::Label    globalLengthLbl, globalStepsLbl;
    juce::ComboBox globalLengthBox, globalStepsBox;

    bool gridShowTune = false;

    // Bottom zone – Grid controls
    juce::Label      patternNameLbl;
    juce::TextButton gridViewBtn   { "Vel" };
    juce::TextButton saveBtn       { "Save" };
    juce::TextButton saveAsBtn     { "Save As" };
    juce::TextButton clearStepsBtn { "Clear Steps" };
    juce::TextButton undoBtn       { "Undo" };
    std::optional<RhythmPattern>   undoPattern;

    // PatternBank pad controls
    juce::TextButton clearPadBtn { "Unassign" };

    // ── Top bar ───────────────────────────────────────────────────────────────
    juce::Label    ddd1InLbl, kbInLbl, midiOutLbl, midiChLbl, bpmLbl, bpmValLbl, syncLbl;
    juce::Label    virtOutLbl, virtChLbl;
    juce::ComboBox ddd1InBox, kbInBox, midiOutBox, midiChBox, virtOutBox, virtChBox;
    juce::TextButton refreshDdd1Btn {"↺"}, refreshKbBtn {"↺"}, refreshOutBtn {"↺"}, refreshVirtOutBtn {"↺"};
    void refreshVirtualMidiOutputs();
    juce::Slider   bpmSlider;

    // ── Pad buttons ───────────────────────────────────────────────────────────
    juce::TextButton padBtns[DDD1HubProcessor::numPads];

    // ── Pad config header (always visible) ───────────────────────────────────
    juce::Label    noteRcvLbl, noteRcvVal, modeLbl;
    juce::Slider   noteRcvSlider;
    juce::ComboBox modeBox;

    // ── Keyboard mode ─────────────────────────────────────────────────────────
    juce::Label    instKeyLbl, instKeyVal, semitoneOffsetLbl, semitoneOffsetVal, rangeLbl,
                   kbVelLbl, kbVelVal;
    juce::Slider   instKeySlider, semitoneOffsetSlider, kbVelSlider;

    juce::Label        retriggHdrLbl, retriggRateLbl, retriggMaxLbl, retriggMaxVal,
                       retriggDecayLbl, retriggDecayVal;
    juce::ToggleButton retriggToggle {"Retrigger"};
    juce::ComboBox     retriggRateBox;
    juce::Slider       retriggMaxSlider, retriggDecaySlider;

    juce::Label        lfoHdrLbl, lfoModeLbl, lfoRateLbl, lfoDepthLbl, lfoShapeLbl, lfoDepthVal;
    juce::ToggleButton lfoToggle {"LFO"};
    juce::ComboBox     lfoModeBox, lfoRateBox, lfoShapeBox;
    juce::Slider       lfoDepthSlider;

    // ── Arpeggiator mode ──────────────────────────────────────────────────────
    juce::Label        arpHdrLbl, arpRateLbl, arpDirLbl, arpOctLbl, arpOctVal;
    juce::ComboBox     arpRateBox, arpDirBox;
    juce::Slider       arpOctSlider;
    juce::ToggleButton arpLatchToggle {"Latch"};

    // ── Pattern Bank mode top panel ───────────────────────────────────────────
    juce::Label        patternHdrLbl, patternInstrLbl, patternGenreLbl, patternStyleLbl,
                       patternSourceLbl, patternResLbl, patternOffsetLbl, patternOffsetVal;
    juce::ComboBox     patternInstrBox, patternGenreBox, patternStyleBox, patternSourceBox,
                       patternResBox;
    juce::ListBox      patternListBox;
    juce::Slider       patternOffsetSlider;
    juce::TextButton   patternBankLoadBtn {"Load Bank"};
    juce::ToggleButton overdubToggle    {"Overdub"};
    juce::ToggleButton grpOverlayToggle {"GroupedTrigs Overlay"};

    // ── Grouped Trigs mode ────────────────────────────────────────────────────
    struct GroupTableModel : public juce::ListBoxModel
    {
        DDD1HubEditor& owner;
        explicit GroupTableModel (DDD1HubEditor& o) : owner (o) {}
        int  getNumRows () override;
        void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool sel) override;
        void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    };
    std::unique_ptr<GroupTableModel> groupTableModel;

    juce::Label      groupHdrLbl;
    juce::ListBox    groupTable;
    juce::TextButton groupAddBtn {"+"},  groupRemoveBtn {"-"};
    juce::Label      groupPadLbl, groupPadVal, groupOffsetLbl, groupOffsetVal,
                     groupVelLbl, groupVelVal, groupTuneLbl, groupTuneVal;
    juce::Slider     groupPadSlider, groupOffsetSlider, groupVelSlider, groupTuneSlider;
    int              groupSelectedRow = -1;

    // ── Delay effect (all modes) ──────────────────────────────────────────────
    juce::ToggleButton delayToggle {"Delay"};
    juce::Label        delayRateLbl, delayRepeatsLbl, delayRepeatsVal,
                       delayDecayLbl, delayDecayVal;
    juce::ComboBox     delayRateBox;
    juce::Slider       delayRepeatsSlider, delayDecaySlider;

    // ── Pattern Sets / Scenes panel ───────────────────────────────────────────
    struct SetsListModel : public juce::ListBoxModel
    {
        DDD1HubEditor& owner;
        explicit SetsListModel (DDD1HubEditor& o) : owner (o) {}
        int  getNumRows () override;
        void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool sel) override;
        void listBoxItemClicked       (int row, const juce::MouseEvent&) override;
        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    };
    std::unique_ptr<SetsListModel> setsListModel;

    struct IdeasListModel : public juce::ListBoxModel
    {
        DDD1HubEditor& owner;
        explicit IdeasListModel (DDD1HubEditor& o) : owner (o) {}
        int  getNumRows () override;
        void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool sel) override;
        void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    };
    std::unique_ptr<IdeasListModel> ideasListModel;

    juce::Label      setsHdrLbl;
    juce::ListBox    setsListBox;
    juce::ListBox    ideasListBox;
    juce::TextButton setsSaveSceneBtn {"Save Scene"};
    juce::TextButton setsResetBtn     {"Reset All"};
    juce::TextButton setsRefreshBtn   {"\xe2\x86\xba"};
    juce::TextButton saveIdeaBtn      {"Save"};
    juce::TextButton saveAsIdeaBtn    {"Save As"};
    juce::TextButton ideasTabBtn      {"Ideas"};
    juce::Label      setsGenreLbl;
    juce::ComboBox   setsGenreBox;
    juce::Label      setsSourceLbl;
    juce::ComboBox   setsSourceBox;
    juce::TextButton setsFillBtn      {"Fill"};
    juce::TextButton setsGrooveBtn    {"Groove"};
    juce::TextButton setsAllBtn       {"All"};
    juce::TextButton setsFavBtn       {"Fav"};
    juce::TextButton setsCrateBtn     {"Crate"};
    juce::TextButton clearCrateBtn    {"\xe2\x9c\x95"}; // ✕
    juce::TextButton setsUnratedBtn   {"Unrated"};
    juce::TextButton setsSkipBtn      {"Skipped"};
    // 0 = all (non-skipped), 1 = groove only, 2 = fill only
    int              setsTypeFilter  = 0;
    // 0 = all, 1 = favorites, 2 = crate, 3 = unrated, 4 = skipped
    int              setsStateFilter = 0;
    bool             showIdeas       = false;
    juce::String     currentIdeaId;
    void rebuildGenreBoxes();
    void rebuildSourceBox();
    void rebuildIdeasList ();
    void updateScenesMode ();
    void updateIdeaButtons ();
    void renameIdea (const juce::String& id, const juce::String& currentName);
    void deleteIdea (const juce::String& id);

    // Capture toggle + Restore Setup — sit before pad buttons
    juce::TextButton captureToggleBtn {""};
    juce::TextButton restoreSetupBtn  {"rst"};
    bool captureActive = false;
    void updateCaptureToggle();

    // Global pattern shift
    juce::TextButton shiftLeftBtn  {"< Shift"};
    juce::TextButton shiftRightBtn {"Shift >"};
    void shiftAllPatterns (int direction);

    struct SceneEntry { bool isAuto; int idx; juce::String name; juce::String style; juce::String source; juce::String groupId; };
    std::vector<SceneEntry> setsEntries;
    void rebuildSetsList();
    void applySetEntry (int entryIdx);

    // ── Virtual piano (Keyboard mode bottom zone) ─────────────────────────────
    bool pianoKeyDown[128] = {};   // currently held via computer keyboard (by note)
    int  pianoMouseNote    = -1;   // note currently held by mouse (-1 = none)

    void refreshPadColors();
    void checkPianoKeys();
    void injectPianoNote (int note, bool on);
    void drawVirtualPiano (juce::Graphics& g, int x, int y, int w, int h);
    int  pianoHitTest (int mouseX, int mouseY) const;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bpmAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DDD1HubEditor)
};
