#pragma once
#include <JuceHeader.h>

struct PatternStep
{
    bool    hit      = false;
    uint8_t velocity = 100;
    int     tune     = 0;   // semitone offset applied via seqTune formula
};

struct RhythmPattern
{
    juce::String             id;
    juce::String             name;
    juce::String             instrument;
    juce::String             source;      // "bardet", "badness", "roland", "allen"
    juce::String             group;       // grouping key, e.g. "bardet_afro_cuban_01"
    juce::StringArray        styles;      // [genre, style] for compat; prefer genre/style fields
    juce::String             genre;
    juce::String             style;
    std::vector<PatternStep> steps;
    int                      resolution = 1;
    juce::String             notes;
    juce::String             hash;
    float                    coverage = 1.0f;

    bool isFill() const
    {
        return group.contains ("_break_") || group.contains ("_fill_")
            || group.contains ("_ending_") || group.endsWith ("_break")
            || group.endsWith ("_fill") || group.endsWith ("_ending")
            || styles.contains ("fill") || styles.contains ("ending");
    }
};

class PatternBank
{
public:
    void load          (const juce::File& jsonFile);
    void loadDirectory (const juce::File& root);
    void save (const juce::File& jsonFile) const;

    bool add (const RhythmPattern& p,
              bool& nearDuplicate,
              juce::String& nearDuplicateName);

    // Always inserts — no duplicate check. Use for explicit user saves.
    void insert (const RhythmPattern& p);

    bool overwrite (const juce::String& id, const RhythmPattern& updated);

    std::vector<const RhythmPattern*> filter (const juce::String& instrument,
                                               const juce::String& genre,
                                               const juce::String& style) const;

    juce::StringArray getSources() const;
    juce::StringArray getGenres()  const;
    static juce::String extractSource (const juce::String& id);

    const std::vector<RhythmPattern>& getAll() const { return patterns; }
    const RhythmPattern* findById (const juce::String& id) const;

private:
    std::vector<RhythmPattern> patterns;

    static void                     appendFromFile (std::vector<RhythmPattern>& out, const juce::File& f);
    static juce::String             computeHash (const RhythmPattern& p);
    static float                    similarity  (const RhythmPattern& a, const RhythmPattern& b);
    static std::vector<PatternStep> normalise   (const std::vector<PatternStep>& steps);
};
