#pragma once
#include <JuceHeader.h>
#include "PatternBank.h"

struct PadAssignment
{
    int          padIndex  = 0;
    juce::String patternId;
    int          resolution = 1;
    int          offset     = 0;
};

struct PatternSet
{
    juce::String               id;
    juce::String               name;
    juce::String               style;   // genre tag for filtering
    juce::String               source;  // "bardet", "badness", "roland", "allen"
    bool                       isFill = false;
    std::vector<PadAssignment> assignments;
};

class PatternSetBank
{
public:
    void load (const juce::File& jsonFile);
    void save (const juce::File& jsonFile) const;

    void add       (const PatternSet& s);
    bool overwrite (const juce::String& id, const PatternSet& s);
    bool remove    (const juce::String& id);

    const std::vector<PatternSet>& getAll() const { return sets; }
    const PatternSet* findById (const juce::String& id) const;

    // Group PatternBank entries by name prefix before " — " into auto PatternSets.
    std::vector<PatternSet> buildAutoSets (const PatternBank& bank,
                                           const int* noteReceive, int numPads) const;

private:
    std::vector<PatternSet> sets;
};
