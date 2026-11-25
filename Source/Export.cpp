#include "Export.h"

using namespace juce;

namespace RememberExport
{
    String sanitise (const String& s)
    {
        String out;
        for (auto c : s.trim())
        {
            if (CharacterFunctions::isLetterOrDigit (c) || c == '-' || c == '_' || c == '#') out << String::charToString (c);
            else if (c == ' ') out << "-";
        }
        return out;
    }

    String buildStem (const ClipMeta& m)
    {
        StringArray parts;
        auto add = [&] (const String& p) { const auto s = sanitise (p); if (s.isNotEmpty()) parts.add (s); };
        add (m.project);
        add (m.type);
        add (m.form);
        if (m.bpm > 1.0)
        {
            const bool whole = std::abs (m.bpm - std::round (m.bpm)) < 0.05;
            parts.add (whole ? String ((int) std::round (m.bpm)) + "bpm" : String (m.bpm, 1) + "bpm");
        }
        if (m.key.trim().isNotEmpty())
            parts.add (sanitise (m.key) + (m.scale == "Minor" ? "min" : m.scale == "Major" ? "maj" : ""));
        return parts.isEmpty() ? String ("Remember") : parts.joinIntoString ("_");
    }

    String fileName (const String& stem, int take)
    {
        return stem + "_T" + String (take).paddedLeft ('0', 2) + ".wav";
    }

    int nextTake (const File& folder, const String& stem)
    {
        int best = 0;
        for (const auto& f : folder.findChildFiles (File::findFiles, false, stem + "_T*.wav"))
        {
            const auto n = f.getFileNameWithoutExtension().fromLastOccurrenceOf ("_T", false, false);
            if (n.isNotEmpty() && n.containsOnly ("0123456789")) best = jmax (best, n.getIntValue());
        }
        return best + 1;
    }

    File defaultFolder()
    {
        return File::getSpecialLocation (File::userMusicDirectory).getChildFile ("Remember");
    }

    String writeWav (const RememberDSP::Storage& st, int64_t start, int64_t len, double sampleRate, const File& file)
    {
        if (len <= 0) return "Nothing selected";
        if (! file.getParentDirectory().createDirectory()) return "Cannot create folder";

        auto stream = std::make_unique<FileOutputStream> (file);
        if (! stream->openedOk()) return "Cannot write " + file.getFileName();
        stream->setPosition (0);
        stream->truncate();

        WavAudioFormat wav;
        std::unique_ptr<AudioFormatWriter> writer (wav.createWriterFor (stream.get(), sampleRate, (unsigned) st.numCh, 24, {}, 0));
        if (writer == nullptr) return "Cannot create WAV writer";
        stream.release();   // gehoert jetzt dem Writer

        constexpr int chunk = 1 << 16;
        std::vector<std::vector<float>> buf ((size_t) st.numCh, std::vector<float> (chunk));
        std::vector<float*> ptr;
        for (auto& b : buf) ptr.push_back (b.data());

        for (int64_t done = 0; done < len; done += chunk)
        {
            const int n = (int) std::min<int64_t> (chunk, len - done);
            if (! st.copyRange (start + done, n, ptr.data(), st.numCh))
            {
                writer.reset();
                file.deleteFile();
                return "Selection is no longer in the history";
            }
            if (! writer->writeFromFloatArrays (ptr.data(), st.numCh, n))
            {
                writer.reset();
                file.deleteFile();
                return "Write error (disk full?)";
            }
        }
        writer.reset();
        return {};
    }
}
