#include "IdeaBank.h"

// ── GroupTarget ───────────────────────────────────────────────────────────────

static juce::var groupTargetToVar (const GroupTarget& g)
{
    auto o = std::make_unique<juce::DynamicObject>();
    o->setProperty ("pad_index",      g.padIndex);
    o->setProperty ("offset_pulses",  g.offsetPulses);
    o->setProperty ("velocity_scale", g.velocityScale);
    o->setProperty ("tune_offset",    g.tuneOffset);
    return juce::var (o.release());
}

static GroupTarget groupTargetFromVar (const juce::var& v)
{
    GroupTarget g;
    g.padIndex      = (int)v.getProperty ("pad_index",      0);
    g.offsetPulses  = (int)v.getProperty ("offset_pulses",  0);
    g.velocityScale = (int)v.getProperty ("velocity_scale", 100);
    g.tuneOffset    = (int)v.getProperty ("tune_offset",    0);
    return g;
}

// ── PadConfig ─────────────────────────────────────────────────────────────────

static juce::var padConfigToVar (const PadConfig& p)
{
    auto o = std::make_unique<juce::DynamicObject>();
    o->setProperty ("note_receive",        p.noteReceive);
    o->setProperty ("mode",                (int)p.mode);
    o->setProperty ("inst_key",            p.instKey);
    o->setProperty ("semitone_offset",     p.semitoneOffset);
    o->setProperty ("kb_velocity",         p.kbVelocity);
    o->setProperty ("retrig_enabled",      p.retriggEnabled);
    o->setProperty ("retrig_rate",         p.retriggRate);
    o->setProperty ("retrig_max",          p.retriggMax);
    o->setProperty ("retrig_decay",        (double)p.retriggDecay);
    o->setProperty ("lfo_enabled",         p.lfoEnabled);
    o->setProperty ("lfo_mode",            p.lfoMode);
    o->setProperty ("lfo_rate",            p.lfoRate);
    o->setProperty ("lfo_depth",           p.lfoDepth);
    o->setProperty ("lfo_shape",           p.lfoShape);
    o->setProperty ("arp_rate",            p.arpRate);
    o->setProperty ("arp_direction",       p.arpDirection);
    o->setProperty ("arp_octaves",         p.arpOctaves);
    o->setProperty ("arp_latch",           p.arpLatch);
    o->setProperty ("selected_pattern_id", p.selectedPatternId);
    o->setProperty ("pattern_resolution",  p.patternResolution);
    o->setProperty ("pattern_offset",      p.patternOffset);
    o->setProperty ("delay_enabled",       p.delayEnabled);
    o->setProperty ("delay_rate",          p.delayRate);
    o->setProperty ("delay_repeats",       p.delayRepeats);
    o->setProperty ("delay_decay",         (double)p.delayDecay);
    o->setProperty ("muted",               p.muted);
    o->setProperty ("overdub_enabled",     p.overdubEnabled);

    juce::Array<juce::var> gts;
    for (const auto& g : p.groupTargets)
        gts.add (groupTargetToVar (g));
    o->setProperty ("group_targets", gts);

    return juce::var (o.release());
}

static PadConfig padConfigFromVar (const juce::var& v)
{
    PadConfig p;
    p.noteReceive       = (int)   v.getProperty ("note_receive",        36);
    p.mode              = (PadMode)(int)v.getProperty ("mode",          0);
    p.instKey           = (int)   v.getProperty ("inst_key",            36);
    p.semitoneOffset    = (int)   v.getProperty ("semitone_offset",     0);
    p.kbVelocity        = (int)   v.getProperty ("kb_velocity",         100);
    p.retriggEnabled    = (bool)  v.getProperty ("retrig_enabled",      false);
    p.retriggRate       = (int)   v.getProperty ("retrig_rate",         1);
    p.retriggMax        = (int)   v.getProperty ("retrig_max",          4);
    p.retriggDecay      = (float)(double)v.getProperty ("retrig_decay", 0.0);
    p.lfoEnabled        = (bool)  v.getProperty ("lfo_enabled",         false);
    p.lfoMode           = (int)   v.getProperty ("lfo_mode",            0);
    p.lfoRate           = (int)   v.getProperty ("lfo_rate",            0);
    p.lfoDepth          = (int)   v.getProperty ("lfo_depth",           3);
    p.lfoShape          = (int)   v.getProperty ("lfo_shape",           0);
    p.arpRate           = (int)   v.getProperty ("arp_rate",            1);
    p.arpDirection      = (int)   v.getProperty ("arp_direction",       0);
    p.arpOctaves        = (int)   v.getProperty ("arp_octaves",         1);
    p.arpLatch          = (bool)  v.getProperty ("arp_latch",           false);
    p.selectedPatternId =         v.getProperty ("selected_pattern_id", "").toString();
    p.patternResolution = (int)   v.getProperty ("pattern_resolution",  1);
    p.patternOffset     = (int)   v.getProperty ("pattern_offset",      0);
    p.delayEnabled      = (bool)  v.getProperty ("delay_enabled",       false);
    p.delayRate         = (int)   v.getProperty ("delay_rate",          1);
    p.delayRepeats      = (int)   v.getProperty ("delay_repeats",       3);
    p.delayDecay        = (float)(double)v.getProperty ("delay_decay",  50.0);
    p.muted             = (bool)  v.getProperty ("muted",               false);
    p.overdubEnabled    = (bool)  v.getProperty ("overdub_enabled",     false);

    if (const auto* arr = v.getProperty ("group_targets", juce::var{}).getArray())
        for (const auto& g : *arr)
            p.groupTargets.push_back (groupTargetFromVar (g));

    return p;
}

// ── PatternStep ───────────────────────────────────────────────────────────────

static juce::var patternStepToVar (const PatternStep& s)
{
    auto o = std::make_unique<juce::DynamicObject>();
    o->setProperty ("hit",      s.hit);
    o->setProperty ("velocity", (int)s.velocity);
    o->setProperty ("tune",     s.tune);
    return juce::var (o.release());
}

static PatternStep patternStepFromVar (const juce::var& v)
{
    PatternStep s;
    s.hit      = (bool)   v.getProperty ("hit",      false);
    s.velocity = (uint8_t)(int)v.getProperty ("velocity", 100);
    s.tune     = (int)    v.getProperty ("tune",     0);
    return s;
}

// ── RhythmPattern ─────────────────────────────────────────────────────────────

static juce::var rhythmPatternToVar (const RhythmPattern& p)
{
    auto o = std::make_unique<juce::DynamicObject>();
    o->setProperty ("id",         p.id);
    o->setProperty ("name",       p.name);
    o->setProperty ("instrument", p.instrument);
    o->setProperty ("source",     p.source);
    o->setProperty ("group",      p.group);
    o->setProperty ("genre",      p.genre);
    o->setProperty ("style",      p.style);
    o->setProperty ("resolution", p.resolution);
    o->setProperty ("notes",      p.notes);
    o->setProperty ("hash",       p.hash);
    o->setProperty ("coverage",   (double)p.coverage);

    juce::Array<juce::var> stylesArr;
    for (const auto& s : p.styles)
        stylesArr.add (s);
    o->setProperty ("styles", stylesArr);

    juce::Array<juce::var> stepsArr;
    for (const auto& s : p.steps)
        stepsArr.add (patternStepToVar (s));
    o->setProperty ("steps", stepsArr);

    return juce::var (o.release());
}

static RhythmPattern rhythmPatternFromVar (const juce::var& v)
{
    RhythmPattern p;
    p.id         = v.getProperty ("id",         "").toString();
    p.name       = v.getProperty ("name",       "").toString();
    p.instrument = v.getProperty ("instrument", "").toString();
    p.source     = v.getProperty ("source",     "").toString();
    p.group      = v.getProperty ("group",      "").toString();
    p.genre      = v.getProperty ("genre",      "").toString();
    p.style      = v.getProperty ("style",      "").toString();
    p.resolution = (int)    v.getProperty ("resolution", 1);
    p.notes      =          v.getProperty ("notes",      "").toString();
    p.hash       =          v.getProperty ("hash",       "").toString();
    p.coverage   = (float)(double)v.getProperty ("coverage", 1.0);

    if (const auto* arr = v.getProperty ("styles", juce::var{}).getArray())
        for (const auto& s : *arr)
            p.styles.add (s.toString());

    if (const auto* arr = v.getProperty ("steps", juce::var{}).getArray())
        for (const auto& s : *arr)
            p.steps.push_back (patternStepFromVar (s));

    return p;
}

// ── PadAssignment + PatternSet ────────────────────────────────────────────────

static juce::var padAssignmentToVar (const PadAssignment& a)
{
    auto o = std::make_unique<juce::DynamicObject>();
    o->setProperty ("pad_index",  a.padIndex);
    o->setProperty ("pattern_id", a.patternId);
    o->setProperty ("resolution", a.resolution);
    o->setProperty ("offset",     a.offset);
    return juce::var (o.release());
}

static PadAssignment padAssignmentFromVar (const juce::var& v)
{
    PadAssignment a;
    a.padIndex   = (int)v.getProperty ("pad_index",  0);
    a.patternId  =      v.getProperty ("pattern_id", "").toString();
    a.resolution = (int)v.getProperty ("resolution", 1);
    a.offset     = (int)v.getProperty ("offset",     0);
    return a;
}

static juce::var patternSetToVar (const PatternSet& s)
{
    auto o = std::make_unique<juce::DynamicObject>();
    o->setProperty ("id",      s.id);
    o->setProperty ("name",    s.name);
    o->setProperty ("style",   s.style);
    o->setProperty ("source",  s.source);
    o->setProperty ("is_fill", s.isFill);

    juce::Array<juce::var> arr;
    for (const auto& a : s.assignments)
        arr.add (padAssignmentToVar (a));
    o->setProperty ("assignments", arr);

    return juce::var (o.release());
}

static PatternSet patternSetFromVar (const juce::var& v)
{
    PatternSet s;
    s.id     = v.getProperty ("id",      "").toString();
    s.name   = v.getProperty ("name",    "").toString();
    s.style  = v.getProperty ("style",   "").toString();
    s.source = v.getProperty ("source",  "").toString();
    s.isFill = (bool)v.getProperty ("is_fill", false);

    if (const auto* arr = v.getProperty ("assignments", juce::var{}).getArray())
        for (const auto& a : *arr)
            s.assignments.push_back (padAssignmentFromVar (a));

    return s;
}

// ── IdeaOrigin ────────────────────────────────────────────────────────────────

static juce::var ideaOriginToVar (const IdeaOrigin& o)
{
    auto obj = std::make_unique<juce::DynamicObject>();
    obj->setProperty ("type",           o.type);
    obj->setProperty ("source",         o.source);
    obj->setProperty ("parent_group",   o.parentGroup);
    obj->setProperty ("parent_name",    o.parentName);
    obj->setProperty ("parent_idea_id", o.parentIdeaId);
    return juce::var (obj.release());
}

static IdeaOrigin ideaOriginFromVar (const juce::var& v)
{
    IdeaOrigin o;
    o.type         = v.getProperty ("type",           "scratch").toString();
    o.source       = v.getProperty ("source",         "").toString();
    o.parentGroup  = v.getProperty ("parent_group",   "").toString();
    o.parentName   = v.getProperty ("parent_name",    "").toString();
    o.parentIdeaId = v.getProperty ("parent_idea_id", "").toString();
    return o;
}

// ── Idea ──────────────────────────────────────────────────────────────────────

static juce::var ideaToVar (const Idea& idea)
{
    auto o = std::make_unique<juce::DynamicObject>();
    o->setProperty ("id",          idea.id);
    o->setProperty ("name",        idea.name);
    o->setProperty ("created_at",  idea.createdAt);
    o->setProperty ("updated_at",  idea.updatedAt);
    o->setProperty ("origin",      ideaOriginToVar (idea.origin));
    o->setProperty ("notes",       idea.notes);
    o->setProperty ("pattern_set", patternSetToVar (idea.patternSet));

    juce::Array<juce::var> patternsArr;
    for (const auto& p : idea.patterns)
        patternsArr.add (rhythmPatternToVar (p));
    o->setProperty ("patterns", patternsArr);

    juce::Array<juce::var> cfgsArr;
    for (const auto& c : idea.padConfigs)
        cfgsArr.add (padConfigToVar (c));
    o->setProperty ("pad_configs", cfgsArr);

    return juce::var (o.release());
}

static Idea ideaFromVar (const juce::var& v)
{
    Idea idea;
    idea.id        = v.getProperty ("id",         "").toString();
    idea.name      = v.getProperty ("name",       "").toString();
    idea.createdAt = v.getProperty ("created_at", "").toString();
    idea.updatedAt = v.getProperty ("updated_at", "").toString();
    idea.notes     = v.getProperty ("notes",      "").toString();
    idea.origin    = ideaOriginFromVar (v.getProperty ("origin",      juce::var{}));
    idea.patternSet = patternSetFromVar (v.getProperty ("pattern_set", juce::var{}));

    if (const auto* arr = v.getProperty ("patterns", juce::var{}).getArray())
        for (const auto& p : *arr)
            idea.patterns.push_back (rhythmPatternFromVar (p));

    if (const auto* arr = v.getProperty ("pad_configs", juce::var{}).getArray())
        for (const auto& c : *arr)
            idea.padConfigs.push_back (padConfigFromVar (c));

    return idea;
}

// ── IdeaBank ──────────────────────────────────────────────────────────────────

void IdeaBank::load (const juce::File& f)
{
    ideas_.clear();
    if (!f.existsAsFile()) return;

    auto parsed = juce::JSON::parse (f.loadFileAsString());
    if (const auto* arr = parsed.getProperty ("ideas", juce::var{}).getArray())
        for (const auto& v : *arr)
            ideas_.push_back (ideaFromVar (v));
}

void IdeaBank::save (const juce::File& f) const
{
    f.getParentDirectory().createDirectory();

    auto root = std::make_unique<juce::DynamicObject>();
    juce::Array<juce::var> arr;
    for (const auto& idea : ideas_)
        arr.add (ideaToVar (idea));
    root->setProperty ("ideas", arr);

    f.replaceWithText (juce::JSON::toString (juce::var (root.release()), true));
}

void IdeaBank::add (const Idea& idea)
{
    ideas_.push_back (idea);
}

bool IdeaBank::overwrite (const juce::String& id, const Idea& idea)
{
    for (auto& existing : ideas_)
        if (existing.id == id) { existing = idea; return true; }
    return false;
}

bool IdeaBank::remove (const juce::String& id)
{
    auto it = std::find_if (ideas_.begin(), ideas_.end(),
        [&id](const Idea& i){ return i.id == id; });
    if (it == ideas_.end()) return false;
    ideas_.erase (it);
    return true;
}

const Idea* IdeaBank::findById (const juce::String& id) const
{
    for (const auto& idea : ideas_)
        if (idea.id == id) return &idea;
    return nullptr;
}

const RhythmPattern* IdeaBank::findPatternById (const juce::String& patId) const
{
    for (const auto& idea : ideas_)
        for (const auto& p : idea.patterns)
            if (p.id == patId) return &p;
    return nullptr;
}
