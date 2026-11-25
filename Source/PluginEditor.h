#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "HistoryView.h"
#include "Look.h"
#include "Export.h"

// Button, der beim Ziehen die Datei an die DAW oder den Finder uebergibt
class DragButton : public juce::TextButton
{
public:
    using TextButton::TextButton;
    std::function<juce::File()> prepareFile;   // schreibt die WAV und gibt sie zurueck (leer = nichts zu ziehen)
    void mouseDrag (const juce::MouseEvent& e) override
    {
        TextButton::mouseDrag (e);
        if (! started && e.getDistanceFromDragStart() > 6 && prepareFile)
        {
            started = true;
            const auto f = prepareFile();
            if (f.existsAsFile())
                juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this);
        }
    }
    void mouseUp (const juce::MouseEvent& e) override { started = false; TextButton::mouseUp (e); }
private:
    bool started = false;
};

class RememberEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RememberEditor (RememberProcessor&);
    ~RememberEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    juce::Rectangle<int> R (float x, float y, float w, float h) const;
    void setChoice (const char* id, int index);
    int getChoice (const char* id) const;
    void showMenu (juce::Component& target, const juce::String& header, const juce::StringArray& items, int current,
                   std::function<void (int)> onPick);
    juce::File currentFolder() const;
    void chooseFolder();
    RememberExport::ClipMeta currentMeta() const;
    void refreshInfo();
    void save();
    juce::File prepareDragFile();
    void setStatus (const juce::String& s);
    void fillTextEditor (juce::TextEditor&, const char* prop, const juce::String& hint);

    static constexpr float designW = 980.0f, designH = 548.0f;
    float frameWidth() const noexcept { return 4.0f * scale; }

    RememberProcessor& processor;
    RememberLookAndFeel lnf;
    HistoryView view;

    juce::TextButton histBtn, snapBtn, holdBtn { "HOLD" }, liveBtn { "LIVE" }, fitBtn { "FIT" };
    juce::TextButton bar1 { "1" }, bar2 { "2" }, bar4 { "4" }, bar8 { "8" }, bar16 { "16" };
    juce::TextEditor projectEd, keyEd;
    juce::TextButton typeBtn, formBtn, scaleBtn, folderBtn;
    juce::TextButton saveBtn { "SAVE" };
    DragButton dragBtn { "DRAG WAV" };

    juce::TooltipWindow tooltip { this, 700 };
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String infoText, statusText;
    int statusTicks = 0, infoTick = 0;
    float scale = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RememberEditor)
};
