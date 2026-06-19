#pragma once
#include <JuceHeader.h>
#include "PatternBank.h"
#include "PatternSetBank.h"
#include "PadConfig.h"

struct IdeaOrigin
{
    juce::String type;         // "library" | "scratch" | "capture" | "idea"
    juce::String source;       // e.g. "allen", "bardet" — empty for scratch/capture
    juce::String parentGroup;  // library group key — for type=="library"
    juce::String parentName;   // human-readable parent name — for type=="library"
    juce::String parentIdeaId; // source Idea ID — for type=="idea" (Save As from existing Idea)
};

struct Idea
{
    juce::String id;
    juce::String name;
    juce::String createdAt;
    juce::String updatedAt;

    IdeaOrigin   origin;
    juce::String notes;

    // Pattern set: which snapshot patterns are assigned to which pads
    PatternSet patternSet;

    // Full pattern snapshots — self-contained, independent of the library.
    // IDs are scoped to this idea ("idea_<ideaId>_pad<N>") so they don't
    // collide with or depend on factory/imported library entries.
    std::vector<RhythmPattern> patterns;

    // All 14 pad configs in pad-index order (position 0 = pad 0, etc.).
    // selectedPatternId in each config points at the corresponding snapshot above.
    std::vector<PadConfig> padConfigs;
};
