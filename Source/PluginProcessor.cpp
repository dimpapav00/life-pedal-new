#include "PluginProcessor.h"

juce::AudioProcessorValueTreeState::ParameterLayout LifePedalAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"octave", 1}, "Octave", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"distortion", 1}, "Distortion", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"filter", 1}, "Filter", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"clipMode", 1}, "Diode Clipping", juce::StringArray{"Symmetrical Si", "Asymmetric LED", "Op-Amp Only"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"boost", 1}, "Boost", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"volume", 1}, "Volume", 0.0f, 1.0f, 0.7f));

    return { params.begin(), params.end() };
}

LifePedalAudioProcessor::LifePedalAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       apvts(*this, nullptr, "Parameters", createParameterLayout())
#endif
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
        case 0: // Symmetrical Silicon Diodes (Classic RAT)
            return std::tanh(input * 2.0f);
        case 1: // Asymmetric LED
            return (input > 0.0f) ? std::tanh(input * 1.2f) : std::tanh(input * 2.5f);
        case 2: // Op-Amp / Hard saturation
            return input / (1.0f + std::abs(input));
        default:
            return std::tanh(input);
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

    // RAT Filter logic (Inverted cutoff range)
    float cutoffFreq = juce::jmap(1.0f - filterVal, 400.0f, 9000.0f);
    float dt = 1.0f / (float)getSampleRate();
    float RC = 1.0f / (2.0f * juce::MathConstants<float>::pi * cutoffFreq);
    float alpha = dt / (RC + dt);

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer (channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float input = channelData[sample];

            // 1. Octave Stage (Full-wave rectifier style)
            float rectified = (std::abs(input) * 2.0f) - 0.3f;
            float octaveSignal = input + (rectified * octaveVal);

            // 2. RAT Distortion Stage
            float driveGain = 1.0f + (distVal * 45.0f);
            float clippedSignal = applyClipping(octaveSignal * driveGain, clipModeVal);

            // 3. Filter Stage (Low pass)
            lastFilterSample[channel] += alpha * (clippedSignal - lastFilterSample[channel]);
            float filteredSignal = lastFilterSample[channel];

            // 4. Boost Stage
            float boostGain = 1.0f + (boostVal * 4.0f);
            float boostedSignal = filteredSignal * boostGain;

            // 5. Output Master Volume
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
