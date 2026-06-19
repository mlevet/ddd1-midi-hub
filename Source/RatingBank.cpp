#include "RatingBank.h"

void RatingBank::load (const juce::File& f)
{
    data_.clear();
    if (!f.existsAsFile()) return;
    auto parsed = juce::JSON::parse (f.loadFileAsString());
    if (auto* obj = parsed.getDynamicObject())
        for (auto& kv : obj->getProperties())
        {
            auto s = kv.value.toString();
            if      (s == "favorite") data_[kv.name.toString()] = CurationState::Favorite;
            else if (s == "skip")     data_[kv.name.toString()] = CurationState::Skip;
        }
}

void RatingBank::save (const juce::File& f) const
{
    f.getParentDirectory().createDirectory();
    auto* root = new juce::DynamicObject();
    for (auto& [id, state] : data_)
        root->setProperty (id, state == CurationState::Favorite ? "favorite" : "skip");
    f.replaceWithText (juce::JSON::toString (juce::var (root), true));
}

CurationState RatingBank::getState (const juce::String& id) const
{
    auto it = data_.find (id);
    return it != data_.end() ? it->second : CurationState::Unrated;
}

void RatingBank::setState (const juce::String& id, CurationState s)
{
    if (s == CurationState::Unrated)
        data_.erase (id);
    else
        data_[id] = s;
}

bool RatingBank::isFavorite (const juce::String& id) const { return getState (id) == CurationState::Favorite; }
bool RatingBank::isSkipped  (const juce::String& id) const { return getState (id) == CurationState::Skip; }
