#pragma once
#include <JuceHeader.h>
#include <vector>

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

    // Grouped Trigs mode — also used as overlay on PatternBank mode
    std::vector<GroupTarget> groupTargets;
    bool grpOverlay     = false;  // when true and mode==PatternBank: fan out each step hit through groupTargets

    bool muted          = false;
    bool overdubEnabled = false;
};
