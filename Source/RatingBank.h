#pragma once
#include <JuceHeader.h>
#include <map>

struct Rating
{
    int  stars  = 0;    // 0=unrated, 1–5
    bool hidden = false;
};

class RatingBank
{
public:
    void load (const juce::File& f);
    void save (const juce::File& f) const;

    Rating get       (const juce::String& id) const;
    void   setStars  (const juce::String& id, int stars);
    void   setHidden (const juce::String& id, bool hidden);
    bool   isHidden  (const juce::String& id) const;

private:
    std::map<juce::String, Rating> data_;
};
