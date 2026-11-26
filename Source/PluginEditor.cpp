#include "PluginEditor.h"
#include "Align.h"

using namespace juce;

namespace
{
    const StringArray historyNames { "1 min", "2 min", "5 min", "10 min", "20 min" };
    const StringArray snapNames { "Off", "1/16", "1/8", "Beat", "Bar" };
    const StringArray noteNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
}

static const char* const normNames[] { "Peak -0.1 dB", "Peak -1 dB", "-14 LUFS", "-16 LUFS", "-18 LUFS", "-23 LUFS", "-18 dB RMS", "-20 dB RMS" };

RememberEditor::RememberEditor (RememberProcessor& p)
    : AudioProcessorEditor (&p), processor (p), view (p.getEngine()), clipView (p)
{
    setLookAndFeel (&lnf);
    setWantsKeyboardFocus (true);

    addAndMakeVisible (view);
    addAndMakeVisible (clipView);
    clipView.setSnapMode (getChoice (ParamID::snap));
    clipView.onChanged = [this] { refreshInfo(); };
    clipView.onKey = [this] (const KeyPress& k) { return handleKey (k); };
    view.setSnapMode (getChoice (ParamID::snap));
    view.setHold (processor.apvts.getRawParameterValue (ParamID::hold)->load() > 0.5f);

    for (Button* b : std::initializer_list<Button*> { &histBtn, &snapBtn, &holdBtn, &liveBtn, &fitBtn, &bar1, &bar2, &bar4, &bar8, &bar16,
                                                       &keyBtn, &nameLoopBtn, &alignDot, &menuBtn, &toEditorBtn, &saveHistBtn, &dragHistBtn,
                                                       &trimBtn, &reverseBtn, &fadeInBtn, &fadeOutBtn, &normBtn, &clearBtn, &undoBtn, &redoBtn, &playBtn, &loopBtn, &wrapBtn, &importBtn, &saveClipBtn, &dragClipBtn })
        addAndMakeVisible (b);
    loopBtn.setClickingTogglesState (true);
    addAndMakeVisible (projectEd);

    histBtn.setTooltip ("How much audio is kept in memory");
    snapBtn.setTooltip ("Snap for selections. Hold Ctrl / Cmd while dragging to move freely");
    holdBtn.setTooltip ("Freeze the view, recording continues");
    liveBtn.setTooltip ("Clear start point and selection, follow the live recording");
    fitBtn.setTooltip ("Show the whole history (F)");
    bar1.setTooltip ("Select 1 bar from the start point (without a start point: the last complete bar)");
    bar2.setTooltip ("Select 2 bars"); bar4.setTooltip ("Select 4 bars"); bar8.setTooltip ("Select 8 bars"); bar16.setTooltip ("Select 16 bars");
    saveHistBtn.setTooltip ("Write the history selection as WAV into the export folder");
    dragHistBtn.setTooltip ("Drag the history selection as a WAV file into your DAW or the Finder");
    toEditorBtn.setTooltip ("Copy the history selection into the editor below");
    reverseBtn.setTooltip ("Reverse the selection, or the whole clip");
    fadeInBtn.setTooltip ("Fade in over the selection (without a selection: over the fade length at the start). Length and shape: menu top right");
    fadeOutBtn.setTooltip ("Fade out over the selection (without a selection: over the fade length at the end). Length and shape: menu top right");
    menuBtn.setTooltip ("Settings: export folder, fade length and shape, bit depth");
        clearBtn.setTooltip ("Empty the whole editor so you can load something new (Undo brings it back)");
    keyBtn.setTooltip ("Key and scale, written into the file name");
    nameLoopBtn.setButtonText ("LOOP");
    nameLoopBtn.toFront (false);
    nameLoopBtn.setTooltip ("On: the word Loop is written into the file name, and TRIM keeps the loop length (see TRIM)");
    alignDot.setButtonText ("ALIGN TO KICK");
    alignDot.setTooltip ("On: TO EDITOR, SAVE and DRAG WAV from the history start on the first kick (corrects plugin latency). Off for pads and sounds without a kick on the beats");
    trimBtn.setTooltip ("Cut the silence at the start and end. With + LOOP on: only the start is cut and moved to the end, so the loop keeps its bar length");
    undoBtn.setTooltip ("Undo (Ctrl/Cmd+Z)");
    redoBtn.setTooltip ("Redo (Ctrl/Cmd+Shift+Z)");
    playBtn.setTooltip ("Listen to the editor through the plugin output (Space). Starts at the green marker");
    loopBtn.setTooltip ("Loop the playback");
    wrapBtn.setTooltip ("Select the tail (the reverb after the loop) and press FOLD TAIL: the whole tail is crossfaded over the start of the clip and the clip ends exactly where the selection began");
    importBtn.setTooltip ("Load a WAV, AIFF or FLAC file into the editor. You can also drop a file onto the plugin");
    saveClipBtn.setTooltip ("Write the edited clip as WAV into the export folder");
    dragClipBtn.setTooltip ("Drag the edited clip as a WAV file into your DAW or the Finder");

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

    fillTextEditor (projectEd, PropID::project, "NAME");
    projectEd.setTooltip ("File name (project name). BPM, key and take number are added automatically");
    keyBtn.onClick = [this] { showKeyMenu(); };
    nameLoopBtn.setClickingTogglesState (true);
    nameLoopBtn.setToggleState (processor.getProp (PropID::form, "Loop") != "-", dontSendNotification);
    nameLoopBtn.onClick = [this]
    {
        processor.setProp (PropID::form, nameLoopBtn.getToggleState() ? String ("Loop") : String ("-"));
        refreshInfo();
    };
    alignDot.setClickingTogglesState (true);
    alignDot.setToggleState (processor.getProp (PropID::autoAlign, "1") != "0", dontSendNotification);
    alignDot.onClick = [this]
    {
        processor.setProp (PropID::autoAlign, alignDot.getToggleState() ? "1" : "0");
        syncGridShift();
        setStatus (alignDot.getToggleState() ? "Align to kick on" : "Align to kick off: exports start exactly on the host grid");
    };
    menuBtn.onClick = [this] { showSettingsMenu(); };
    RememberExport::bitDepth() = processor.getProp (PropID::bits, "24").getIntValue();
    RememberExport::takeDigits() = jlimit (2, 4, processor.getProp (PropID::takeDigits, "2").getIntValue());
    applyTooltipSetting();
    syncGridShift();
    saveHistBtn.onClick = [this] { saveHistory(); };
    dragHistBtn.prepareFile = [this] { return prepareHistoryDrag(); };
    saveClipBtn.onClick = [this] { saveClip(); };
    dragClipBtn.prepareFile = [this] { return prepareClipDrag(); };

    toEditorBtn.onClick = [this] { toEditor(); };
    trimBtn.onClick = [this] { doTrim(); };
    reverseBtn.onClick = [this] { doReverse(); };
    fadeInBtn.onClick = [this] { doFade (true); };
    fadeOutBtn.onClick = [this] { doFade (false); };
    clipView.onContext = [this] { showClipMenu(); };
    normBtn.onClick = [this] { doNormalize (normMode()); };
    clearBtn.onClick = [this] { doClear(); };
    undoBtn.onClick = [this] { doUndo(); };
    redoBtn.onClick = [this] { doRedo(); };
    playBtn.onClick = [this] { doPlay(); };
    wrapBtn.onClick = [this] { doWrap(); };
    importBtn.onClick = [this] { chooseImport(); };
    loopBtn.onClick = [this] { processor.getAudition().setLoop (loopBtn.getToggleState()); };
    processor.getAudition().setLoop (false);

    view.onChanged = [this] { refreshInfo(); };

    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) designW / (double) designH);
    setResizeLimits (640, (int) (640.0f * designH / designW), 1700, (int) (1700.0f * designH / designW));
    setSize ((int) designW, (int) designH);

    if (S().hasClip()) clipView.refresh (true);
    refreshInfo();
    startTimerHz (30);
}

RememberEditor::~RememberEditor()
{
    stopTimer();
    processor.getAudition().stop();
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

double RememberEditor::historyBpm() const
{
    double bpm; float bpb;
    if (view.getSelectionTempo (bpm, bpb)) return bpm;
    if (processor.getEngine().hostHasPpq.load()) return processor.getEngine().lastBpm.load();
    return 0.0;
}

RememberExport::ClipMeta RememberEditor::currentMeta (double bpm) const
{
    RememberExport::ClipMeta m;
    m.project = processor.getProp (PropID::project);
    const auto form = processor.getProp (PropID::form, "Loop");
    m.form = form == "-" ? String() : form;
    m.key = processor.getProp (PropID::key);
    m.scale = processor.getProp (PropID::scale);
    m.bpm = bpm;
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
    const double bpm = S().hasClip() ? S().clip.bpm : historyBpm();
    const auto meta = currentMeta (bpm);
    const auto stem = RememberExport::buildStem (meta);
    const int take = RememberExport::nextTake (currentFolder(), stem);

    String s;
    if (meta.bpm > 1.0) s << String (meta.bpm, std::abs (meta.bpm - std::round (meta.bpm)) < 0.05 ? 0 : 1) << " BPM";
    else s << "NO TEMPO";
    s << String::fromUTF8 ("   \xc2\xb7   ") << "TAKE " << String (take).paddedLeft ('0', RememberExport::takeDigits());
    infoText = s;

    const auto key = processor.getProp (PropID::key), scale = processor.getProp (PropID::scale);
    keyBtn.setButtonText (key.isEmpty() ? String ("KEY") : key.toUpperCase() + (scale == "Minor" ? " MIN" : scale == "Major" ? " MAJ" : ""));
    normBtn.setTooltip (String ("Normalize the whole clip to ") + normNames[normMode()] + ". Change the target: menu top right");
    menuBtn.setTooltip ("Settings. Export folder: " + currentFolder().getFullPathName());
    repaint (R (16, 352, 520, 40));
}

String RememberEditor::shiftNote (int64_t samples) const
{
    if (samples == 0) return {};
    return "  (aligned to the first kick, " + String ((double) samples * 1000.0 / processor.getEngine().getSampleRate(), 1) + " ms)";
}

void RememberEditor::saveHistory()
{
    int64_t s, e;
    auto st = processor.getEngine().storage();
    if (st == nullptr || ! view.getSelection (s, e))
    {
        setStatus ("Select a range in the history first: drag in the waveform or use a BARS button");
        return;
    }
    const auto stem = RememberExport::buildStem (currentMeta (historyBpm()));
    const auto folder = currentFolder();
    const auto file = folder.getChildFile (RememberExport::fileName (stem, RememberExport::nextTake (folder, stem)));
    const auto sh = view.exportShift (s, e);
    const auto err = RememberExport::writeWav (*st, s + sh, e - s, processor.getEngine().getSampleRate(), file);
    setStatus (err.isEmpty() ? "Saved  " + file.getFileName() + shiftNote (sh) : err);
    refreshInfo();
}

File RememberEditor::prepareHistoryDrag()
{
    int64_t s, e;
    auto st = processor.getEngine().storage();
    if (st == nullptr || ! view.getSelection (s, e))
    {
        setStatus ("Select a range in the history first: drag in the waveform or use a BARS button");
        return {};
    }
    const auto stem = RememberExport::buildStem (currentMeta (historyBpm()));
    const auto tmp = File::getSpecialLocation (File::tempDirectory).getChildFile ("Remember");
    const auto file = tmp.getChildFile (RememberExport::fileName (stem, RememberExport::nextTake (currentFolder(), stem)));
    const auto sh = view.exportShift (s, e);
    const auto err = RememberExport::writeWav (*st, s + sh, e - s, processor.getEngine().getSampleRate(), file);
    if (err.isNotEmpty()) { setStatus (err); return {}; }
    if (sh != 0) setStatus ("Dragging" + shiftNote (sh));
    return file;
}

void RememberEditor::saveClip()
{
    if (! needClip()) return;
    const auto stem = RememberExport::buildStem (currentMeta (S().clip.bpm));
    const auto folder = currentFolder();
    const auto file = folder.getChildFile (RememberExport::fileName (stem, RememberExport::nextTake (folder, stem)));
    const auto err = RememberExport::writeClipWav (S().clip, file);
    setStatus (err.isEmpty() ? "Saved  " + file.getFileName() : err);
    refreshInfo();
}

File RememberEditor::prepareClipDrag()
{
    if (! needClip()) return {};
    const auto stem = RememberExport::buildStem (currentMeta (S().clip.bpm));
    const auto tmp = File::getSpecialLocation (File::tempDirectory).getChildFile ("Remember");
    const auto file = tmp.getChildFile (RememberExport::fileName (stem, RememberExport::nextTake (currentFolder(), stem)));
    const auto err = RememberExport::writeClipWav (S().clip, file);
    if (err.isNotEmpty()) { setStatus (err); return {}; }
    return file;
}

// ---------------------------------------------------------------------------------------------
// Editor

bool RememberEditor::needClip()
{
    if (S().hasClip()) return true;
    setStatus ("The editor is empty. Select audio in the history and press TO EDITOR");
    return false;
}

bool RememberEditor::needSelection (RememberClip::Range& r)
{
    if (! needClip()) return false;
    if (clipView.getSelection (r)) return true;
    setStatus ("Select a range in the editor first");
    return false;
}

RememberClip::Range RememberEditor::whole() const
{
    RememberClip::Range r;
    if (! clipView.getSelection (r)) r = { 0, processor.getSession().clip.length() };
    return r;
}

void RememberEditor::afterEdit (bool refit)
{
    S().touch();
    processor.publishClip();
    clipView.refresh (refit);
    refreshInfo();
}

void RememberEditor::toEditor()
{
    RememberClip::Clip c;
    if (! view.grabClip (c))
    {
        setStatus ("Select a range in the history first: drag in the waveform or use a BARS button");
        return;
    }
    processor.getAudition().stop();
    S().load (std::move (c));
    clipView.clearSelection();
    clipView.setMarker (-1);
    afterEdit (true);
    setStatus (String ((double) S().clip.length() / S().clip.sampleRate, 2) + " s copied into the editor" + shiftNote (view.lastExportShift()));
}

void RememberEditor::doCopy()
{
    RememberClip::Range r;
    if (! needSelection (r)) return;
    S().clipboard = RememberClip::extract (S().clip, r);
    setStatus ("Copied " + String ((double) r.length() / S().clip.sampleRate, 2) + " s");
}

void RememberEditor::doCut()
{
    RememberClip::Range r;
    if (! needSelection (r)) return;
    S().beginEdit();
    S().clipboard = RememberClip::extract (S().clip, r);
    RememberClip::deleteRange (S().clip, r);
    clipView.clearSelection();
    clipView.setMarker (r.a);
    afterEdit();
}

void RememberEditor::doPaste()
{
    if (! needClip()) return;
    if (S().clipboard.empty() || S().clipboard[0].empty() || (int) S().clipboard.size() != S().clip.numChannels())
    {
        setStatus ("Nothing to paste. Copy or cut a range first");
        return;
    }
    RememberClip::Range r;
    int64_t pos = S().clip.length();
    if (clipView.getMarker() >= 0) pos = clipView.getMarker();
    else if (clipView.getSelection (r)) pos = r.a;
    pos = jlimit<int64_t> (0, S().clip.length(), pos);
    S().beginEdit();
    RememberClip::insert (S().clip, pos, S().clipboard);
    const int64_t n = (int64_t) S().clipboard[0].size();
    afterEdit();
    clipView.setSelection (pos, pos + n);
}

void RememberEditor::doDelete()
{
    RememberClip::Range r;
    if (! needSelection (r)) return;
    S().beginEdit();
    RememberClip::deleteRange (S().clip, r);
    clipView.clearSelection();
    clipView.setMarker (r.a);
    afterEdit();
}

void RememberEditor::doCrop()
{
    RememberClip::Range r;
    if (! needSelection (r)) return;
    S().beginEdit();
    RememberClip::crop (S().clip, r);
    clipView.clearSelection();
    clipView.setMarker (-1);
    afterEdit (true);
}

void RememberEditor::doTrim()
{
    if (! needClip()) return;
    if (RememberClip::findNonSilent (S().clip, trimThresholdDb()).empty())
    {
        setStatus ("The clip is silent, nothing to trim");
        return;
    }
    S().beginEdit();
    if (nameLoopBtn.getToggleState())
    {
        // Loop: nur den Anfang bis zum ersten Schlag abschneiden und hinten wieder anhaengen, die Laenge (Takte) bleibt
        const double sr = S().clip.sampleRate;
        const auto first = RememberClip::findNonSilent (S().clip, trimThresholdDb());
        if (! RememberClip::trimLoopStart (S().clip, trimThresholdDb()))
        {
            S().undo();
            setStatus ("The loop already starts on the first sound, nothing to trim");
            return;
        }
        setStatus ("Loop start trimmed by " + String ((double) first.a * 1000.0 / sr, 1) + " ms and moved to the end, the loop keeps its length");
    }
    else
        RememberClip::trimSilence (S().clip, trimThresholdDb());
    S().clip.gridOffset = 0.0;      // Takt 1 liegt auf dem neuen Anfang, sonst bleibt am Loop-Beginn ein Rest Stille im Raster
    clipView.clearSelection();
    clipView.setMarker (-1);
    afterEdit (true);
}

void RememberEditor::doReverse()
{
    if (! needClip()) return;
    S().beginEdit();
    RememberClip::reverse (S().clip, whole());
    afterEdit();
}

void RememberEditor::doFade (bool in)
{
    if (! needClip()) return;
    RememberClip::Range r;
    if (! clipView.getSelection (r))
    {
        const int64_t n = jmin (S().clip.length(), (int64_t) std::llround (S().clip.sampleRate * fadeLengthMs() / 1000.0));
        r = in ? RememberClip::Range { 0, n } : RememberClip::Range { S().clip.length() - n, S().clip.length() };
    }
    S().beginEdit();
    if (in) RememberClip::fadeIn (S().clip, r, fadeShape()); else RememberClip::fadeOut (S().clip, r, fadeShape());
    afterEdit();
}

void RememberEditor::doNormalize (int mode)
{
    if (! needClip()) return;
    static const double targets[] { -0.1, -1.0, -14.0, -16.0, -18.0, -23.0, -18.0, -20.0 };
    if (mode < 0 || mode > 7) return;
    S().beginEdit();
    const auto r = whole();
    const auto res = mode < 2 ? RememberClip::normalizePeak (S().clip, r, targets[mode])
                   : mode < 6 ? RememberClip::normalizeLufs (S().clip, r, targets[mode], -0.1)
                              : RememberClip::normalizeRms (S().clip, r, targets[mode], -0.1);
    if (! res.ok)
    {
        S().undo();
        setStatus ("Nothing to normalize (silence)");
        return;
    }
    afterEdit();
    setStatus ("Gain " + String (res.gainDb, 1) + " dB" + (res.limited ? "  (limited by the peak ceiling -0.1 dBFS)" : ""));
}

void RememberEditor::doUndo()
{
    if (S().undo()) { clipView.clearSelection(); afterEdit (true); }
}

void RememberEditor::doRedo()
{
    if (S().redo()) { clipView.clearSelection(); afterEdit (true); }
}

void RememberEditor::doWrap()
{
    RememberClip::Range r;
    if (! needSelection (r)) return;
    if (r.length() >= S().clip.length())
    {
        setStatus ("Select only the tail, not the whole clip");
        return;
    }
    S().beginEdit();
    RememberClip::wrap (S().clip, r);
    clipView.clearSelection();
    clipView.setMarker (-1);
    afterEdit (true);
    setStatus ("Tail folded: " + String ((double) r.length() / S().clip.sampleRate, 2) + " s tail crossfaded over the start");
}

bool RememberEditor::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files)
        if (File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg")) return true;
    return false;
}

void RememberEditor::filesDropped (const StringArray& files, int, int)
{
    dropping = false;
    repaint();
    if (files.size() > 0) importFile (File (files[0]));
}

void RememberEditor::chooseImport()
{
    chooser = std::make_unique<FileChooser> ("Import audio", currentFolder().exists() ? currentFolder() : File::getSpecialLocation (File::userMusicDirectory),
                                             "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                          [safe = SafePointer<RememberEditor> (this)] (const FileChooser& fc)
                          {
                              if (safe != nullptr && fc.getResult().existsAsFile()) safe->importFile (fc.getResult());
                          });
}

void RememberEditor::importFile (const File& f)
{
    AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<AudioFormatReader> reader (fm.createReaderFor (f));
    if (reader == nullptr || reader->lengthInSamples <= 0) { setStatus ("Cannot read " + f.getFileName()); return; }
    const double maxSeconds = 20.0 * 60.0;
    if ((double) reader->lengthInSamples / reader->sampleRate > maxSeconds) { setStatus ("File is longer than 20 minutes"); return; }

    const int n = (int) reader->lengthInSamples;
    AudioBuffer<float> src (2, n);
    reader->read (&src, 0, n, 0, true, true);
    if (reader->numChannels == 1) src.copyFrom (1, 0, src, 0, 0, n);

    const double hostRate = processor.getEngine().getSampleRate() > 0.0 ? processor.getEngine().getSampleRate() : reader->sampleRate;
    RememberClip::Clip c;
    c.sampleRate = hostRate;
    if (std::abs (hostRate - reader->sampleRate) < 0.5)
    {
        c.ch.resize (2);
        for (int ch = 0; ch < 2; ++ch) c.ch[(size_t) ch].assign (src.getReadPointer (ch), src.getReadPointer (ch) + n);
    }
    else
    {
        const double ratio = reader->sampleRate / hostRate;
        const int outN = (int) std::floor ((double) n / ratio);
        c.ch.assign (2, std::vector<float> ((size_t) outN));
        for (int ch = 0; ch < 2; ++ch)
        {
            LagrangeInterpolator li;
            li.process (ratio, src.getReadPointer (ch), c.ch[(size_t) ch].data(), outN);
        }
    }
    // Tempo vom Host (falls bekannt): Raster mit Taktanfang am Dateianfang
    const double bpm = historyBpm();
    if (bpm > 1.0) { c.bpm = bpm; c.gridOffset = 0.0; }

    processor.getAudition().stop();
    S().load (std::move (c));
    clipView.clearSelection();
    clipView.setMarker (-1);
    afterEdit (true);
    setStatus ("Imported  " + f.getFileName() + (bpm > 1.0 ? "  (grid from the host tempo, bar 1 = file start)" : ""));
}

int RememberEditor::normMode() const
{
    return jlimit (0, 7, processor.getProp (PropID::normMode, "4").getIntValue());
}

float RememberEditor::trimThresholdDb() const
{
    return (float) jlimit (-90, -20, processor.getProp (PropID::trimDb, "-60").getIntValue());
}

void RememberEditor::applyTooltipSetting()
{
    tooltip.setMillisecondsBeforeTipAppears (processor.getProp (PropID::tooltips, "1") == "0" ? 100000000 : 700);
}

int RememberEditor::fadeLengthMs() const
{
    return jlimit (1, 5000, processor.getProp (PropID::fadeMs, "25").getIntValue());
}

RememberClip::FadeShape RememberEditor::fadeShape() const
{
    return (RememberClip::FadeShape) jlimit (0, 3, processor.getProp (PropID::fadeShape, "0").getIntValue());
}

void RememberEditor::syncGridShift()
{
    auto& eng = processor.getEngine();
    const int64_t want = (int64_t) std::llround (processor.getProp (PropID::gridMs, "0").getDoubleValue() * eng.getSampleRate() / 1000.0);
    if (want != eng.getGridShift()) eng.setGridShift (want);
    const bool al = processor.getProp (PropID::autoAlign, "1") != "0";
    view.setAutoAlign (al);
    if (alignDot.getToggleState() != al) alignDot.setToggleState (al, dontSendNotification);
}

void RememberEditor::calibrateGrid()
{
    auto& eng = processor.getEngine();
    int64_t s, e;
    auto st = eng.storage();
    if (st == nullptr || ! view.getSelection (s, e))
    {
        setStatus ("Select 2 or more bars with a steady kick in the history first, then align the grid");
        return;
    }
    const double sr = eng.getSampleRate();
    std::vector<RememberDSP::Meta> m;
    st->readMeta (s, e, m);
    double bpm = 0.0;
    float bpb = 4.0f;
    if (! RememberDSP::tempoAt (m, s, bpm, bpb) || bpm < 20.0)
    {
        setStatus ("The host sends no tempo for this range, cannot align");
        return;
    }
    std::vector<RememberDSP::GridPoint> g;
    RememberDSP::gridFromMeta (m, st->writtenNow(), sr, s, e, 1.0, g);
    const double beatSec = 60.0 / bpm;
    const int64_t pad = (int64_t) (0.35 * beatSec * sr);
    const int64_t a = jmax (s - pad, st->oldestValid()), z = jmin (e + pad, st->writtenNow());
    if (g.size() < 2 || z - a < (int64_t) (0.5 * sr))
    {
        setStatus ("The selection is too short. Select 2 or more bars");
        return;
    }
    std::vector<float> l ((size_t) (z - a)), r ((size_t) (z - a));
    float* dst[2] { l.data(), r.data() };
    if (! st->copyRange (a, z - a, dst, 2)) { setStatus ("Cannot read this range"); return; }
    for (size_t i = 0; i < l.size(); ++i) l[i] = 0.5f * (l[i] + r[i]);
    std::vector<int64_t> beats;
    for (const auto& p : g) beats.push_back (p.sample - a);

    const auto res = RememberAlign::estimateGridOffset (l, sr, beats, beatSec);
    if (! res.ok)
    {
        setStatus ("No clear beat found. Select a part with a steady kick on the beats (2 or more bars)");
        return;
    }
    const int64_t total = eng.getGridShift() + res.offsetSamples;
    const double ms = (double) total * 1000.0 / sr;
    processor.setProp (PropID::gridMs, String (ms, 2));
    eng.setGridShift (total);
    view.clearSelection();
    view.repaint();
    refreshInfo();
    setStatus ("Grid aligned, offset " + String (ms, 1) + " ms (" + String (res.hits) + " beats, spread " + String (res.spreadMs, 1) + " ms). Select the bars again");
}

void RememberEditor::showSettingsMenu()
{
    static const int lengths[] { 5, 10, 25, 50, 100, 250, 500 };
    static const char* shapes[] { "Smooth (S-curve)", "Linear", "Equal power", "Exponential" };
    static const int depths[] { 16, 24, 32 };

    static const int trims[] { -80, -70, -60, -50, -40, -30 };
    static const int digitsList[] { 2, 3, 4 };

    PopupMenu m, len, shp, bits, nrm, trm, dig;
    for (int i = 0; i < 7; ++i) len.addItem (100 + i, String (lengths[i]) + " ms", true, lengths[i] == fadeLengthMs());
    for (int i = 0; i < 4; ++i) shp.addItem (200 + i, shapes[i], true, i == (int) fadeShape());
    for (int i = 0; i < 3; ++i) bits.addItem (300 + i, String (depths[i]) + (depths[i] == 32 ? " bit float" : " bit"), true, depths[i] == RememberExport::bitDepth());
    for (int i = 0; i < 8; ++i) nrm.addItem (400 + i, normNames[i], true, i == normMode());
    for (int i = 0; i < 6; ++i) trm.addItem (500 + i, String (trims[i]) + " dB", true, trims[i] == (int) trimThresholdDb());
    for (int i = 0; i < 3; ++i) dig.addItem (600 + i, "T" + String (1).paddedLeft ('0', digitsList[i]) + "  (" + String (digitsList[i]) + " digits)", true, digitsList[i] == RememberExport::takeDigits());

    m.addSectionHeader ("Export");
    m.addItem (1, "Export folder...  " + currentFolder().getFileName());
    m.addItem (2, "Reset folder to default", processor.getProp (PropID::folder).isNotEmpty());
    m.addSubMenu ("WAV bit depth", bits);
    m.addSubMenu ("Take number in file name", dig);
    m.addSeparator();
    m.addSectionHeader ("Timing  (grid offset " + String (processor.getProp (PropID::gridMs, "0").getDoubleValue(), 1) + " ms)");
    m.addItem (8, "Align grid to selection (steady kick, 2+ bars)", view.hasSelection());
    m.addItem (10, "Auto-align exports to the first kick", true, processor.getProp (PropID::autoAlign, "1") != "0");
    m.addItem (9, "Reset grid offset", processor.getProp (PropID::gridMs, "0").getDoubleValue() != 0.0);
    m.addSeparator();
    m.addSectionHeader ("Editing");
    m.addSubMenu ("Normalize target", nrm);
    m.addSubMenu ("Trim threshold", trm);
    m.addSubMenu ("Fade length", len);
    m.addSubMenu ("Fade shape", shp);
    m.addSeparator();
    m.addItem (7, "Show tooltips", true, processor.getProp (PropID::tooltips, "1") != "0");

    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&menuBtn).withMinimumWidth (260),
                     [safe = SafePointer<RememberEditor> (this)] (int r)
                     {
                         if (safe == nullptr || r <= 0) return;
                         auto& p = safe->processor;
                         if (r == 1) safe->chooseFolder();
                         else if (r == 2) { p.setProp (PropID::folder, {}); safe->refreshInfo(); safe->setStatus ("Export folder: default"); }
                         else if (r >= 100 && r < 107) { p.setProp (PropID::fadeMs, String (lengths[r - 100])); safe->setStatus ("Fade length " + String (lengths[r - 100]) + " ms"); }
                         else if (r >= 200 && r < 204) { p.setProp (PropID::fadeShape, String (r - 200)); safe->setStatus (String ("Fade shape: ") + shapes[r - 200]); }
                         else if (r == 8) safe->calibrateGrid();
                         else if (r == 10) { p.setProp (PropID::autoAlign, p.getProp (PropID::autoAlign, "1") == "0" ? "1" : "0"); safe->syncGridShift(); safe->setStatus (p.getProp (PropID::autoAlign, "1") == "0" ? "Auto-align off" : "Auto-align on"); }
                         else if (r == 9) { p.setProp (PropID::gridMs, "0"); safe->syncGridShift(); safe->view.clearSelection(); safe->view.repaint(); safe->setStatus ("Grid offset reset to 0 ms"); }
                         else if (r == 7) { p.setProp (PropID::tooltips, p.getProp (PropID::tooltips, "1") == "0" ? "1" : "0"); safe->applyTooltipSetting(); }
                         else if (r >= 400 && r < 408) { p.setProp (PropID::normMode, String (r - 400)); safe->refreshInfo(); safe->setStatus (String ("Normalize target: ") + normNames[r - 400]); }
                         else if (r >= 500 && r < 506) { p.setProp (PropID::trimDb, String (trims[r - 500])); safe->setStatus ("Trim threshold " + String (trims[r - 500]) + " dB"); }
                         else if (r >= 600 && r < 603) { RememberExport::takeDigits() = digitsList[r - 600]; p.setProp (PropID::takeDigits, String (digitsList[r - 600])); safe->refreshInfo(); safe->setStatus ("Take number digits: " + String (digitsList[r - 600])); }
                         else if (r >= 300 && r < 303) { RememberExport::bitDepth() = depths[r - 300]; p.setProp (PropID::bits, String (depths[r - 300])); safe->setStatus ("WAV bit depth " + String (depths[r - 300])); }
                     });
}

void RememberEditor::showKeyMenu()
{
    const auto key = processor.getProp (PropID::key), scale = processor.getProp (PropID::scale);
    PopupMenu m;
    m.addSectionHeader ("Key");
    m.addItem (1000, "- none -", true, key.isEmpty());
    for (int i = 0; i < 12; ++i)
    {
        m.addItem (1 + i, noteNames[i], true, key == noteNames[i] && scale != "Minor");
        m.addItem (101 + i, noteNames[i] + " minor", true, key == noteNames[i] && scale == "Minor");
    }
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&keyBtn).withMinimumWidth (jmax (140, keyBtn.getWidth())),
                     [safe = SafePointer<RememberEditor> (this)] (int r)
                     {
                         if (safe == nullptr || r <= 0) return;
                         if (r == 1000) { safe->processor.setProp (PropID::key, {}); safe->processor.setProp (PropID::scale, {}); }
                         else
                         {
                             const bool minor = r > 100;
                             safe->processor.setProp (PropID::key, noteNames[(r - 1) % 100]);
                             safe->processor.setProp (PropID::scale, minor ? "Minor" : "Major");
                         }
                         safe->refreshInfo();
                     });
}

void RememberEditor::showClipMenu()
{
    const bool sel = clipView.hasSelection(), paste = ! S().clipboard.empty();
    PopupMenu m;
    m.addItem (1, "Copy", sel);
    m.addItem (2, "Cut", sel);
    m.addItem (3, "Paste", paste);
    m.addItem (4, "Delete selection", sel);
    m.addItem (5, "Crop to selection", sel);
    m.addSeparator();
    m.addItem (6, "Select all");
    m.showMenuAsync (PopupMenu::Options(), [safe = SafePointer<RememberEditor> (this)] (int r)
    {
        if (safe == nullptr) return;
        switch (r)
        {
            case 1: safe->doCopy(); break;
            case 2: safe->doCut(); break;
            case 3: safe->doPaste(); break;
            case 4: safe->doDelete(); break;
            case 5: safe->doCrop(); break;
            case 6: if (safe->S().hasClip()) safe->clipView.setSelection (0, safe->S().clip.length()); break;
            default: break;
        }
    });
}

void RememberEditor::doClear()
{
    if (! S().hasClip()) return;
    processor.getAudition().stop();
    S().beginEdit();
    const double sr = S().clip.sampleRate;
    S().clip = RememberClip::Clip();
    S().clip.sampleRate = sr;
    clipView.clearSelection();
    clipView.setMarker (-1);
    afterEdit (true);
    setStatus ("Editor emptied. Select audio in the history and press TO EDITOR, or drop a file");
}

void RememberEditor::doPlay()
{
    auto& a = processor.getAudition();
    if (a.isPlaying()) { a.stop(); return; }
    if (! needClip()) return;
    RememberClip::Range r;
    int64_t start;
    if (clipView.getSelection (r)) start = clipView.getMarker() >= r.a && clipView.getMarker() < r.b ? clipView.getMarker() : r.a;
    else
    {
        r = { 0, S().clip.length() };
        start = clipView.getMarker() >= 0 ? clipView.getMarker() : 0;
    }
    processor.publishClip();
    a.play (start, r.a, r.b, loopBtn.getToggleState());
}

bool RememberEditor::handleKey (const KeyPress& k)
{
    const auto m = k.getModifiers();
    const bool cmd = m.isCommandDown() || m.isCtrlDown();
    const auto c = (juce::juce_wchar) CharacterFunctions::toLowerCase (k.getTextCharacter());
    const int code = k.getKeyCode();
    if (cmd && (code == 'C' || code == 'c')) { doCopy(); return true; }
    if (cmd && (code == 'X' || code == 'x')) { doCut(); return true; }
    if (cmd && (code == 'V' || code == 'v')) { doPaste(); return true; }
    if (cmd && (code == 'Z' || code == 'z')) { if (m.isShiftDown()) doRedo(); else doUndo(); return true; }
    if (cmd && (code == 'A' || code == 'a')) { if (S().hasClip()) clipView.setSelection (0, S().clip.length()); return true; }
    if (code == KeyPress::deleteKey || code == KeyPress::backspaceKey) { doDelete(); return true; }
    if (code == KeyPress::spaceKey) { doPlay(); return true; }
    juce::ignoreUnused (c);
    return false;
}

void RememberEditor::timerCallback()
{
    processor.getEngine().collectGarbage();

    const bool hold = processor.apvts.getRawParameterValue (ParamID::hold)->load() > 0.5f;
    if (hold != view.isHeld()) view.setHold (hold);
    view.setSnapMode (getChoice (ParamID::snap));
    clipView.setSnapMode (getChoice (ParamID::snap));
    processor.getAudition().collectGarbage();

    histBtn.setButtonText ("HISTORY  " + historyNames[jlimit (0, 4, getChoice (ParamID::history))].toUpperCase());
    snapBtn.setButtonText ("SNAP  " + snapNames[jlimit (0, 4, getChoice (ParamID::snap))].toUpperCase());
    holdBtn.getProperties().set ("active", hold);
    liveBtn.getProperties().set ("active", view.isLive());
    syncGridShift();
    const bool overEditor = clipView.isMouseOver (true);
    if (overEditor != hintEditor) { hintEditor = overEditor; repaint (R (0, 696, designW, 30)); }
    const bool sel = view.hasSelection();
    const bool clip = S().hasClip(), csel = clipView.hasSelection();
    const bool playing = processor.getAudition().isPlaying();
    saveHistBtn.getProperties().set ("active", sel);
    saveHistBtn.setEnabled (sel);
    dragHistBtn.setEnabled (sel);
    toEditorBtn.setEnabled (sel);
    toEditorBtn.getProperties().set ("active", sel);
        for (Button* b : std::initializer_list<Button*> { &trimBtn, &reverseBtn, &fadeInBtn, &fadeOutBtn, &normBtn, &clearBtn, &playBtn, &loopBtn }) b->setEnabled (clip);
    wrapBtn.setEnabled (csel);
    undoBtn.setEnabled (S().canUndo());
    redoBtn.setEnabled (S().canRedo());
    saveClipBtn.setEnabled (clip);
    saveClipBtn.getProperties().set ("active", clip);
    dragClipBtn.setEnabled (clip);
    playBtn.setButtonText (playing ? "STOP" : "PLAY");
    playBtn.getProperties().set ("active", playing);
    loopBtn.getProperties().set ("active", loopBtn.getToggleState());
    for (Component* b : std::initializer_list<Component*> { &histBtn, &snapBtn, &holdBtn, &liveBtn, &saveHistBtn, &dragHistBtn, &toEditorBtn,
                                                            &trimBtn, &reverseBtn, &fadeInBtn, &fadeOutBtn, &normBtn, &clearBtn, &undoBtn, &redoBtn, &playBtn, &loopBtn, &wrapBtn, &importBtn, &saveClipBtn, &dragClipBtn, &keyBtn }) b->repaint();

    if (++infoTick % 8 == 0) refreshInfo();
    if (statusTicks > 0 && --statusTicks == 0) repaint (R (0, 696, designW, 30));
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

    // Spalten rechts (SAVE / DRAG WAV / FOLDER) liegen in allen Zeilen uebereinander
    view.setBounds (R (16, 130, 948, 214));
    // Raster: 10 gleich breite Spalten. Alle Reihen (Verlauf, Werkzeuge, Wiedergabe) richten sich danach aus
    const float gap = 6.0f, cw = (948.0f - gap * 9.0f) / 10.0f;
    auto colX = [&] (int i) { return 16.0f + (float) i * (cw + gap); };
    auto span = [&] (int n) { return cw * (float) n + gap * (float) (n - 1); };

    saveHistBtn.setBounds (R (colX (8), 352, cw, 40));
    dragHistBtn.setBounds (R (colX (9), 352, cw, 40));

    {
        // Eine Zeile, alle Knoepfe gleich gross, in Arbeitsreihenfolge
        Button* order[] { &importBtn, &toEditorBtn, &trimBtn, &fadeInBtn, &fadeOutBtn, &normBtn, &reverseBtn, &wrapBtn, &undoBtn, &redoBtn };
        for (int i = 0; i < 10; ++i) order[i]->setBounds (R (colX (i), 400, cw, 40));
    }
    clipView.setBounds (R (16, 448, 948, 196));

    // Unter dem Editor: PLAY, LOOP, CLEAR | Name, Tonart, +LOOP | SAVE, DRAG WAV
    playBtn.setBounds (R (colX (0), 652, cw, 40));
    loopBtn.setBounds (R (colX (1), 652, cw, 40));
    clearBtn.setBounds (R (colX (2), 652, cw, 40));
    // Name ist das breiteste Feld, der LOOP-Punkt sitzt am rechten Ende im Feld; KEY ist ein normal grosser Knopf
    projectEd.setBounds (R (colX (3), 652, span (4), 40));
    nameLoopBtn.setBounds (R (colX (6) + cw - 88.0f, 652, 84.0f, 40));
    keyBtn.setBounds (R (colX (7), 652, cw, 40));
    alignDot.setBounds (R (colX (3) + 10.0f, 352, 200.0f, 40));
    saveClipBtn.setBounds (R (colX (8), 652, cw, 40));
    dragClipBtn.setBounds (R (colX (9), 652, cw, 40));
    menuBtn.setBounds (R (916, 14, 48, 40));

    const auto f = Font (rememberFont (14.0f * scale));
    projectEd.setFont (f);
    projectEd.setBorder (BorderSize<int> (0, 0, 0, (int) (96.0f * scale)));
}

void RememberEditor::paint (Graphics& g)
{
    const auto& T = RememberColours::theme();
    const auto bounds = getLocalBounds().toFloat();
    g.setGradientFill (ColourGradient (T.bgTop, 0.0f, 0.0f, T.bgBottom, 0.0f, bounds.getBottom(), false));
    g.fillAll();

    // Kopfzeile wie bei den anderen Plugins (z. B. Drift Station): Logo links, Titel, darunter "by Dani Koenig . v1.x"
    const String byText = String::fromUTF8 ("by Dani Koenig  \xc2\xb7  v") + JucePlugin_VersionString;
    {
        const Colour cyan (0xff9fe3ee), ring (0xff3d5058);
        const float cx = 50.0f * scale, cy = 42.0f * scale, r = 19.0f * scale;
        // Remember-Logo: Ring = Ringpuffer, heller Bogen = zurueckliegende Aufnahme, Punkt = Aufnahmekopf "jetzt"
        const float sw = 4.5f * scale;
        g.setColour (ring);
        g.drawEllipse (cx - r, cy - r, 2.0f * r, 2.0f * r, sw);
        const float head = 0.0f;                                         // oben (JUCE misst ab 12 Uhr im Uhrzeigersinn)
        Path trail;
        trail.addCentredArc (cx, cy, r, r, 0.0f, head - 1.9f, head, true);   // gegen den Uhrzeigersinn zurueck
        g.setColour (cyan.withAlpha (0.55f));
        g.strokePath (trail, PathStrokeType (sw, PathStrokeType::curved, PathStrokeType::rounded));
        Path near;
        near.addCentredArc (cx, cy, r, r, 0.0f, head - 0.7f, head, true);
        g.setColour (cyan);
        g.strokePath (near, PathStrokeType (sw, PathStrokeType::curved, PathStrokeType::rounded));
        g.fillEllipse (cx - 5.5f * scale, cy - r - 5.5f * scale, 11.0f * scale, 11.0f * scale);
        g.fillEllipse (cx - 4.5f * scale, cy - 4.5f * scale, 9.0f * scale, 9.0f * scale);

        g.setColour (T.text);
        g.setFont (Font (rememberFont (36.0f * scale, true)));
        g.drawText ("REMEMBER", R (86, 10, 500, 40), Justification::centredLeft);
        g.setColour (T.textDim);
        g.setFont (Font (rememberFont (14.0f * scale)));
        g.drawText (byText, R (88, 50, 500, 18), Justification::centredLeft);
    }

    // Beschriftung "BARS" vor den Taktknoepfen
    g.setColour (T.textDim);
    g.setFont (Font (rememberFont (12.0f * scale, true)));
    g.drawText ("SELECT BARS", R (492, 80, 100, 40), Justification::centred);


    if (dropping)
    {
        g.setColour (T.accent.withAlpha (0.12f));
        g.fillRoundedRectangle (R (16, 448, 948, 196).toFloat(), T.radius);
        g.setColour (T.accent);
        g.setFont (Font (rememberFont (16.0f * scale, true)));
        g.drawText ("Drop to load into the editor", R (16, 448, 948, 196), Justification::centred);
    }

    // Info rechts neben dem Ordner
    g.setColour (T.text);
    g.setFont (Font (rememberFont (13.0f * scale, true)));
    g.drawText (infoText, R (16, 352, 520, 40), Justification::centredLeft);

    // Statuszeile
    if (statusTicks > 0)
    {
        g.setColour (T.accent);
        g.setFont (Font (rememberFont (12.5f * scale)));
        g.drawText (statusText, R (16, 700, 948, 22), Justification::centredLeft, true);
    }
    else
    {
        g.setColour (T.textDim.withAlpha (0.8f));
        g.setFont (Font (rememberFont (11.5f * scale)));
        g.drawText (hintEditor
            ? String::fromUTF8 ("EDITOR:  click = marker (play and paste position)   \xc2\xb7   drag = select   \xc2\xb7   double-click = bar   \xc2\xb7   right click = copy, cut, paste   \xc2\xb7   Space = play   \xc2\xb7   Esc = clear")
            : String::fromUTF8 ("HISTORY:  click = start point   \xc2\xb7   drag = select   \xc2\xb7   double-click = bar   \xc2\xb7   wheel = zoom   \xc2\xb7   Esc = clear"),
                    R (16, 700, 948, 22), Justification::centredLeft, true);
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
