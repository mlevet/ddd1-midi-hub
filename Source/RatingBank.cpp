#include "RatingBank.h"

void RatingBank::load (const juce::File& f)
{
    data_.clear();
    if (!f.existsAsFile()) return;
    auto parsed = juce::JSON::parse (f.loadFileAsString());
    if (auto* obj = parsed.getDynamicObject())
        for (auto& kv : obj->getProperties())
        {
            Rating r;
            if (auto* inner = kv.value.getDynamicObject())
            {
                r.stars  = (int)(double)inner->getProperty ("rating");
                r.hidden = (bool)inner->getProperty ("hidden");
            }
            data_[kv.name.toString()] = r;
        }
}

void RatingBank::save (const juce::File& f) const
{
    f.getParentDirectory().createDirectory();
    auto* root = new juce::DynamicObject();
    for (auto& [id, r] : data_)
    {
        auto* inner = new juce::DynamicObject();
        inner->setProperty ("rating", r.stars);
        inner->setProperty ("hidden", r.hidden);
        root->setProperty (id, juce::var (inner));
    }
    f.replaceWithText (juce::JSON::toString (juce::var (root), true));
}

Rating RatingBank::get (const juce::String& id) const
{
    auto it = data_.find (id);
    return it != data_.end() ? it->second : Rating{};
}

void RatingBank::setStars (const juce::String& id, int stars)
{
    data_[id].stars = juce::jlimit (0, 5, stars);
}

void RatingBank::setHidden (const juce::String& id, bool hidden)
{
    data_[id].hidden = hidden;
}

bool RatingBank::isHidden (const juce::String& id) const
{
    auto it = data_.find (id);
    return it != data_.end() && it->second.hidden;
}
