#include "PluginProcessor.h"

LifePedalAudioProcessor::LifePedalAudioProcessor()
     : AudioProcessor (BusesProperties()
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
       apvts(*this, nullptr, "Parameters", {
           std::make_unique<juce::AudioParameterFloat>("octave", "Octave", 0.0f, 1.0f, 0.0f),
           std::make_unique<juce::AudioParameterFloat>("distortion", "Distortion", 0.0f, 1.0f, 0.5f),
           std::make_unique<juce::AudioParameterFloat>("filter", "Filter", 0.0f, 1.0f, 0.5f),
           std::make_unique<juce::AudioParameterChoice>("clipMode", "Diode Clipping", juce::StringArray{"Symmetrical Si", "Asymmetric LED", "Op-Amp Only"}, 0),
           std::make_unique<juce::AudioParameterFloat>("boost", "Boost", 0.0f, 1.0f, 0.0f),
           std::make_unique<juce::AudioParameterFloat>("volume", "Volume", 0.0f, 1.0f, 0.7f)
       })
{
}

LifePedalAudioProcessor::~LifePedalAudioProcessor() {}

void LifePedalAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    lastFilterSample[0] = 0.0f;
    lastFilterSample[1] = 0.0f;
}

void LifePedalAudioProcessor::releaseResources() {}

bool LifePedalAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

static float applyClipping(float input, int mode)
{
    switch (mode)
    {
        case 0: return std::tanh(input * 2.0f);
        case 1: return (input > 0.0f) ? std::tanh(input * 1.2f) : std::tanh(input * 2.5f);
        case 2: return input / (1.0f + std::abs(input));
        default: return std::tanh(input);
    }
}

void LifePedalAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    float octaveVal   = *apvts.getRawParameterValue("octave");
    float distVal     = *apvts.getRawParameterValue("distortion");
    float filterVal   = *apvts.getRawParameterValue("filter");
    int clipModeVal   = static_cast<int>(*apvts.getRawParameterValue("clipMode"));
    float boostVal    = *apvts.getRawParameterValue("boost");
    float volumeVal   = *apvts.getRawParameterValue("volume");

    float cutoffFreq = juce::jmap(1.0f - filterVal, 400.0f, 9000.0f);
    float dt = 1.0f / (float)getSampleRate();
    float RC = 1.0f / (2.0f * 3.14159265f * cutoffFreq);
    float alpha = dt / (RC + dt);

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer (channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float input = channelData[sample];

            float rectified = (std::abs(input) * 2.0f) - 0.3f;
            float octaveSignal = input + (rectified * octaveVal);

            float driveGain = 1.0f + (distVal * 45.0f);
            float clippedSignal = applyClipping(octaveSignal * driveGain, clipModeVal);

            lastFilterSample[channel] += alpha * (clippedSignal - lastFilterSample[channel]);
            float filteredSignal = lastFilterSample[channel];

            float boostGain = 1.0f + (boostVal * 4.0f);
            float boostedSignal = filteredSignal * boostGain;

            channelData[sample] = boostedSignal * volumeVal;
        }
    }
}

void LifePedalAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void LifePedalAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LifePedalAudioProcessor();
}
