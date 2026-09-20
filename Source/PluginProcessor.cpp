#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
DiamondRoomAudioProcessor::DiamondRoomAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, &undoManager, "DiamondRoom", dr::params::createLayout())
{
    cache.attach (state);
}

//==============================================================================
void DiamondRoomAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    maxBlockSize = juce::jmax (1, samplesPerBlock);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, 2 };

    driver.prepare (spec);
    hReverb.prepare (spec);
    mannyReverb.prepare (spec);
    valhallaReverb.prepare (spec);
    trueVerb.prepare (spec);
    tube.prepare (spec);

    for (auto* buffer : { &dryBuffer, &drivenBuffer, &wetBuffer })
    {
        buffer->setSize (2, maxBlockSize, false, false, true);
        buffer->clear();
    }

    // The Driver sits ahead of the dry/wet split, so both branches carry its
    // latency equally, and the tube stage runs at the base rate. Nothing on the
    // dry path needs delaying; the host only has to know about the Driver.
    dryDelayLength = 0;
    setLatencySamples ((int) std::ceil (driver.getLatencySamples()));

    dryDelayBuffer.setSize (2, juce::jmax (1, dryDelayLength + 1), false, false, true);
    dryDelayBuffer.clear();
    dryDelayWritePos = 0;

    const auto rampSeconds = 0.03;
    for (auto* s : { &hMixSmooth, &mMixSmooth, &vMixSmooth, &tMixSmooth, &masterMixSmooth })
        s->reset (sampleRate, rampSeconds);

    updateParameters();

    for (auto* s : { &hMixSmooth, &mMixSmooth, &vMixSmooth, &tMixSmooth, &masterMixSmooth })
        s->setCurrentAndTargetValue (s->getTargetValue());
}

void DiamondRoomAudioProcessor::releaseResources()
{
    driver.reset();
    hReverb.reset();
    mannyReverb.reset();
    valhallaReverb.reset();
    trueVerb.reset();
    tube.reset();
    dryDelayBuffer.clear();
}

bool DiamondRoomAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    const auto& in  = layouts.getMainInputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return in == out;
}

//==============================================================================
void DiamondRoomAudioProcessor::updateParameters()
{
    driver.setDrive (cache.drive->load());

    hReverb.setTone (cache.hTone->load());
    hReverb.setTime (cache.hTime->load());

    mannyReverb.setDistortion (cache.mDist->load());
    mannyReverb.setAmount (cache.mAmount->load());

    valhallaReverb.setHighCut (cache.vHighCut->load());
    valhallaReverb.setDecay (cache.vDecay->load());

    trueVerb.setDistance (cache.tDistance->load());
    trueVerb.setRoomsize (cache.tRoomsize->load());

    tube.setMix (cache.tube->load());

    hActive = cache.hOn->load() > 0.5f;
    mActive = cache.mOn->load() > 0.5f;
    vActive = cache.vOn->load() > 0.5f;
    tActive = cache.tOn->load() > 0.5f;

    hMixSmooth.setTargetValue (hActive ? cache.hMix->load() * 0.01f : 0.0f);
    mMixSmooth.setTargetValue (mActive ? cache.mMix->load() * 0.01f : 0.0f);
    vMixSmooth.setTargetValue (vActive ? cache.vMix->load() * 0.01f : 0.0f);
    tMixSmooth.setTargetValue (tActive ? cache.tMix->load() * 0.01f : 0.0f);

    masterMixSmooth.setTargetValue (cache.mix->load() * 0.01f);
}

void DiamondRoomAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numSamples = buffer.getNumSamples();
    const auto numInputs  = getTotalNumInputChannels();
    const auto numOutputs = getTotalNumOutputChannels();

    for (int ch = numInputs; ch < numOutputs; ++ch)
        buffer.clear (ch, 0, numSamples);

    if (numSamples == 0 || numOutputs == 0)
        return;

    // Scratch buffers and both oversamplers are sized for the block length the
    // host promised. If it hands us a longer one anyway, split it rather than
    // overrun - reallocating here would mean allocating on the audio thread.
    if (numSamples > maxBlockSize)
    {
        for (int offset = 0; offset < numSamples; offset += maxBlockSize)
        {
            const auto chunk = juce::jmin (maxBlockSize, numSamples - offset);

            juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(),
                                           numOutputs, offset, chunk);
            juce::MidiBuffer emptyMidi;
            processBlock (view, emptyMidi);
        }

        return;
    }

    updateParameters();

    // Everything downstream is stereo; mono input is duplicated and folded
    // back down at the end.
    auto copyIn = [&buffer, numSamples, numInputs] (juce::AudioBuffer<float>& dest)
    {
        dest.copyFrom (0, 0, buffer, 0, 0, numSamples);
        dest.copyFrom (1, 0, buffer, numInputs > 1 ? 1 : 0, 0, numSamples);
    };

    copyIn (drivenBuffer);

    // -- 1. Driver, ahead of the dry/wet split -----------------------------
    // The Patcher graph fed only the reverbs from the Driver, which left the
    // dial inaudible at anything but a very wet Mix. Here it colours the whole
    // signal, so Drive works as a tone control for the plugin rather than for
    // the reverb bus alone.
    {
        juce::dsp::AudioBlock<float> block (drivenBuffer.getArrayOfWritePointers(),
                                            2, (size_t) numSamples);
        driver.process (block);
    }

    dryBuffer.makeCopyOf (drivenBuffer, true);

    // -- 2. the four reverbs in parallel, each with its own mix ------------
    wetBuffer.clear();

    auto* drivenL = drivenBuffer.getReadPointer (0);
    auto* drivenR = drivenBuffer.getReadPointer (1);
    auto* wetL = wetBuffer.getWritePointer (0);
    auto* wetR = wetBuffer.getWritePointer (1);

    for (int n = 0; n < numSamples; ++n)
    {
        const auto inL = drivenL[n];
        const auto inR = drivenR[n];

        float sumL = 0.0f, sumR = 0.0f;
        float l = 0.0f, r = 0.0f;

        const auto hGain = hMixSmooth.getNextValue();
        const auto mGain = mMixSmooth.getNextValue();
        const auto vGain = vMixSmooth.getNextValue();
        const auto tGain = tMixSmooth.getNextValue();

        if (hActive || hGain > 0.0f)
        {
            hReverb.process (inL, inR, l, r);
            sumL += l * hGain;
            sumR += r * hGain;
        }

        if (mActive || mGain > 0.0f)
        {
            mannyReverb.process (inL, inR, l, r);
            sumL += l * mGain;
            sumR += r * mGain;
        }

        if (vActive || vGain > 0.0f)
        {
            valhallaReverb.process (inL, inR, l, r);
            sumL += l * vGain;
            sumR += r * vGain;
        }

        if (tActive || tGain > 0.0f)
        {
            trueVerb.process (inL, inR, l, r);
            sumL += l * tGain;
            sumR += r * tGain;
        }

        wetL[n] = sumL;
        wetR[n] = sumR;
    }

    // -- 3. Tube: the CleanVoice valve stage, blended by its own Mix -------
    {
        juce::dsp::AudioBlock<float> block (wetBuffer.getArrayOfWritePointers(),
                                            2, (size_t) numSamples);
        tube.process (block);
    }

    // -- 4. dry delay compensation and the master Mix ----------------------
    {
        float* wet[2] { wetBuffer.getWritePointer (0), wetBuffer.getWritePointer (1) };
        const float* dryIn[2] { dryBuffer.getReadPointer (0), dryBuffer.getReadPointer (1) };
        float* line[2] { dryDelayBuffer.getWritePointer (0), dryDelayBuffer.getWritePointer (1) };
        const auto lineLength = dryDelayBuffer.getNumSamples();

        for (int n = 0; n < numSamples; ++n)
        {
            float dry[2] { dryIn[0][n], dryIn[1][n] };

            if (dryDelayLength > 0)
            {
                const auto readPos = (dryDelayWritePos + 1) % lineLength;

                for (int ch = 0; ch < 2; ++ch)
                {
                    const auto delayed = line[ch][readPos];
                    line[ch][dryDelayWritePos] = dry[ch];
                    dry[ch] = delayed;
                }

                dryDelayWritePos = readPos;
            }

            const auto mix = masterMixSmooth.getNextValue();

            for (int ch = 0; ch < 2; ++ch)
                wet[ch][n] = dry[ch] + mix * (wet[ch][n] - dry[ch]);
        }
    }

    // -- 5. back to the host -----------------------------------------------
    if (numOutputs == 1)
    {
        auto* out = buffer.getWritePointer (0);
        const auto* l = wetBuffer.getReadPointer (0);
        const auto* r = wetBuffer.getReadPointer (1);

        for (int n = 0; n < numSamples; ++n)
            out[n] = 0.5f * (l[n] + r[n]);
    }
    else
    {
        for (int ch = 0; ch < 2; ++ch)
            buffer.copyFrom (ch, 0, wetBuffer, ch, 0, numSamples);
    }
}

//==============================================================================
juce::AudioProcessorEditor* DiamondRoomAudioProcessor::createEditor()
{
    return new DiamondRoomAudioProcessorEditor (*this);
}

int DiamondRoomAudioProcessor::getSavedEditorWidth() const
{
    return (int) state.state.getProperty ("uiWidth", 0);
}

void DiamondRoomAudioProcessor::setSavedEditorWidth (int width)
{
    // Deliberately not through the undo manager: resizing the window is not an
    // edit to the patch.
    state.state.setProperty ("uiWidth", width, nullptr);
}

void DiamondRoomAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void DiamondRoomAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DiamondRoomAudioProcessor();
}
