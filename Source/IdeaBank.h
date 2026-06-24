#pragma once
#include "Idea.h"

class IdeaBank
{
public:
    void load (const juce::File& f);
    void save (const juce::File& f) const;

    void add      (const Idea& idea);
    bool overwrite (const juce::String& id, const Idea& idea);
    bool remove    (const juce::String& id);

    const std::vector<Idea>&   getAll()                              const { return ideas_; }
    const Idea*                findById        (const juce::String& id)      const;
    const RhythmPattern*       findPatternById (const juce::String& patId)   const;

private:
    std::vector<Idea> ideas_;
};
