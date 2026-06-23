#pragma once
#include <JuceHeader.h>
#include <set>

class CrateBank
{
public:
    void load    (const juce::File& f);
    void save    (const juce::File& f) const;
    bool contains (const juce::String& id) const;
    void toggle   (const juce::String& id);
    void clear    ();

private:
    std::set<juce::String> ids_;
};
