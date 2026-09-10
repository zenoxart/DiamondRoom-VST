/*
    Development helper. Instantiates the plugin and either paints its editor
    into a PNG without opening a window, so the panel artwork can be reviewed
    from a build step, or runs the DSP offline and checks the tail behaves. Not
    part of the shipping plugin - see the DIAMONDROOM_BUILD_SCREENSHOT option in
    CMakeLists.txt.

    Usage: DiamondRoomShot <output.png> [width]
           DiamondRoomShot --audio          (offline DSP self-test)
           DiamondRoomShot --ui             (fader geometry self-test)
*/

#include "../Source/PluginProcessor.h"
#include "../Source/gui/MetalSlider.h"

namespace
{
    //==========================================================================
    /**
        Checks that what a fader draws and what it drags agree. JUCE derives the
        draggable region in Slider::resized(); if a subclass draws its track
        somewhere else, the cap and the mouse disagree and the control becomes
        impossible to set.
    */
    void collectSliders (juce::Component& parent, std::vector<dr::MetalSlider*>& found)
    {
        for (auto* child : parent.getChildren())
        {
            if (auto* slider = dynamic_cast<dr::MetalSlider*> (child))
                found.push_back (slider);

            collectSliders (*child, found);
        }
    }

    int runUiSelfTest()
    {
        DiamondRoomAudioProcessor processor;
        processor.prepareToPlay (48000.0, 512);

        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

        if (editor == nullptr)
        {
            std::cerr << "no editor" << std::endl;
            return 1;
        }

        bool ok = true;

        for (const auto width : { 900, 1300, 2001 })
        {
            editor->setSize (width, juce::roundToInt ((double) width * 764.0 / 2001.0));

            std::vector<dr::MetalSlider*> sliders;
            collectSliders (*editor, sliders);

            if (sliders.empty())
            {
                std::cerr << "FAIL: no faders found" << std::endl;
                return 1;
            }

            float worst = 0.0f;

            for (auto* slider : sliders)
            {
                const auto track = slider->getTrackArea();
                const auto range = slider->getRange();
                const auto vertical = slider->isVertical();

                for (const auto proportion : { 0.0, 0.25, 0.5, 0.75, 1.0 })
                {
                    const auto value = range.getStart() + proportion * range.getLength();
                    slider->setValue (value, juce::dontSendNotification);

                    // Where the cap is drawn.
                    const auto drawn = vertical
                        ? track.getBottom() - (float) proportion * track.getHeight()
                        : track.getX() + (float) proportion * track.getWidth();

                    // Where JUCE maps that value on screen.
                    const auto mapped = slider->getPositionOfValue (value);

                    worst = juce::jmax (worst, std::abs (drawn - mapped));
                }
            }

            std::cout << "width " << width << " : worst cap/mouse mismatch "
                      << worst << " px" << std::endl;

            // A pixel of rounding is fine; anything more is a control the user
            // cannot place accurately.
            if (worst > 1.5f)
            {
                std::cerr << "  FAIL: fader drawing and dragging disagree" << std::endl;
                ok = false;
            }
        }

        std::cout << (ok ? "PASS" : "FAILED") << std::endl;
        return ok ? 0 : 1;
    }

    constexpr double testSampleRate = 48000.0;
    constexpr int blockSize = 512;

    struct TailReport
    {
        float inputPeak = 0.0f;
        float outputPeak = 0.0f;
        float tailPeak = 0.0f;      // after the burst ends
        float tailAtEnd = 0.0f;
        double decaySeconds = 0.0;  // tail peak down to -60 dB
        bool finite = true;
    };

    /** One second of noise, then nine seconds of silence. */
    TailReport measureTail (DiamondRoomAudioProcessor& processor)
    {
        constexpr int burstBlocks = 94;    // ~1 s
        constexpr int totalBlocks = 940;   // ~10 s

        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        juce::Random random (0x51ee7);

        std::vector<float> envelope;
        envelope.reserve ((size_t) totalBlocks);

        TailReport report;

        for (int block = 0; block < totalBlocks; ++block)
        {
            buffer.clear();

            if (block < burstBlocks)
            {
                for (int ch = 0; ch < 2; ++ch)
                {
                    auto* data = buffer.getWritePointer (ch);

                    for (int n = 0; n < blockSize; ++n)
                        data[n] = 0.25f * (random.nextFloat() * 2.0f - 1.0f);
                }

                report.inputPeak = juce::jmax (report.inputPeak, buffer.getMagnitude (0, blockSize));
            }

            processor.processBlock (buffer, midi);

            float blockPeak = 0.0f;

            for (int ch = 0; ch < 2; ++ch)
            {
                const auto* data = buffer.getReadPointer (ch);

                for (int n = 0; n < blockSize; ++n)
                {
                    if (! std::isfinite (data[n]))
                        report.finite = false;

                    blockPeak = juce::jmax (blockPeak, std::abs (data[n]));
                }
            }

            envelope.push_back (blockPeak);
            report.outputPeak = juce::jmax (report.outputPeak, blockPeak);

            if (block >= burstBlocks)
                report.tailPeak = juce::jmax (report.tailPeak, blockPeak);
        }

        report.tailAtEnd = envelope.back();

        // How long the tail takes to fall 60 dB below its own peak, measured
        // from the moment the input stops.
        const auto threshold = report.tailPeak * 0.001f;
        int lastAbove = burstBlocks;

        for (int i = burstBlocks; i < (int) envelope.size(); ++i)
            if (envelope[(size_t) i] > threshold)
                lastAbove = i;

        report.decaySeconds = (double) (lastAbove - burstBlocks) * blockSize / testSampleRate;
        return report;
    }

    juce::String db (float gain)
    {
        return juce::String (juce::Decibels::gainToDecibels (gain, -144.0f), 1) + " dB";
    }

    bool runCase (const juce::String& name,
                  const std::function<void (juce::AudioProcessorValueTreeState&)>& configure)
    {
        DiamondRoomAudioProcessor processor;
        processor.setPlayConfigDetails (2, 2, testSampleRate, blockSize);

        if (configure != nullptr)
            configure (processor.getState());

        processor.prepareToPlay (testSampleRate, blockSize);

        const auto report = measureTail (processor);

        std::cout << name << "\n"
                  << "  input peak  : " << db (report.inputPeak) << "\n"
                  << "  output peak : " << db (report.outputPeak) << "\n"
                  << "  tail peak   : " << db (report.tailPeak) << "\n"
                  << "  tail -60 dB : " << report.decaySeconds << " s\n"
                  << "  at 10 s     : " << db (report.tailAtEnd) << "\n"
                  << "  latency     : " << processor.getLatencySamples() << " samples"
                  << std::endl;

        bool ok = true;

        auto fail = [&ok, &name] (const juce::String& why)
        {
            std::cerr << "  FAIL (" << name << "): " << why << std::endl;
            ok = false;
        };

        if (! report.finite)
            fail ("non-finite samples");

        if (report.outputPeak > 4.0f)
            fail ("output ran away, peak " + db (report.outputPeak));

        // A tail that is not clearly below its own peak after nine seconds of
        // silence means something in a feedback path is self-oscillating.
        if (report.tailAtEnd > report.tailPeak * 0.01f)
            fail ("tail is not decaying");

        if (report.tailPeak < report.inputPeak * 0.0005f)
            fail ("no audible reverb tail");

        return ok;
    }

    void setValue (juce::AudioProcessorValueTreeState& state, const char* id, float value)
    {
        if (auto* parameter = state.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    }

    int runAudioSelfTest()
    {
        bool ok = true;

        ok &= runCase ("default settings", nullptr);

        ok &= runCase ("everything maxed", [] (juce::AudioProcessorValueTreeState& state)
        {
            setValue (state, dr::params::drive, 10.0f);
            setValue (state, dr::params::tube, 10.0f);
            setValue (state, dr::params::mix, 100.0f);

            for (auto* id : { dr::params::hMix, dr::params::mMix,
                              dr::params::vMix, dr::params::tMix })
                setValue (state, id, 100.0f);

            setValue (state, dr::params::hTime, 10.0f);
            setValue (state, dr::params::vDecay, 10.0f);
            setValue (state, dr::params::mDist, 10.0f);
            setValue (state, dr::params::mAmount, 10.0f);
            setValue (state, dr::params::tRoomsize, 10.0f);
            setValue (state, dr::params::tDistance, 10.0f);
            setValue (state, dr::params::vHighCut, 10.0f);
            setValue (state, dr::params::hTone, 10.0f);
        });

        ok &= runCase ("all reverbs bypassed", [] (juce::AudioProcessorValueTreeState& state)
        {
            for (auto* id : { dr::params::hOn, dr::params::mOn,
                              dr::params::vOn, dr::params::tOn })
                setValue (state, id, 0.0f);
        });

        std::cout << (ok ? "PASS" : "FAILED") << std::endl;
        return ok ? 0 : 1;
    }
}

int main (int argc, char* argv[])
{
    const juce::ScopedJuceInitialiser_GUI juceInit;

    if (argc > 1 && juce::String (argv[1]) == "--audio")
        return runAudioSelfTest();

    if (argc > 1 && juce::String (argv[1]) == "--ui")
        return runUiSelfTest();

    const juce::File output = argc > 1
        ? juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[1]))
        : juce::File::getCurrentWorkingDirectory().getChildFile ("DiamondRoom.png");

    const auto width = argc > 2 ? juce::String (argv[2]).getIntValue() : 2001;

    DiamondRoomAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    if (editor == nullptr)
    {
        std::cerr << "no editor" << std::endl;
        return 1;
    }

    const auto height = juce::roundToInt ((double) width * 764.0 / 2001.0);
    editor->setSize (width, height);

    juce::Image image (juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);

    {
        juce::Graphics g (image);
        editor->paintEntireComponent (g, true);
    }

    output.deleteFile();

    if (auto stream = output.createOutputStream())
    {
        juce::PNGImageFormat png;

        if (png.writeImageToStream (image, *stream))
        {
            std::cout << "wrote " << output.getFullPathName() << " ("
                      << image.getWidth() << "x" << image.getHeight() << ")" << std::endl;
            return 0;
        }
    }

    std::cerr << "failed to write " << output.getFullPathName() << std::endl;
    return 1;
}
