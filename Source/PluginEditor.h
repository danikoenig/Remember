#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "HistoryView.h"
#include "ClipView.h"
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

// Menue-Symbol (drei Balken) oben rechts
class MenuButton : public juce::TextButton
{
public:
    MenuButton() : TextButton ("") {}
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        TextButton::paintButton (g, over, down);
        const auto b = getLocalBounds().toFloat().reduced (getWidth() * 0.28f, getHeight() * 0.27f);
        g.setColour (juce::Colour (0xffeceef2).withAlpha (over ? 1.0f : 0.8f));
        const float h = juce::jmax (2.0f, b.getHeight() * 0.16f);
        for (int i = 0; i < 3; ++i)
            g.fillRoundedRectangle (b.getX(), b.getY() + i * (b.getHeight() - h) * 0.5f, b.getWidth(), h, h * 0.5f);
    }
};

class RememberEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit RememberEditor (RememberProcessor&);
    ~RememberEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dropping = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dropping = false; repaint(); }

private:
    void timerCallback() override;
    juce::Rectangle<int> R (float x, float y, float w, float h) const;
    void setChoice (const char* id, int index);
    int getChoice (const char* id) const;
    void showMenu (juce::Component& target, const juce::String& header, const juce::StringArray& items, int current,
                   std::function<void (int)> onPick);
    juce::File currentFolder() const;
    void chooseFolder();
    RememberExport::ClipMeta currentMeta (double bpm) const;
    double historyBpm() const;
    void refreshInfo();
    void setStatus (const juce::String& s);
    void fillTextEditor (juce::TextEditor&, const char* prop, const juce::String& hint);

    // Export
    void saveHistory();
    juce::File prepareHistoryDrag();
    void saveClip();
    juce::File prepareClipDrag();

    // Editor-Funktionen
    RememberClip::Session& S() { return processor.getSession(); }
    void afterEdit (bool refit = false);
    bool needClip();
    bool needSelection (RememberClip::Range&);
    RememberClip::Range whole() const;
    void toEditor();
    void doCopy();
    void doCut();
    void doPaste();
    void doDelete();
    void doCrop();
    void doTrim();
    void doReverse();
    void doFade (bool in);
    void doNormalize (int mode);
    void doUndo();
    void doRedo();
    void doWrap();
    void importFile (const juce::File&);
    void chooseImport();
    void showClipMenu();
    void showSettingsMenu();
    int fadeLengthMs() const;
    int normMode() const;
    float trimThresholdDb() const;
    void applyTooltipSetting();
    RememberClip::FadeShape fadeShape() const;
    void showKeyMenu();
    void doClear();
    void doPlay();
    bool handleKey (const juce::KeyPress&);

    static constexpr float designW = 980.0f, designH = 730.0f;
    float frameWidth() const noexcept { return 4.0f * scale; }

    RememberProcessor& processor;
    RememberLookAndFeel lnf;
    HistoryView view;
    ClipView clipView;

    // Kopf: History
    juce::TextButton histBtn, snapBtn, holdBtn { "HOLD" }, liveBtn { "LIVE" }, fitBtn { "FIT" };
    juce::TextButton bar1 { "1" }, bar2 { "2" }, bar4 { "4" }, bar8 { "8" }, bar16 { "16" };
    juce::TextButton toEditorBtn { "TO EDITOR" };
    juce::TextButton saveHistBtn { "SAVE" };
    DragButton dragHistBtn { "DRAG WAV" };

    // Editor
    juce::TextButton trimBtn { "TRIM" }, reverseBtn { "REVERSE" }, normBtn { "NORMALIZE" }, clearBtn { "CLEAR" }, undoBtn { "UNDO" }, redoBtn { "REDO" };
    juce::TextButton fadeInBtn { "FADE IN" }, fadeOutBtn { "FADE OUT" };
    MenuButton menuBtn;
    juce::TextButton playBtn { "PLAY" }, loopBtn { "LOOP" }, wrapBtn { "FOLD TAIL" }, importBtn { "IMPORT" };
    juce::TextButton saveClipBtn { "SAVE" };
    DragButton dragClipBtn { "DRAG WAV" };

    // Metadaten und Ordner
    juce::TextEditor projectEd;
    juce::TextButton keyBtn, nameLoopBtn { "+ LOOP" };
    bool hintEditor = false;

    juce::TooltipWindow tooltip { this, 700 };
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String infoText, statusText;
    int statusTicks = 0, infoTick = 0;
    bool dropping = false;
    float scale = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RememberEditor)
};
