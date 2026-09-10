#include "PresetManager.h"
#include "Parameters.h"

namespace dr
{

namespace
{
    constexpr const char* presetFileExtension = ".drpreset";

    struct FactoryPreset
    {
        const char* name;
        float drive, tube, mix;
        float hTone, hTime, hMix;
        float mDist, mAmount, mMix;
        float vHighCut, vDecay, vMix;
        float tDistance, tRoomsize, tMix;
    };

    // drive tube  mix | tone time hMix | dist amt mMix | hcut decay vMix | dist size tMix
    const FactoryPreset factoryPresets[] =
    {
        { "Diamond Room",  3.0f, 3.5f, 35.0f,  0.0f, 6.9f, 40.0f,  2.0f, 4.5f, 40.0f,  9.3f, 5.9f, 40.0f,  2.6f, 7.8f, 40.0f },
        { "Vocal Plate",   1.5f, 2.5f, 28.0f,  2.5f, 5.4f, 62.0f,  0.5f, 3.0f, 18.0f,  9.6f, 4.2f, 30.0f,  1.8f, 5.0f, 22.0f },
        { "Tight Room",    2.0f, 2.0f, 22.0f, -1.0f, 3.2f, 20.0f,  1.0f, 3.5f, 25.0f,  8.4f, 3.0f, 15.0f,  1.2f, 4.5f, 70.0f },
        { "Cathedral",     1.0f, 3.0f, 48.0f,  1.5f, 9.2f, 70.0f,  0.0f, 5.5f, 35.0f,  9.5f, 8.4f, 65.0f,  4.5f, 9.6f, 40.0f },
        { "Dark Chamber",  4.0f, 5.0f, 38.0f, -6.0f, 6.0f, 45.0f,  3.5f, 6.0f, 60.0f,  5.5f, 5.2f, 30.0f,  3.0f, 6.4f, 25.0f },
        { "Driven Wash",   7.5f, 6.5f, 55.0f,  3.0f, 8.4f, 55.0f,  6.5f, 7.5f, 50.0f,  9.0f, 7.6f, 60.0f,  5.5f, 8.6f, 35.0f },
        { "Crushed Verb", 10.0f, 9.0f, 70.0f, -3.0f, 7.2f, 60.0f, 10.0f, 8.5f, 70.0f,  6.8f, 6.6f, 45.0f,  7.0f, 7.2f, 45.0f },
        { "Subtle Air",    0.0f, 1.0f, 16.0f,  5.0f, 4.6f, 35.0f,  0.0f, 2.0f, 10.0f, 10.0f, 3.4f, 45.0f,  1.0f, 6.0f, 18.0f },
    };

    juce::String sanitise (const juce::String& name)
    {
        return juce::File::createLegalFileName (name.trim());
    }
}

//==============================================================================
PresetManager::PresetManager (juce::AudioProcessorValueTreeState& stateToUse,
                              juce::UndoManager& undoManagerToUse)
    : state (stateToUse), undoManager (undoManagerToUse)
{
    currentName = factoryPresets[0].name;

    // Anything the user moves marks the preset as edited.
    for (const auto* id : { params::drive, params::tube, params::mix,
                            params::hTone, params::hTime, params::hMix, params::hOn,
                            params::mDist, params::mAmount, params::mMix, params::mOn,
                            params::vHighCut, params::vDecay, params::vMix, params::vOn,
                            params::tDistance, params::tRoomsize, params::tMix, params::tOn })
        state.addParameterListener (id, this);
}

PresetManager::~PresetManager()
{
    for (const auto* id : { params::drive, params::tube, params::mix,
                            params::hTone, params::hTime, params::hMix, params::hOn,
                            params::mDist, params::mAmount, params::mMix, params::mOn,
                            params::vHighCut, params::vDecay, params::vMix, params::vOn,
                            params::tDistance, params::tRoomsize, params::tMix, params::tOn })
        state.removeParameterListener (id, this);
}

//==============================================================================
juce::File PresetManager::getUserPresetDirectory()
{
    auto directory = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                         .getChildFile ("DiamondRoom")
                         .getChildFile ("Presets");

    if (! directory.isDirectory())
        directory.createDirectory();

    return directory;
}

juce::StringArray PresetManager::getFactoryPresetNames() const
{
    juce::StringArray names;

    for (const auto& preset : factoryPresets)
        names.add (preset.name);

    return names;
}

juce::StringArray PresetManager::getUserPresetNames() const
{
    juce::StringArray names;

    for (const auto& file : getUserPresetDirectory().findChildFiles (
             juce::File::findFiles, false, juce::String ("*") + presetFileExtension))
        names.add (file.getFileNameWithoutExtension());

    names.sortNatural();
    return names;
}

juce::StringArray PresetManager::getAllPresetNames() const
{
    auto names = getFactoryPresetNames();
    names.addArray (getUserPresetNames());
    return names;
}

bool PresetManager::isUserPreset (const juce::String& name) const
{
    return getUserPresetNames().contains (name);
}

//==============================================================================
void PresetManager::applyFactoryPreset (int index)
{
    const auto& preset = factoryPresets[(size_t) index];

    const std::pair<const char*, float> values[]
    {
        { params::drive, preset.drive }, { params::tube, preset.tube }, { params::mix, preset.mix },
        { params::hTone, preset.hTone }, { params::hTime, preset.hTime }, { params::hMix, preset.hMix },
        { params::mDist, preset.mDist }, { params::mAmount, preset.mAmount }, { params::mMix, preset.mMix },
        { params::vHighCut, preset.vHighCut }, { params::vDecay, preset.vDecay }, { params::vMix, preset.vMix },
        { params::tDistance, preset.tDistance }, { params::tRoomsize, preset.tRoomsize }, { params::tMix, preset.tMix },
    };

    for (const auto& [id, value] : values)
        if (auto* parameter = state.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));

    // Factory presets all arm every reverb.
    for (const auto* id : { params::hOn, params::mOn, params::vOn, params::tOn })
        if (auto* parameter = state.getParameter (id))
            parameter->setValueNotifyingHost (1.0f);
}

void PresetManager::loadPreset (const juce::String& name)
{
    const juce::ScopedValueSetter<bool> scope (loading, true);

    undoManager.beginNewTransaction ("Load preset " + name);

    const auto factoryNames = getFactoryPresetNames();
    const auto factoryIndex = factoryNames.indexOf (name);

    if (factoryIndex >= 0)
    {
        applyFactoryPreset (factoryIndex);
    }
    else
    {
        const auto file = getUserPresetDirectory().getChildFile (sanitise (name) + presetFileExtension);

        if (! file.existsAsFile())
            return;

        if (auto xml = juce::XmlDocument::parse (file))
        {
            auto tree = juce::ValueTree::fromXml (*xml);

            if (tree.isValid() && tree.hasType (state.state.getType()))
            {
                // The window size belongs to this machine, not to the preset.
                const auto width = state.state.getProperty ("uiWidth");
                state.replaceState (tree);

                if (! width.isVoid())
                    state.state.setProperty ("uiWidth", width, nullptr);
            }
        }
    }

    currentName = name;
    modified = false;
    sendChangeMessage();
}

int PresetManager::indexOfCurrentPreset() const
{
    return getAllPresetNames().indexOf (currentName);
}

void PresetManager::loadNext()
{
    const auto names = getAllPresetNames();

    if (names.isEmpty())
        return;

    const auto index = juce::jmax (0, indexOfCurrentPreset());
    loadPreset (names[(index + 1) % names.size()]);
}

void PresetManager::loadPrevious()
{
    const auto names = getAllPresetNames();

    if (names.isEmpty())
        return;

    const auto index = juce::jmax (0, indexOfCurrentPreset());
    loadPreset (names[(index + names.size() - 1) % names.size()]);
}

//==============================================================================
juce::Result PresetManager::saveUserPreset (const juce::String& name)
{
    const auto cleaned = sanitise (name);

    if (cleaned.isEmpty())
        return juce::Result::fail ("That is not a usable preset name.");

    if (getFactoryPresetNames().contains (cleaned))
        return juce::Result::fail ("\"" + cleaned + "\" is a factory preset and cannot be overwritten.");

    auto tree = state.copyState();
    tree.removeProperty ("uiWidth", nullptr);

    auto xml = tree.createXml();

    if (xml == nullptr)
        return juce::Result::fail ("Could not serialise the current settings.");

    const auto file = getUserPresetDirectory().getChildFile (cleaned + presetFileExtension);

    if (! xml->writeTo (file))
        return juce::Result::fail ("Could not write " + file.getFullPathName());

    currentName = cleaned;
    modified = false;
    sendChangeMessage();
    return juce::Result::ok();
}

juce::Result PresetManager::deleteUserPreset (const juce::String& name)
{
    if (! isUserPreset (name))
        return juce::Result::fail ("\"" + name + "\" is not a user preset.");

    const auto file = getUserPresetDirectory().getChildFile (sanitise (name) + presetFileExtension);

    if (! file.deleteFile())
        return juce::Result::fail ("Could not delete " + file.getFullPathName());

    if (currentName == name)
        modified = true;

    sendChangeMessage();
    return juce::Result::ok();
}

//==============================================================================
juce::String PresetManager::getDisplayName() const
{
    if (currentName.isEmpty())
        return "Init";

    return modified ? currentName + " *" : currentName;
}

void PresetManager::parameterChanged (const juce::String&, float)
{
    if (loading || modified)
        return;

    modified = true;

    // Called from whichever thread moved the parameter, including the audio
    // thread when a host automates one.
    juce::MessageManager::callAsync ([safe = juce::WeakReference<PresetManager> (this)]
    {
        if (safe != nullptr)
            safe->sendChangeMessage();
    });
}

} // namespace dr
