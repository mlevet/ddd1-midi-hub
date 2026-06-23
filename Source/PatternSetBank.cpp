#include "PatternSetBank.h"

// ── JSON helpers ──────────────────────────────────────────────────────────────

static juce::var assignmentToVar (const PadAssignment& a)
{
    auto obj = std::make_unique<juce::DynamicObject>();
    obj->setProperty ("padIndex",   a.padIndex);
    obj->setProperty ("patternId",  a.patternId);
    obj->setProperty ("resolution", a.resolution);
    obj->setProperty ("offset",     a.offset);
    return juce::var (obj.release());
}

static PadAssignment assignmentFromVar (const juce::var& v)
{
    PadAssignment a;
    a.padIndex   = (int)   v.getProperty ("padIndex",   0);
    a.patternId  =         v.getProperty ("patternId",  "").toString();
    a.resolution = (int)   v.getProperty ("resolution", 1);
    a.offset     = (int)   v.getProperty ("offset",     0);
    return a;
}

static juce::var setToVar (const PatternSet& s)
{
    auto obj = std::make_unique<juce::DynamicObject>();
    obj->setProperty ("id",     s.id);
    obj->setProperty ("name",   s.name);
    obj->setProperty ("style",  s.style);
    obj->setProperty ("source", s.source);
    obj->setProperty ("isFill", s.isFill);
    juce::Array<juce::var> arr;
    for (auto& a : s.assignments)
        arr.add (assignmentToVar (a));
    obj->setProperty ("assignments", arr);
    return juce::var (obj.release());
}

static PatternSet setFromVar (const juce::var& v)
{
    PatternSet s;
    s.id     = v.getProperty ("id",     "").toString();
    s.name   = v.getProperty ("name",   "").toString();
    s.style  = v.getProperty ("style",  "").toString();
    s.source = v.getProperty ("source", "").toString();
    s.isFill = (bool)v.getProperty ("isFill", false);
    if (const auto* arr = v.getProperty ("assignments", juce::var{}).getArray())
        for (auto& a : *arr)
            s.assignments.push_back (assignmentFromVar (a));
    return s;
}

// ── PatternSetBank ────────────────────────────────────────────────────────────

void PatternSetBank::load (const juce::File& f)
{
    sets.clear();
    if (!f.existsAsFile()) return;
    auto parsed = juce::JSON::parse (f.loadFileAsString());
    if (const auto* arr = parsed.getArray())
        for (auto& v : *arr)
            sets.push_back (setFromVar (v));
}

void PatternSetBank::save (const juce::File& f) const
{
    juce::Array<juce::var> arr;
    for (auto& s : sets) arr.add (setToVar (s));
    f.replaceWithText (juce::JSON::toString (juce::var (arr), false));
}

void PatternSetBank::add (const PatternSet& s)
{
    sets.push_back (s);
}

bool PatternSetBank::overwrite (const juce::String& id, const PatternSet& s)
{
    for (auto& existing : sets)
        if (existing.id == id) { existing = s; return true; }
    return false;
}

bool PatternSetBank::remove (const juce::String& id)
{
    auto it = std::find_if (sets.begin(), sets.end(),
        [&id](const PatternSet& s){ return s.id == id; });
    if (it == sets.end()) return false;
    sets.erase (it);
    return true;
}

const PatternSet* PatternSetBank::findById (const juce::String& id) const
{
    for (auto& s : sets)
        if (s.id == id) return &s;
    return nullptr;
}

std::vector<PatternSet> PatternSetBank::buildAutoSets (const PatternBank& bank,
                                                        const int* noteReceive,
                                                        int numPads) const
{
    // Instrument name → candidate MIDI notes (in preference order)
    struct InstrMap { const char* name; int notes[3]; int count; };
    static const InstrMap kMap[] = {
        { "kick",         { 36,  0,  0 }, 1 },
        { "snare",        { 38,  0,  0 }, 1 },
        { "rimshot",      { 37,  0,  0 }, 1 },
        { "clap",         { 39,  0,  0 }, 1 },
        { "closed_hihat", { 44,  0,  0 }, 1 },
        { "open_hihat",   { 46,  0,  0 }, 1 },
        { "pedal_hihat",  { 44, 46,  0 }, 2 },
        { "high_tom",     { 48,  0,  0 }, 1 },
        { "mid_tom",      { 45,  0,  0 }, 1 },
        { "low_tom",      { 43,  0,  0 }, 1 },
        { "ride",         { 51,  0,  0 }, 1 },
        { "crash",        { 49,  0,  0 }, 1 },
        { "cowbell",      { 56,  0,  0 }, 1 },
        { "tambourine",   { 54, 58,  0 }, 2 },
        { "shaker",       { 70, 54,  0 }, 2 },
        { "claves",       { 75,  0,  0 }, 1 },
        { "conga",        { 63, 64,  0 }, 2 },
        { "bongo",        { 60, 61,  0 }, 2 },
        { "bass",         { 35, 36,  0 }, 2 },
        // legacy/simplified tags from older bank entries
        { "hh",           { 44, 46,  0 }, 2 },
        { "tom",          { 48, 45, 43 }, 3 },
        { "perc",         { 56, 58, 39 }, 3 },
        { "cymbal",       { 51, 49,  0 }, 2 },
    };

    auto padForInstr = [&](const juce::String& instr) -> int {
        for (auto& m : kMap)
        {
            if (instr == m.name)
            {
                for (int ni = 0; ni < m.count; ++ni)
                    for (int p = 0; p < numPads; ++p)
                        if (noteReceive[p] == m.notes[ni]) return p;
            }
        }
        return -1;
    };

    // Group patterns by their group field (or id with instrument stripped as fallback)
    std::map<juce::String, std::vector<const RhythmPattern*>> groups;
    for (const auto& pat : bank.getAll())
    {
        if (pat.id.startsWith ("__")) continue;
        juce::String groupKey = pat.group.isNotEmpty() ? pat.group : pat.id;
        groups[groupKey].push_back (&pat);
    }

    std::vector<PatternSet> result;
    for (auto& [name, patterns] : groups)
    {
        if (patterns.size() < 2) continue;
        PatternSet s;
        s.id     = "auto_" + name.toLowerCase().replaceCharacters (" ", "_");
        s.name   = patterns[0]->name.isNotEmpty() ? patterns[0]->name : name;
        s.style  = patterns[0]->genre.isNotEmpty() ? patterns[0]->genre
                 : (patterns[0]->styles.size() > 0 ? patterns[0]->styles[0] : "");
        s.source = patterns[0]->source;
        s.isFill = patterns[0]->isFill();

        int nextPad = 0;
        for (auto* pat : patterns)
        {
            PadAssignment a;
            a.patternId  = pat->id;
            a.resolution = pat->resolution;
            a.offset     = 0;

            int p = padForInstr (pat->instrument);
            if (p < 0)
            {
                // Fall back to next unassigned pad
                while (nextPad < numPads)
                {
                    bool used = false;
                    for (auto& ex : s.assignments)
                        if (ex.padIndex == nextPad) { used = true; break; }
                    if (!used) break;
                    ++nextPad;
                }
                if (nextPad >= numPads) continue;
                p = nextPad++;
            }

            // Skip if pad already assigned in this set
            bool dup = false;
            for (auto& ex : s.assignments)
                if (ex.padIndex == p) { dup = true; break; }
            if (!dup)
            {
                a.padIndex = p;
                s.assignments.push_back (a);
            }
        }

        if (!s.assignments.empty())
            result.push_back (std::move (s));
    }
    return result;
}
