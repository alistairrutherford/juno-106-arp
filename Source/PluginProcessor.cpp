#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
Juno106AudioProcessor::Juno106AudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    shared.lfoRate    = apvts.getRawParameterValue ("lfoRate");
    shared.lfoDelay   = apvts.getRawParameterValue ("lfoDelay");
    shared.dcoRange   = apvts.getRawParameterValue ("dcoRange");
    shared.dcoLfo     = apvts.getRawParameterValue ("dcoLfo");
    shared.dcoPwm     = apvts.getRawParameterValue ("dcoPwm");
    shared.pwmSource  = apvts.getRawParameterValue ("pwmSource");
    shared.sawOn      = apvts.getRawParameterValue ("sawOn");
    shared.pulseOn    = apvts.getRawParameterValue ("pulseOn");
    shared.subLevel   = apvts.getRawParameterValue ("subLevel");
    shared.noiseLevel = apvts.getRawParameterValue ("noiseLevel");
    shared.vcfFreq    = apvts.getRawParameterValue ("vcfFreq");
    shared.vcfRes     = apvts.getRawParameterValue ("vcfRes");
    shared.vcfEnvPol  = apvts.getRawParameterValue ("vcfEnvPol");
    shared.vcfEnv     = apvts.getRawParameterValue ("vcfEnv");
    shared.vcfLfo     = apvts.getRawParameterValue ("vcfLfo");
    shared.vcfKybd    = apvts.getRawParameterValue ("vcfKybd");
    shared.envAttack  = apvts.getRawParameterValue ("attack");
    shared.envDecay   = apvts.getRawParameterValue ("decay");
    shared.envSustain = apvts.getRawParameterValue ("sustain");
    shared.envRelease = apvts.getRawParameterValue ("release");
    shared.vcaMode    = apvts.getRawParameterValue ("vcaMode");
    shared.vcaLevel   = apvts.getRawParameterValue ("vcaLevel");
    shared.portTime   = apvts.getRawParameterValue ("portamento");

    // Six voices, like the original.
    for (int i = 0; i < 6; ++i)
        synth.addVoice (new JunoVoice (shared));

    synth.addSound (new JunoSound());
}

juce::AudioProcessorValueTreeState::ParameterLayout Juno106AudioProcessor::createLayout()
{
    using FloatParam  = juce::AudioParameterFloat;
    using ChoiceParam = juce::AudioParameterChoice;
    using BoolParam   = juce::AudioParameterBool;
    using Range       = juce::NormalisableRange<float>;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Performance
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { "portamento", 1 }, "Portamento",
                                              Range (0.0f, 2.0f, 0.0f, 0.5f), 0.0f));

    // LFO
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { "lfoRate", 1 },  "LFO Rate",
                                              Range (0.1f, 30.0f, 0.0f, 0.4f), 4.0f));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { "lfoDelay", 1 }, "LFO Delay",
                                              Range (0.0f, 3.0f), 0.0f));

    // DCO
    layout.add (std::make_unique<ChoiceParam> (juce::ParameterID { "dcoRange", 1 }, "DCO Range",
                                               juce::StringArray { "16'", "8'", "4'" }, 1));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { "dcoLfo", 1 },  "DCO LFO",   Range (0.0f, 1.0f), 0.0f));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { "dcoPwm", 1 },  "DCO PWM",   Range (0.0f, 1.0f), 0.0f));
    layout.add (std::make_unique<ChoiceParam> (juce::ParameterID { "pwmSource", 1 }, "PWM Source",
                                               juce::StringArray { "LFO", "Manual" }, 0));
    layout.add (std::make_unique<BoolParam> (juce::ParameterID { "sawOn", 1 },   "Saw",   true));
    layout.add (std::make_unique<BoolParam> (juce::ParameterID { "pulseOn", 1 }, "Pulse", false));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { "subLevel", 1 },   "Sub Level",   Range (0.0f, 1.0f), 0.0f));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { "noiseLevel", 1 }, "Noise Level", Range (0.0f, 1.0f), 0.0f));

    // HPF
    layout.add (std::make_unique<ChoiceParam> (juce::ParameterID { "hpf", 1 }, "HPF",
                                               juce::StringArray { "0", "1", "2", "3" }, 1));

    // VCF
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { "vcfFreq", 1 }, "VCF Freq", Range (0.0f, 1.0f), 0.7f));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { "vcfRes", 1 },  "VCF Res",  Range (0.0f, 1.0f), 0.0f));
    layout.add (std::make_unique<ChoiceParam> (juce::ParameterID { "vcfEnvPol", 1 }, "VCF Env Polarity",
                                               juce::StringArray { "Positive", "Negative" }, 0));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { "vcfEnv", 1 },  "VCF Env",  Range (0.0f, 1.0f), 0.0f));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { "vcfLfo", 1 },  "VCF LFO",  Range (0.0f, 1.0f), 0.0f));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { "vcfKybd", 1 }, "VCF Kybd", Range (0.0f, 1.0f), 0.5f));

    // ENV
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { "attack", 1 },  "Attack",
                                              Range (0.001f, 3.0f, 0.0f, 0.35f), 0.005f));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { "decay", 1 },   "Decay",
                                              Range (0.002f, 10.0f, 0.0f, 0.3f), 0.3f));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { "sustain", 1 }, "Sustain", Range (0.0f, 1.0f), 0.8f));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { "release", 1 }, "Release",
                                              Range (0.002f, 12.0f, 0.0f, 0.3f), 0.2f));

    // VCA
    layout.add (std::make_unique<ChoiceParam> (juce::ParameterID { "vcaMode", 1 }, "VCA Mode",
                                               juce::StringArray { "Env", "Gate" }, 0));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { "vcaLevel", 1 }, "VCA Level", Range (0.0f, 1.0f), 0.8f));

    // Chorus
    layout.add (std::make_unique<BoolParam> (juce::ParameterID { "chorusI", 1 },  "Chorus I",  false));
    layout.add (std::make_unique<BoolParam> (juce::ParameterID { "chorusII", 1 }, "Chorus II", false));

    return layout;
}

//==============================================================================
void Juno106AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);

    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<JunoVoice*> (synth.getVoice (i)))
            v->prepare (sampleRate);

    shared.lfoBuffer.assign ((size_t) juce::jmax (16, samplesPerBlock), 0.0f);
    chorus.prepare (sampleRate);
    hpf.prepare (sampleRate);
    lfoPhase = 0.0f;
}

bool Juno106AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

void Juno106AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int numSamples = buffer.getNumSamples();

    // Mod wheel (CC1) adds immediate LFO vibrato in the voices.
    for (const auto metadata : midi)
    {
        const auto msg = metadata.getMessage();
        if (msg.isController() && msg.getControllerNumber() == 1)
            shared.modWheel.store ((float) msg.getControllerValue() / 127.0f);
    }

    if ((int) shared.lfoBuffer.size() < numSamples)
        shared.lfoBuffer.assign ((size_t) numSamples, 0.0f);

    // Global free-running triangle LFO, shared by every voice.
    const float inc = shared.lfoRate->load() / (float) getSampleRate();
    for (int i = 0; i < numSamples; ++i)
    {
        shared.lfoBuffer[(size_t) i] = lfoPhase < 0.5f ? 4.0f * lfoPhase - 1.0f
                                                       : 3.0f - 4.0f * lfoPhase;
        lfoPhase += inc;
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
    }

    synth.renderNextBlock (buffer, midi, 0, numSamples);

    // HPF (linear, so post-mix is equivalent) then BBD chorus.
    hpf.setPosition ((int) apvts.getRawParameterValue ("hpf")->load());
    auto* l = buffer.getWritePointer (0);
    for (int i = 0; i < numSamples; ++i)
        l[i] = hpf.process (l[i]);

    if (buffer.getNumChannels() > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

    const bool cI  = apvts.getRawParameterValue ("chorusI")->load()  > 0.5f;
    const bool cII = apvts.getRawParameterValue ("chorusII")->load() > 0.5f;
    chorus.setMode (cI && cII ? 3 : cII ? 2 : cI ? 1 : 0);
    chorus.process (buffer);
}

//==============================================================================
// Factory presets. Each entry lists only the parameters that differ from the
// defaults; everything else is reset first. Exposed to AU hosts as factory
// presets via the program interface.
namespace
{
    struct Preset
    {
        const char* name;
        std::vector<std::pair<const char*, float>> values;   // paramID -> plain value
    };

    const std::vector<Preset>& getFactoryPresets()
    {
        static const std::vector<Preset> presets = {
            { "Init", {} },

            { "Lush Strings",
              { { "sawOn", 1 }, { "subLevel", 0.35f }, { "chorusI", 1 },
                { "attack", 0.35f }, { "decay", 1.2f }, { "sustain", 0.75f }, { "release", 0.9f },
                { "vcfFreq", 0.55f }, { "vcfEnv", 0.2f }, { "vcfKybd", 0.7f },
                { "lfoRate", 5.0f }, { "lfoDelay", 0.8f }, { "dcoLfo", 0.12f } } },

            { "Analog Brass",
              { { "sawOn", 1 }, { "vcfFreq", 0.42f }, { "vcfRes", 0.12f }, { "vcfEnv", 0.5f },
                { "attack", 0.03f }, { "decay", 0.5f }, { "sustain", 0.55f }, { "release", 0.12f },
                { "vcfKybd", 0.6f } } },

            { "PWM Pad",
              { { "sawOn", 0 }, { "pulseOn", 1 }, { "dcoPwm", 0.6f }, { "pwmSource", 0 },
                { "lfoRate", 0.9f }, { "subLevel", 0.2f }, { "chorusII", 1 },
                { "attack", 0.6f }, { "decay", 1.5f }, { "sustain", 0.8f }, { "release", 1.4f },
                { "vcfFreq", 0.5f }, { "vcfKybd", 0.6f } } },

            { "Deep Sub Bass",
              { { "dcoRange", 0 }, { "sawOn", 1 }, { "subLevel", 1.0f },
                { "vcfFreq", 0.32f }, { "vcfEnv", 0.35f }, { "vcfKybd", 0.4f },
                { "attack", 0.002f }, { "decay", 0.35f }, { "sustain", 0.25f }, { "release", 0.08f } } },

            { "Resonant Sweep",
              { { "sawOn", 1 }, { "subLevel", 0.3f },
                { "vcfFreq", 0.15f }, { "vcfRes", 0.85f }, { "vcfEnv", 0.75f },
                { "attack", 0.005f }, { "decay", 2.0f }, { "sustain", 0.0f }, { "release", 0.5f } } },

            { "Pipe Organ",
              { { "sawOn", 0 }, { "pulseOn", 1 }, { "pwmSource", 1 }, { "dcoPwm", 0.1f },
                { "subLevel", 0.8f }, { "vcaMode", 1 }, { "vcfFreq", 0.75f },
                { "chorusI", 1 }, { "vcfKybd", 0.8f } } },

            { "Vibrato Keys",
              { { "sawOn", 1 }, { "subLevel", 0.4f },
                { "vcfFreq", 0.6f }, { "vcfEnv", 0.3f }, { "vcfKybd", 0.7f },
                { "attack", 0.005f }, { "decay", 1.2f }, { "sustain", 0.4f }, { "release", 0.4f },
                { "chorusI", 1 }, { "chorusII", 1 } } },

            { "Porta Lead",
              { { "sawOn", 1 }, { "subLevel", 0.5f }, { "portamento", 0.12f },
                { "vcfFreq", 0.45f }, { "vcfRes", 0.3f }, { "vcfEnv", 0.4f }, { "vcfKybd", 0.7f },
                { "attack", 0.003f }, { "decay", 0.8f }, { "sustain", 0.6f }, { "release", 0.25f },
                { "dcoLfo", 0.15f }, { "lfoDelay", 0.6f }, { "lfoRate", 5.5f } } },

            { "Wind Noise",
              { { "sawOn", 0 }, { "noiseLevel", 1.0f },
                { "vcfFreq", 0.5f }, { "vcfRes", 0.45f }, { "vcfLfo", 0.35f }, { "lfoRate", 0.4f },
                { "attack", 1.2f }, { "sustain", 1.0f }, { "release", 2.0f },
                { "vcfKybd", 0.0f } } },
        };
        return presets;
    }
}

int Juno106AudioProcessor::getNumPrograms()
{
    return (int) getFactoryPresets().size();
}

const juce::String Juno106AudioProcessor::getProgramName (int index)
{
    const auto& presets = getFactoryPresets();
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? presets[(size_t) index].name
                                                                  : juce::String();
}

void Juno106AudioProcessor::setCurrentProgram (int index)
{
    const auto& presets = getFactoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;

    currentProgram = index;

    // Reset everything to defaults, then apply the preset's overrides.
    for (auto* param : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            ranged->setValueNotifyingHost (ranged->getDefaultValue());

    for (const auto& [paramID, value] : presets[(size_t) index].values)
        if (auto* param = apvts.getParameter (paramID))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
}

//==============================================================================
void Juno106AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void Juno106AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* Juno106AudioProcessor::createEditor()
{
    return new Juno106AudioProcessorEditor (*this);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Juno106AudioProcessor();
}
