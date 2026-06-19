#include "PatternBank.h"

// ── Source extraction ─────────────────────────────────────────────────────────

juce::String PatternBank::extractSource (const juce::String& id)
{
    juce::String prefix = id.upToFirstOccurrenceOf ("_", false, false);
    // page-based IDs from older badness exports (p016, p020, …)
    if (prefix.startsWith ("p") && prefix.length() <= 4
        && prefix.substring (1).containsOnly ("0123456789"))
        return "badness";
    return prefix;
}

juce::StringArray PatternBank::getSources() const
{
    juce::StringArray result;
    for (auto& p : patterns)
        if (p.source.isNotEmpty() && !result.contains (p.source))
            result.add (p.source);
    result.sort (false);
    return result;
}

juce::StringArray PatternBank::getGenres() const
{
    juce::StringArray result;
    for (auto& p : patterns)
    {
        juce::String g = p.genre.isNotEmpty() ? p.genre
                       : (p.styles.size() > 0  ? p.styles[0] : "");
        if (g.isNotEmpty() && !result.contains (g))
            result.add (g);
    }
    result.sort (false);
    return result;
}

// ── Normalise: rotate so first hit is at index 0 ─────────────────────────────

std::vector<PatternStep> PatternBank::normalise (const std::vector<PatternStep>& steps)
{
    if (steps.empty()) return steps;
    int first = -1;
    for (int i = 0; i < (int)steps.size(); ++i)
        if (steps[(size_t)i].hit) { first = i; break; }
    if (first <= 0) return steps;
    std::vector<PatternStep> result;
    result.reserve (steps.size());
    for (int i = first; i < (int)steps.size(); ++i) result.push_back (steps[(size_t)i]);
    for (int i = 0;     i < first;             ++i) result.push_back (steps[(size_t)i]);
    return result;
}

// ── FNV-1a hash of normalised hit string ─────────────────────────────────────

juce::String PatternBank::computeHash (const RhythmPattern& p)
{
    auto norm = normalise (p.steps);
    juce::String s;
    for (auto& st : norm) s += st.hit ? "1" : "0";
    uint32_t hash = 2166136261u;
    for (auto c : s) { hash ^= (uint32_t)(unsigned char)c; hash *= 16777619u; }
    return juce::String::toHexString ((int)hash);
}

// ── Similarity: 1 - (Hamming distance / length) on normalised hit booleans ───

float PatternBank::similarity (const RhythmPattern& a, const RhythmPattern& b)
{
    auto na = normalise (a.steps);
    auto nb = normalise (b.steps);
    int len = juce::jmax ((int)na.size(), (int)nb.size());
    if (len == 0) return 1.0f;
    na.resize ((size_t)len);
    nb.resize ((size_t)len);
    int dist = 0;
    for (int i = 0; i < len; ++i)
        if (na[(size_t)i].hit != nb[(size_t)i].hit) ++dist;
    return 1.0f - (float)dist / (float)len;
}

// ── Add: reject exact dup, flag near-dup (> 0.85), otherwise insert ──────────

bool PatternBank::add (const RhythmPattern& p, bool& nearDuplicate, juce::String& nearDuplicateName)
{
    RhythmPattern toAdd = p;
    toAdd.hash = computeHash (p);
    nearDuplicate     = false;
    nearDuplicateName = {};

    for (auto& existing : patterns)
    {
        if (existing.hash == toAdd.hash)
            return false;
        if (similarity (existing, toAdd) > 0.85f)
        {
            nearDuplicate     = true;
            nearDuplicateName = existing.name;
        }
    }
    patterns.push_back (std::move (toAdd));
    return true;
}

// ── insert (unconditional) ────────────────────────────────────────────────────

void PatternBank::insert (const RhythmPattern& p)
{
    RhythmPattern toAdd = p;
    if (toAdd.hash.isEmpty()) toAdd.hash = computeHash (p);
    patterns.push_back (std::move (toAdd));
}

// ── findById ─────────────────────────────────────────────────────────────────

const RhythmPattern* PatternBank::findById (const juce::String& id) const
{
    for (auto& p : patterns)
        if (p.id == id) return &p;
    return nullptr;
}

// ── overwrite ─────────────────────────────────────────────────────────────────

bool PatternBank::overwrite (const juce::String& id, const RhythmPattern& updated)
{
    for (auto& p : patterns)
        if (p.id == id) { p = updated; return true; }
    return false;
}

// ── Filter ────────────────────────────────────────────────────────────────────

std::vector<const RhythmPattern*> PatternBank::filter (const juce::String& instrument,
                                                        const juce::String& genre,
                                                        const juce::String& style) const
{
    std::vector<const RhythmPattern*> result;
    for (auto& p : patterns)
    {
        bool instrMatch = instrument.isEmpty() || instrument == "All" || p.instrument == instrument;
        bool genreMatch = genre.isEmpty() || genre == "All"
                          || p.genre == genre
                          || (p.styles.size() > 0 && p.styles[0] == genre);
        bool styleMatch = style.isEmpty() || style == "All"
                          || p.style == style
                          || (p.styles.size() > 1 && p.styles[1] == style);
        if (instrMatch && genreMatch && styleMatch && !p.id.startsWith ("__"))
            result.push_back (&p);
    }
    return result;
}

// ── JSON load helpers ─────────────────────────────────────────────────────────

void PatternBank::appendFromFile (std::vector<RhythmPattern>& out, const juce::File& jsonFile)
{
    if (!jsonFile.existsAsFile()) return;
    juce::var json = juce::JSON::parse (jsonFile.loadFileAsString());
    if (!json.isArray()) return;

    for (int i = 0; i < json.size(); ++i)
    {
        auto* obj = json[i].getDynamicObject();
        if (!obj) continue;

        RhythmPattern p;
        p.id         = obj->getProperty ("id").toString();
        p.name       = obj->getProperty ("name").toString();
        p.instrument = obj->getProperty ("instrument").toString();
        p.resolution = (int)obj->getProperty ("resolution");
        p.notes      = obj->getProperty ("notes").toString();
        p.hash       = obj->getProperty ("hash").toString();
        p.source     = obj->getProperty ("source").toString();
        p.group      = obj->getProperty ("group").toString();
        p.genre      = obj->getProperty ("genre").toString();
        p.style      = obj->getProperty ("style").toString();
        p.coverage   = obj->hasProperty ("coverage") ? (float)(double)obj->getProperty ("coverage") : 1.0f;

        if (p.source.isEmpty()) p.source = extractSource (p.id);
        if (p.group.isEmpty() && p.instrument.isNotEmpty())
        {
            juce::String suffix = "_" + p.instrument;
            p.group = p.id.endsWith (suffix) ? p.id.dropLastCharacters (suffix.length()) : p.id;
        }

        if (auto* stylesArr = obj->getProperty ("styles").getArray())
            for (auto& s : *stylesArr) p.styles.add (s.toString());
        if (p.styles.isEmpty())
        {
            if (p.genre.isNotEmpty()) p.styles.add (p.genre);
            if (p.style.isNotEmpty() && p.style != p.genre) p.styles.add (p.style);
        }

        if (auto* stepsArr = obj->getProperty ("steps").getArray())
        {
            for (auto& st : *stepsArr)
            {
                PatternStep step;
                if (auto* so = st.getDynamicObject())
                {
                    step.hit      = (bool)so->getProperty ("hit");
                    step.velocity = (uint8_t)(int)so->getProperty ("velocity");
                    step.tune     = (int)so->getProperty ("tune");
                }
                p.steps.push_back (step);
            }
        }

        if (p.hash.isEmpty()) p.hash = computeHash (p);
        out.push_back (std::move (p));
    }
}

void PatternBank::load (const juce::File& jsonFile)
{
    patterns.clear();
    appendFromFile (patterns, jsonFile);
}

void PatternBank::loadDirectory (const juce::File& root)
{
    patterns.clear();

    // factory/<source>/patterns.json
    auto factory = root.getChildFile ("factory");
    if (factory.isDirectory())
        for (auto& sub : factory.findChildFiles (juce::File::findDirectories, false, "*"))
            appendFromFile (patterns, sub.getChildFile ("patterns.json"));

    // imported/midi_archive/<genre>/<style>/patterns.json  (two-level, new structure)
    // Also handles <genre>/patterns.json for any flat files that still exist
    auto imported = root.getChildFile ("imported").getChildFile ("midi_archive");
    if (imported.isDirectory())
    {
        for (auto& genreDir : imported.findChildFiles (juce::File::findDirectories, false, "*"))
        {
            // Flat file at genre level (legacy / single-style genres)
            appendFromFile (patterns, genreDir.getChildFile ("patterns.json"));
            // Two-level: genre/style/patterns.json
            for (auto& styleDir : genreDir.findChildFiles (juce::File::findDirectories, false, "*"))
                appendFromFile (patterns, styleDir.getChildFile ("patterns.json"));
        }
    }

    // user/patterns.json
    appendFromFile (patterns, root.getChildFile ("user").getChildFile ("patterns.json"));
}

// ── JSON save ─────────────────────────────────────────────────────────────────

void PatternBank::save (const juce::File& jsonFile) const
{
    juce::Array<juce::var> arr;
    for (auto& p : patterns)
    {
        if (p.id.startsWith ("__")) continue;  // skip live/unsaved scratch patterns
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("id",         p.id);
        obj->setProperty ("name",       p.name);
        obj->setProperty ("instrument", p.instrument);
        obj->setProperty ("resolution", p.resolution);
        obj->setProperty ("notes",      p.notes);
        obj->setProperty ("hash",       p.hash);

        juce::Array<juce::var> styles;
        for (auto& s : p.styles) styles.add (s);
        obj->setProperty ("styles", styles);

        juce::Array<juce::var> steps;
        for (auto& st : p.steps)
        {
            auto* so = new juce::DynamicObject();
            so->setProperty ("hit",      st.hit);
            so->setProperty ("velocity", (int)st.velocity);
            so->setProperty ("tune",     st.tune);
            steps.add (juce::var (so));
        }
        obj->setProperty ("steps", steps);
        arr.add (juce::var (obj));
    }

    jsonFile.getParentDirectory().createDirectory();
    jsonFile.replaceWithText (juce::JSON::toString (arr));
}
