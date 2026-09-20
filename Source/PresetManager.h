#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace dr
{

/**
    Factory and user presets.

    Factory presets are parameter tables compiled into the plugin; user presets
    are XML files of the whole state, written next to the other plugin data for
    this machine. Both are addressed by name, and the name of anything the user
    has since altered is shown with a trailing asterisk.
*/
class PresetManager final : public juce::ChangeBroadcaster,
                            private juce::AudioProcessorValueTreeState::Listener
{
public:
    PresetManager (juce::AudioProcessorValueTreeState& state, juce::UndoManager& undoManager);
    ~PresetManager() override;

    /** Where user presets live. Created on demand. */
    static juce::File getUserPresetDirectory();

    juce::StringArray getFactoryPresetNames() const;
    juce::StringArray getUserPresetNames() const;

    /** Factory presets followed by user presets, in that order. */
    juce::StringArray getAllPresetNames() const;

    bool isUserPreset (const juce::String& name) const;

    void loadPreset (const juce::String& name);
    void loadNext();
    void loadPrevious();

    /** Writes the current state as a user preset, overwriting one of the same name. */
    juce::Result saveUserPreset (const juce::String& name);
    juce::Result deleteUserPreset (const juce::String& name);

    juce::String getCurrentPresetName() const { return currentName; }
    bool isModified() const { return modified; }

    /** Name for display: the preset name, marked if it has been edited since. */
    juce::String getDisplayName() const;

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void applyFactoryPreset (int index);
    int indexOfCurrentPreset() const;

    juce::AudioProcessorValueTreeState& state;
    juce::UndoManager& undoManager;

    juce::String currentName;
    std::atomic<bool> modified { false };
    std::atomic<bool> loading { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};

} // namespace dr
