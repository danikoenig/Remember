#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

double RememberProcessor::historySecondsFor (int index)
{
    static const double s[] = { 60.0, 120.0, 300.0, 600.0, 1200.0 };
    return s[jlimit (0, 4, index)];
}

AudioProcessorValueTreeState::ParameterLayout RememberProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::history, 1 }, "History",
                    StringArray { "1 min", "2 min", "5 min", "10 min", "20 min" }, 2));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::snap, 1 }, "Snap",
                    StringArray { "Off", "1/16", "1/8", "Beat", "Bar" }, 3));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamID::hold, 1 }, "Hold", false));
    return { p.begin(), p.end() };
}

RememberProcessor::RememberProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "REMEMBER", createLayout())
{
    pHistory = apvts.getRawParameterValue (ParamID::history);
    apvts.addParameterListener (ParamID::history, this);
    engine.prepare (sampleRate, 2, historySecondsFor ((int) pHistory->load()));
}

RememberProcessor::~RememberProcessor()
{
    cancelPendingUpdate();
    apvts.removeParameterListener (ParamID::history, this);
}

bool RememberProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto o = l.getMainOutputChannelSet();
    return (o == AudioChannelSet::stereo() || o == AudioChannelSet::mono()) && o == l.getMainInputChannelSet();
}

void RememberProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    audition.setSampleRate (sr);
    engine.prepare (sr, jlimit (1, 2, getTotalNumInputChannels()), historySecondsFor ((int) pHistory->load()));
}

void RememberProcessor::handleAsyncUpdate()
{
    engine.setHistorySeconds (historySecondsFor ((int) pHistory->load()));
}

// Das Audio geht unveraendert durch. Der Audio-Thread schreibt nur in den Ringpuffer (keine Allokation, keine Locks, keine Dateien).
void RememberProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;

    RememberDSP::HostInfo h;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            h.playing = pos->getIsPlaying();
            if (auto p = pos->getPpqPosition()) { h.hasPpq = true; h.ppq = *p; }
            if (auto b = pos->getBpm()) h.bpm = *b;
            if (auto ts = pos->getTimeSignature())
                if (ts->denominator > 0) h.beatsPerBar = (float) ts->numerator * 4.0f / (float) ts->denominator;
            if (auto lb = pos->getPpqPositionOfLastBarStart()) h.barStartPpq = *lb;
        }

    engine.process (buffer.getArrayOfReadPointers(), jmin (buffer.getNumChannels(), getTotalNumInputChannels()),
                    buffer.getNumSamples(), h);

    // Vorhoeren des Editor-Clips (nach der Aufnahme, damit immer der Eingang aufgenommen wird)
    audition.process (buffer.getArrayOfWritePointers(), jmin (buffer.getNumChannels(), 2), buffer.getNumSamples());
}

void RememberProcessor::publishClip()
{
    auto d = std::make_shared<RememberAudition::Data>();
    d->sampleRate = session.clip.sampleRate;
    d->ch = session.clip.ch;
    audition.publish (std::move (d));
}

AudioProcessorEditor* RememberProcessor::createEditor() { return new RememberEditor (*this); }

String RememberProcessor::getProp (const char* id, const String& def) const
{
    return apvts.state.getProperty (id, def).toString();
}

void RememberProcessor::setProp (const char* id, const String& value)
{
    apvts.state.setProperty (id, value, nullptr);
}

void RememberProcessor::getStateInformation (MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void RememberProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RememberProcessor(); }
