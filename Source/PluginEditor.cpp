#include "PluginEditor.h"

using namespace juce;

namespace
{
    const StringArray historyNames { "1 min", "2 min", "5 min", "10 min", "20 min" };
    const StringArray snapNames { "Off", "1/16", "1/8", "Beat", "Bar" };
    const StringArray typeNames { "Synth", "Bass", "Drums", "Perc", "Vocal", "Guitar", "Keys", "FX", "Texture", "Mix", "Other" };
    const StringArray formNames { "Loop", "One-Shot", "Phrase", "Riff", "Hit", "Take" };
    const StringArray scaleNames { "Major", "Minor" };
}

RememberEditor::RememberEditor (RememberProcessor& p)
    : AudioProcessorEditor (&p), processor (p), view (p.getEngine())
{
    setLookAndFeel (&lnf);
    setWantsKeyboardFocus (true);

    addAndMakeVisible (view);
    view.setSnapMode (getChoice (ParamID::snap));
    view.setHold (processor.apvts.getRawParameterValue (ParamID::hold)->load() > 0.5f);

    for (Button* b : std::initializer_list<Button*> { &histBtn, &snapBtn, &holdBtn, &liveBtn, &fitBtn, &bar1, &bar2, &bar4, &bar8, &bar16,
                                                       &typeBtn, &formBtn, &scaleBtn, &folderBtn, &saveBtn, &dragBtn })
        addAndMakeVisible (b);
    addAndMakeVisible (projectEd);
    addAndMakeVisible (keyEd);

    histBtn.setTooltip ("How much audio is kept in memory");
    snapBtn.setTooltip ("Snap for selections. Hold Ctrl / Cmd while dragging to move freely");
    holdBtn.setTooltip ("Freeze the view, recording continues");
    liveBtn.setTooltip ("Clear start point and selection, follow the live recording");
    fitBtn.setTooltip ("Show the whole history (F)");
    bar1.setTooltip ("Select 1 bar from the start point (without a start point: the last complete bar)");
    bar2.setTooltip ("Select 2 bars"); bar4.setTooltip ("Select 4 bars"); bar8.setTooltip ("Select 8 bars"); bar16.setTooltip ("Select 16 bars");
    saveBtn.setTooltip ("Write the selection as WAV into the export folder");
    dragBtn.setTooltip ("Drag the selection as a WAV file into your DAW or the Finder");

    histBtn.onClick = [this]
    {
        showMenu (histBtn, "History", historyNames, getChoice (ParamID::history), [this] (int i) { setChoice (ParamID::history, i); });
    };
    snapBtn.onClick = [this]
    {
        showMenu (snapBtn, "Snap", snapNames, getChoice (ParamID::snap), [this] (int i) { setChoice (ParamID::snap, i); view.setSnapMode (i); });
    };
    holdBtn.onClick = [this]
    {
        const bool on = ! (processor.apvts.getRawParameterValue (ParamID::hold)->load() > 0.5f);
        if (auto* par = processor.apvts.getParameter (ParamID::hold))
        {
            par->beginChangeGesture();
            par->setValueNotifyingHost (on ? 1.0f : 0.0f);
            par->endChangeGesture();
        }
        view.setHold (on);
    };
    liveBtn.onClick = [this]
    {
        if (auto* par = processor.apvts.getParameter (ParamID::hold)) par->setValueNotifyingHost (0.0f);
        view.goLive();
    };
    fitBtn.onClick = [this] { view.zoomFit(); };
    auto barBtn = [this] (TextButton& b, int n)
    {
        b.onClick = [this, n]
        {
            view.selectBars (n);
            if (! view.hasSelection()) setStatus ("Not enough audio recorded yet, or the host sends no bar positions");
        };
    };
    barBtn (bar1, 1); barBtn (bar2, 2); barBtn (bar4, 4); barBtn (bar8, 8); barBtn (bar16, 16);

    fillTextEditor (projectEd, PropID::project, "Project");
    fillTextEditor (keyEd, PropID::key, "Key");
    typeBtn.onClick = [this]
    {
        StringArray items { "- none -" };
        items.addArray (typeNames);
        const int cur = typeNames.indexOf (processor.getProp (PropID::type)) + 1;
        showMenu (typeBtn, "Type", items, cur, [this] (int i) { processor.setProp (PropID::type, i == 0 ? String() : typeNames[i - 1]); refreshInfo(); });
    };
    formBtn.onClick = [this]
    {
        StringArray items { "- none -" };
        items.addArray (formNames);
        const int cur = formNames.indexOf (processor.getProp (PropID::form, "Loop")) + 1;
        showMenu (formBtn, "Form", items, cur, [this] (int i) { processor.setProp (PropID::form, i == 0 ? String ("-") : formNames[i - 1]); refreshInfo(); });
    };
    scaleBtn.onClick = [this]
    {
        StringArray items { "- none -" };
        items.addArray (scaleNames);
        const int cur = scaleNames.indexOf (processor.getProp (PropID::scale)) + 1;
        showMenu (scaleBtn, "Scale", items, cur, [this] (int i) { processor.setProp (PropID::scale, i == 0 ? String() : scaleNames[i - 1]); refreshInfo(); });
    };
    folderBtn.onClick = [this] { chooseFolder(); };
    saveBtn.onClick = [this] { save(); };
    dragBtn.prepareFile = [this] { return prepareDragFile(); };

    view.onChanged = [this] { refreshInfo(); };

    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) designW / (double) designH);
    setResizeLimits (640, (int) (640.0f * designH / designW), 1700, (int) (1700.0f * designH / designW));
    setSize ((int) designW, (int) designH);

    refreshInfo();
    startTimerHz (30);
}

RememberEditor::~RememberEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void RememberEditor::fillTextEditor (TextEditor& e, const char* prop, const String& hint)
{
    const auto& T = RememberColours::theme();
    e.setMultiLine (false);
    e.setReturnKeyStartsNewLine (false);
    e.setScrollbarsShown (false);
    e.setIndents (8, 0);
    e.setJustification (Justification::centredLeft);
    e.setTextToShowWhenEmpty (hint, T.textDim);
    e.setText (processor.getProp (prop), false);
    e.onTextChange = [this, &e, prop] { processor.setProp (prop, e.getText()); refreshInfo(); };
    e.onReturnKey = [&e] { e.giveAwayKeyboardFocus(); };
    e.onEscapeKey = [&e] { e.giveAwayKeyboardFocus(); };
}

int RememberEditor::getChoice (const char* id) const
{
    return roundToInt (processor.apvts.getRawParameterValue (id)->load());
}

void RememberEditor::setChoice (const char* id, int index)
{
    if (auto* par = processor.apvts.getParameter (id))
    {
        par->beginChangeGesture();
        par->setValueNotifyingHost (par->convertTo0to1 ((float) index));
        par->endChangeGesture();
    }
}

void RememberEditor::showMenu (Component& target, const String& header, const StringArray& items, int current,
                               std::function<void (int)> onPick)
{
    PopupMenu m;
    m.addSectionHeader (header);
    for (int i = 0; i < items.size(); ++i) m.addItem (i + 1, items[i], true, i == current);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&target).withMinimumWidth (jmax (140, target.getWidth())),
                     [safe = SafePointer<RememberEditor> (this), onPick] (int r)
                     {
                         if (safe != nullptr && r > 0) onPick (r - 1);
                     });
}

File RememberEditor::currentFolder() const
{
    const auto f = processor.getProp (PropID::folder);
    return f.isNotEmpty() ? File (f) : RememberExport::defaultFolder();
}

void RememberEditor::chooseFolder()
{
    chooser = std::make_unique<FileChooser> ("Export folder", currentFolder().exists() ? currentFolder() : File::getSpecialLocation (File::userMusicDirectory));
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories,
                          [safe = SafePointer<RememberEditor> (this)] (const FileChooser& fc)
                          {
                              if (safe == nullptr) return;
                              const auto f = fc.getResult();
                              if (f != File() && f.isDirectory())
                              {
                                  safe->processor.setProp (PropID::folder, f.getFullPathName());
                                  safe->refreshInfo();
                                  safe->setStatus ("Export folder: " + f.getFullPathName());
                              }
                          });
}

RememberExport::ClipMeta RememberEditor::currentMeta() const
{
    RememberExport::ClipMeta m;
    m.project = processor.getProp (PropID::project);
    m.type = processor.getProp (PropID::type);
    const auto form = processor.getProp (PropID::form, "Loop");
    m.form = form == "-" ? String() : form;
    m.key = processor.getProp (PropID::key);
    m.scale = processor.getProp (PropID::scale);
    double bpm; float bpb;
    if (view.getSelectionTempo (bpm, bpb)) m.bpm = bpm;
    else if (processor.getEngine().hostHasPpq.load()) m.bpm = processor.getEngine().lastBpm.load();
    return m;
}

void RememberEditor::setStatus (const String& s)
{
    statusText = s;
    statusTicks = 30 * 8;
    repaint();
}

void RememberEditor::refreshInfo()
{
    const auto meta = currentMeta();
    const auto stem = RememberExport::buildStem (meta);
    const int take = RememberExport::nextTake (currentFolder(), stem);

    String s;
    if (meta.bpm > 1.0) s << String (meta.bpm, std::abs (meta.bpm - std::round (meta.bpm)) < 0.05 ? 0 : 1) << " BPM";
    else s << "NO TEMPO";
    s << String::fromUTF8 ("   \xc2\xb7   ") << "TAKE " << String (take).paddedLeft ('0', 2);
    infoText = s;

    typeBtn.setButtonText ("TYPE  " + (processor.getProp (PropID::type).isEmpty() ? String ("-") : processor.getProp (PropID::type).toUpperCase()));
    const auto form = processor.getProp (PropID::form, "Loop");
    formBtn.setButtonText ("FORM  " + (form == "-" ? String ("-") : form.toUpperCase()));
    scaleBtn.setButtonText ("SCALE  " + (processor.getProp (PropID::scale).isEmpty() ? String ("-") : processor.getProp (PropID::scale).toUpperCase()));
    folderBtn.setButtonText ("FOLDER  " + currentFolder().getFileName().toUpperCase());
    folderBtn.setTooltip (currentFolder().getFullPathName());
    repaint (R (440, 452, 250, 52));
}

void RememberEditor::save()
{
    int64_t s, e;
    auto st = processor.getEngine().storage();
    if (st == nullptr || ! view.getSelection (s, e))
    {
        setStatus ("Select a range first: drag in the waveform or use a BARS button");
        return;
    }
    const auto meta = currentMeta();
    const auto stem = RememberExport::buildStem (meta);
    const auto folder = currentFolder();
    const auto file = folder.getChildFile (RememberExport::fileName (stem, RememberExport::nextTake (folder, stem)));
    const auto err = RememberExport::writeWav (*st, s, e - s, processor.getEngine().getSampleRate(), file);
    setStatus (err.isEmpty() ? "Saved  " + file.getFileName() : err);
    refreshInfo();
}

File RememberEditor::prepareDragFile()
{
    int64_t s, e;
    auto st = processor.getEngine().storage();
    if (st == nullptr || ! view.getSelection (s, e))
    {
        setStatus ("Select a range first: drag in the waveform or use a BARS button");
        return {};
    }
    const auto meta = currentMeta();
    const auto stem = RememberExport::buildStem (meta);
    const auto tmp = File::getSpecialLocation (File::tempDirectory).getChildFile ("Remember");
    const auto file = tmp.getChildFile (RememberExport::fileName (stem, RememberExport::nextTake (currentFolder(), stem)));
    const auto err = RememberExport::writeWav (*st, s, e - s, processor.getEngine().getSampleRate(), file);
    if (err.isNotEmpty()) { setStatus (err); return {}; }
    return file;
}

void RememberEditor::timerCallback()
{
    processor.getEngine().collectGarbage();

    const bool hold = processor.apvts.getRawParameterValue (ParamID::hold)->load() > 0.5f;
    if (hold != view.isHeld()) view.setHold (hold);
    view.setSnapMode (getChoice (ParamID::snap));

    histBtn.setButtonText ("HISTORY  " + historyNames[jlimit (0, 4, getChoice (ParamID::history))].toUpperCase());
    snapBtn.setButtonText ("SNAP  " + snapNames[jlimit (0, 4, getChoice (ParamID::snap))].toUpperCase());
    holdBtn.getProperties().set ("active", hold);
    liveBtn.getProperties().set ("active", view.isLive());
    const bool sel = view.hasSelection();
    saveBtn.getProperties().set ("active", sel);
    saveBtn.setEnabled (sel);
    dragBtn.setEnabled (sel);
    for (Component* b : std::initializer_list<Component*> { &histBtn, &snapBtn, &holdBtn, &liveBtn, &saveBtn, &dragBtn }) b->repaint();

    if (++infoTick % 8 == 0) refreshInfo();
    if (statusTicks > 0 && --statusTicks == 0) repaint (R (0, 508, designW, 30));
}

Rectangle<int> RememberEditor::R (float x, float y, float w, float h) const
{
    return { roundToInt (x * scale), roundToInt (y * scale), roundToInt (w * scale), roundToInt (h * scale) };
}

void RememberEditor::resized()
{
    scale = (float) getWidth() / designW;

    histBtn.setBounds (R (16, 80, 150, 40));
    snapBtn.setBounds (R (174, 80, 130, 40));
    holdBtn.setBounds (R (312, 80, 86, 40));
    liveBtn.setBounds (R (406, 80, 86, 40));
    bar1.setBounds  (R (592, 80, 52, 40));
    bar2.setBounds  (R (650, 80, 52, 40));
    bar4.setBounds  (R (708, 80, 52, 40));
    bar8.setBounds  (R (766, 80, 52, 40));
    bar16.setBounds (R (824, 80, 52, 40));
    fitBtn.setBounds (R (892, 80, 72, 40));

    view.setBounds (R (16, 130, 948, 270));

    projectEd.setBounds (R (16, 412, 250, 34));
    typeBtn.setBounds (R (274, 412, 150, 34));
    formBtn.setBounds (R (432, 412, 150, 34));
    keyEd.setBounds (R (590, 412, 90, 34));
    scaleBtn.setBounds (R (688, 412, 120, 34));

    folderBtn.setBounds (R (16, 458, 410, 44));
    saveBtn.setBounds (R (700, 458, 120, 44));
    dragBtn.setBounds (R (828, 458, 136, 44));

    const auto f = Font (rememberFont (14.0f * scale));
    projectEd.setFont (f);
    keyEd.setFont (f);
}

void RememberEditor::paint (Graphics& g)
{
    const auto& T = RememberColours::theme();
    const auto bounds = getLocalBounds().toFloat();
    g.setGradientFill (ColourGradient (T.bgTop, 0.0f, 0.0f, T.bgBottom, 0.0f, bounds.getBottom(), false));
    g.fillAll();

    // Kopfzeile wie bei den anderen Plugins: Titel gross, klein daneben "by Dani Koenig . v0.x" auf der Grundlinie
    const String byText = String::fromUTF8 ("by Dani Koenig  \xc2\xb7  v") + JucePlugin_VersionString;
    {
        const String title = "REMEMBER";
        const float titleSize = 60.0f * scale, spacing = T.titleSpacing * scale, smallSize = 11.0f * scale, gapW = 14.0f * scale;
        Font big (rememberFont (titleSize, true)), small (rememberFont (smallSize));
        float titleW = 0.0f;
        for (int i = 0; i < title.length(); ++i)
            titleW += big.getStringWidthFloat (String::charToString (title[i])) + spacing;
        titleW -= spacing;
        const float smallW = small.getStringWidthFloat (byText);
        float x = ((float) getWidth() - (titleW + gapW + smallW)) * 0.5f;

        g.setColour (T.text);
        g.setFont (big);
        for (int i = 0; i < title.length(); ++i)
        {
            const auto ch = String::charToString (title[i]);
            const float gw = big.getStringWidthFloat (ch);
            g.drawText (ch, Rectangle<float> (x, 8.0f * scale, gw + 4.0f, 62.0f * scale), Justification::centredLeft);
            x += gw + spacing;
        }
        x = x - spacing + gapW;
        g.setColour (T.textDim);
        g.setFont (small);
        g.drawText (byText, Rectangle<float> (x, 44.0f * scale, smallW + 6.0f, 16.0f * scale), Justification::centredLeft);
    }

    // Beschriftung "BARS" vor den Taktknoepfen
    g.setColour (T.textDim);
    g.setFont (Font (rememberFont (12.0f * scale, true)));
    g.drawText ("SELECT BARS", R (506, 80, 80, 40), Justification::centredRight);

    // Info rechts neben dem Ordner
    g.setColour (T.text);
    g.setFont (Font (rememberFont (13.0f * scale, true)));
    g.drawText (infoText, R (444, 458, 250, 44), Justification::centred);

    // Statuszeile
    if (statusTicks > 0)
    {
        g.setColour (T.textDim);
        g.setFont (Font (rememberFont (12.5f * scale)));
        g.drawText (statusText, R (16, 512, 948, 22), Justification::centredLeft, true);
    }
    else
    {
        g.setColour (T.textDim.withAlpha (0.8f));
        g.setFont (Font (rememberFont (11.5f * scale)));
        g.drawText (String::fromUTF8 ("Click = start point   \xc2\xb7   drag = select   \xc2\xb7   double-click = bar   \xc2\xb7   wheel = zoom   \xc2\xb7   Ctrl/Cmd = no snap   \xc2\xb7   Esc = clear"),
                    R (16, 512, 948, 22), Justification::centredLeft, true);
    }
}

void RememberEditor::paintOverChildren (Graphics& g)
{
    // Fester Rahmen wie bei den anderen Plugins. Oben eckig, unten mit dem Eckenradius der macOS-Fenster gerundet
    const auto& T = RememberColours::theme();
    const float fw = frameWidth(), cr = 10.0f * scale;
    const auto full = getLocalBounds().toFloat();
    Path ring;
    ring.addRoundedRectangle (full.getX(), full.getY(), full.getWidth(), full.getHeight(), cr, cr, false, false, true, true);
    ring.addRoundedRectangle (full.getX() + fw, full.getY() + fw, full.getWidth() - 2 * fw, full.getHeight() - 2 * fw,
                              jmax (2.0f, cr - fw), jmax (2.0f, cr - fw), false, false, true, true);
    ring.setUsingNonZeroWinding (false);
    g.setColour (T.frame);
    g.fillPath (ring);
}
