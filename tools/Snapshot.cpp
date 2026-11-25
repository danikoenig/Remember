// Entwickler-Werkzeug: fuettert das Plugin mit Testaudio (124 bpm) und simuliertem Host-Transport
// und speichert ein Bild der Oberflaeche.   RememberSnapshot out.png [bars]
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

using namespace juce;

struct FakeHost : public AudioPlayHead
{
    double ppq = 0.0, bpm = 124.0;
    bool playing = false;
    Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setIsPlaying (playing);
        p.setPpqPosition (ppq);
        p.setBpm (bpm);
        p.setTimeSignature (TimeSignature { 4, 4 });
        p.setPpqPositionOfLastBarStart (std::floor (ppq / 4.0) * 4.0);
        return p;
    }
};

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const File out = File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "remember_ui.png");
    const int bars = argc > 2 ? String (argv[2]).getIntValue() : 4;

    RememberProcessor proc;
    FakeHost host;
    proc.setPlayHead (&host);
    const double sr = 48000.0;
    proc.setRateAndBufferSizeDetails (sr, 512);
    proc.prepareToPlay (sr, 512);

    // 70 s Audio: Kick auf jedem Beat, Hat auf jedem 8tel, Bass-Ton; Transport startet bei 6 s
    AudioBuffer<float> buf (2, 512);
    MidiBuffer midi;
    Random rnd (7);
    const double spb = sr * 60.0 / host.bpm;
    int64_t t = 0;
    double bassPhase = 0.0;
    while (t < (int64_t) (70.0 * sr))
    {
        host.playing = t >= (int64_t) (6.0 * sr);
        if (host.playing) host.ppq = (double) (t - (int64_t) (6.0 * sr)) / spb;
        for (int i = 0; i < 512; ++i)
        {
            const double pos = host.playing ? (double) (t + i - (int64_t) (6.0 * sr)) / spb : 0.0;
            const double inBeat = pos - std::floor (pos);
            const double inEighth = pos * 2.0 - std::floor (pos * 2.0);
            float v = 0.0f;
            if (host.playing)
            {
                v += (float) (std::sin (2.0 * MathConstants<double>::pi * (55.0 + 120.0 * std::exp (-inBeat * 18.0)) * inBeat * spb / sr) * std::exp (-inBeat * 5.0) * 0.8);
                v += (float) ((rnd.nextFloat() * 2.0f - 1.0f) * std::exp (-inEighth * 14.0) * 0.25);
                bassPhase += 2.0 * MathConstants<double>::pi * (((int) (pos / 4.0) % 2) ? 43.65 : 49.0) / sr;
                v += (float) (std::sin (bassPhase) * 0.25 * (0.6 + 0.4 * std::sin (pos * 1.3)));
            }
            else v = rnd.nextFloat() * 0.002f;
            buf.setSample (0, i, v);
            buf.setSample (1, i, v * 0.95f);
        }
        proc.processBlock (buf, midi);
        t += 512;
    }

    std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize (980, 818);
    MessageManager::getInstance()->runDispatchLoopUntil (300);

    // Button "bars" druecken: letzte vollstaendige Takte (ohne Startpunkt)
    std::function<void (Component&)> press = [&] (Component& c)
    {
        if (auto* b = dynamic_cast<TextButton*> (&c))
            if (b->getButtonText() == String (bars)) { b->triggerClick(); return; }
        for (auto* ch : c.getChildren()) press (*ch);
    };
    if (bars > 0) press (*ed);
    MessageManager::getInstance()->runDispatchLoopUntil (400);

    // In den Editor kopieren, bearbeiten
    auto pressText = [&] (const String& text)
    {
        std::function<bool (Component&)> f = [&] (Component& c)
        {
            if (auto* b = dynamic_cast<TextButton*> (&c))
                if (b->getButtonText() == text && b->isEnabled()) { b->triggerClick(); return true; }
            for (auto* ch : c.getChildren()) if (f (*ch)) return true;
            return false;
        };
        f (*ed);
        MessageManager::getInstance()->runDispatchLoopUntil (150);
    };
    if (bars > 0)
    {
        pressText ("TO EDITOR");
        proc.getAudition().stop();
    }

    // SAVE druecken (Ordner /tmp/remember_out), Ergebnis wird von aussen geprueft
    proc.setProp (PropID::folder, "/tmp/remember_out");
    proc.setProp (PropID::project, "Test");
    proc.setProp (PropID::type, "Synth");
    proc.setProp (PropID::key, "A");
    proc.setProp (PropID::scale, "Minor");
    std::function<void (Component&)> save = [&] (Component& c)
    {
        if (auto* b = dynamic_cast<TextButton*> (&c))
            if (b->getButtonText() == "SAVE") b->triggerClick();
        for (auto* ch : c.getChildren()) save (*ch);
    };
    save (*ed);   // zwei SAVE-Knoepfe: History und Editor
    MessageManager::getInstance()->runDispatchLoopUntil (200);

    // WAV-Import pruefen: die eben gespeicherte Datei per Drop laden
    if (bars > 0)
    {
        auto* re = dynamic_cast<RememberEditor*> (ed.get());
        re->filesDropped ({ "/tmp/remember_out/Test_Synth_Loop_124bpm_Amin_T01.wav" }, 0, 0);
        MessageManager::getInstance()->runDispatchLoopUntil (200);
        std::printf ("clip length after import: %lld\n", (long long) proc.getSession().clip.length());
    }

    const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    out.deleteFile();
    FileOutputStream os (out);
    PNGImageFormat png;
    png.writeImageToStream (img, os);
    std::printf ("written %s (%d x %d)\n", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    ed.reset();
    return 0;
}
