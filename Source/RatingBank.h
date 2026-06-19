#pragma once
#include <JuceHeader.h>
#include <map>

enum class CurationState { Unrated, Favorite, Skip };

class RatingBank
{
public:
    void load (const juce::File& f);
    void save (const juce::File& f) const;

    CurationState getState   (const juce::String& id) const;
    void          setState   (const juce::String& id, CurationState s);
    bool          isFavorite (const juce::String& id) const;
    bool          isSkipped  (const juce::String& id) const;

private:
    std::map<juce::String, CurationState> data_;
};
