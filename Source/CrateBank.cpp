#include "CrateBank.h"

void CrateBank::load (const juce::File& f)
{
    ids_.clear();
    if (!f.existsAsFile()) return;
    auto parsed = juce::JSON::parse (f.loadFileAsString());
    if (const auto* arr = parsed.getProperty ("patterns", juce::var{}).getArray())
        for (const auto& v : *arr)
            ids_.insert (v.toString());
}

void CrateBank::save (const juce::File& f) const
{
    f.getParentDirectory().createDirectory();
    auto root = std::make_unique<juce::DynamicObject>();
    juce::Array<juce::var> arr;
    for (const auto& id : ids_)
        arr.add (id);
    root->setProperty ("patterns", arr);
    f.replaceWithText (juce::JSON::toString (juce::var (root.release()), true));
}

bool CrateBank::contains (const juce::String& id) const { return ids_.count (id) > 0; }

void CrateBank::toggle (const juce::String& id)
{
    if (ids_.count (id)) ids_.erase (id);
    else                 ids_.insert (id);
}

void CrateBank::clear() { ids_.clear(); }
