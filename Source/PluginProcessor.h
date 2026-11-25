#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Audition.h"
#include "Clip.h"
#include "Engine.h"

namespace ParamID
{
    inline constexpr const char* history = "history";   // 1, 2, 5, 10, 20 Minuten
    inline constexpr const char* snap    = "snap";      // Aus, 1/16, 1/8, Beat, Takt
    inline constexpr const char* hold    = "hold";
}

namespace PropID
{
    inline constexpr const char* folder  = "folder";
    inline constexpr const char* project = "project";
    inline constexpr const char* type    = "type";
    inline constexpr const char* form    = "form";
    inline constexpr const char* key     = "key";
    inline constexpr const char* scale   = "scale";
    inline constexpr const char* fadeMs  = "fadeMs";
    inline constexpr const char* fadeShape = "fadeShape";   // 0 Smooth, 1 Linear, 2 Equal power, 3 Exponential
    inline constexpr const char* bits    = "bits";
    inline constexpr const char* autoAlign = "autoAlign";  // "0" = aus (Standard: an)
    inline constexpr const char* gridMs  = "gridMs";      // Raster-Versatz in ms (Latenz vor dem Plugin), > 0 = Audio kommt spaeter
    inline constexpr const char* normMode = "normMode";  // Index in der Normalize-Liste
    inline constexpr const char* trimDb  = "trimDb";
    inline constexpr const char* takeDigits = "takeDigits";
    inline constexpr const char* tooltips = "tooltips";   // "0" = aus
}

class RememberProcessor : public juce::AudioProcessor,
                          private juce::AsyncUpdater,
                          private juce::AudioProcessorValueTreeState::Listener
{
public:
    RememberProcessor();
    ~RememberProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "REMEMBER"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    RememberDSP::Engine& getEngine() { return engine; }
    RememberClip::Session& getSession() { return session; }
    RememberAudition::Player& getAudition() { return audition; }
    // Nach jeder Aenderung des Editor-Clips aufrufen: gibt eine Kopie an den Audio-Thread zum Vorhoeren
    void publishClip();
    juce::AudioProcessorValueTreeState apvts;

    static double historySecondsFor (int index);
    juce::String getProp (const char* id, const juce::String& def = {}) const;
    void setProp (const char* id, const juce::String& value);

private:
    void handleAsyncUpdate() override;
    void parameterChanged (const juce::String&, float) override { triggerAsyncUpdate(); }
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    RememberDSP::Engine engine;
    RememberClip::Session session;          // Editor-Clip: bleibt erhalten, auch wenn das Fenster geschlossen wird
    RememberAudition::Player audition;
    std::atomic<float>* pHistory = nullptr;
    double sampleRate = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RememberProcessor)
};
