/*
    Development helper. Instantiates the plugin and either paints its editor
    into a PNG without opening a window, so the panel artwork can be reviewed
    from a build step, or runs the DSP offline and checks the tail behaves. Not
    part of the shipping plugin - see the DIAMONDROOM_BUILD_SCREENSHOT option in
    CMakeLists.txt.

    Usage: DiamondRoomShot <output.png> [width]
           DiamondRoomShot --audio          (offline DSP self-test)
           DiamondRoomShot --ui             (fader geometry self-test)
           DiamondRoomShot --drive          (Drive harmonic analysis)
*/

#include <iomanip>
#include <map>

#include "../Source/PluginProcessor.h"
#include "../Source/dsp/OneKnobDriver.h"
#include "../Source/gui/MetalSlider.h"

namespace
{
    //==========================================================================
    /**
        Measures what the Drive stage actually does to a sine: how much even and
        odd harmonic content it generates, whether it holds its level once the
        curve saturates, and whether the tone gets darker rather than brighter.
    */
    struct DriveSpectrum
    {
        float fundamentalDb = 0.0f;
        float evenDb = 0.0f;      // 2nd + 4th, relative to the fundamental
        float oddDb = 0.0f;       // 3rd + 5th, relative to the fundamental
        float levelChangeDb = 0.0f;
        float tiltDb = 0.0f;      // high band against low band, on noise
    };

    DriveSpectrum measureDrive (float driveAmount)
    {
        constexpr double rate = 48000.0;
        constexpr int block = 512;
        constexpr int fftOrder = 15;
        constexpr int fftSize = 1 << fftOrder;

        // A bin-centred test tone, so the harmonics land in single bins and
        // nothing has to be windowed away.
        constexpr int fundamentalBin = 683;
        constexpr double toneHz = fundamentalBin * rate / fftSize;
        constexpr float amplitude = 0.126f;   // -18 dBFS

        dr::OneKnobDriver driver;
        driver.prepare ({ rate, (juce::uint32) block, 2 });
        driver.setDrive (driveAmount);

        juce::AudioBuffer<float> buffer (2, block);
        std::vector<float> captured;
        captured.reserve ((size_t) fftSize);

        double phase = 0.0;
        const auto phaseStep = juce::MathConstants<double>::twoPi * toneHz / rate;

        // One buffer of warm-up so the filters and the smoothed gains settle,
        // then one buffer captured.
        const auto totalSamples = fftSize * 2;

        for (int written = 0; written < totalSamples; written += block)
        {
            for (int n = 0; n < block; ++n)
            {
                const auto s = amplitude * (float) std::sin (phase);
                phase += phaseStep;

                for (int ch = 0; ch < 2; ++ch)
                    buffer.setSample (ch, n, s);
            }

            juce::dsp::AudioBlock<float> audioBlock (buffer);
            driver.process (audioBlock);

            if (written >= fftSize)
                for (int n = 0; n < block && (int) captured.size() < fftSize; ++n)
                    captured.push_back (buffer.getSample (0, n));
        }

        std::vector<float> fftData ((size_t) fftSize * 2, 0.0f);
        std::copy (captured.begin(), captured.end(), fftData.begin());

        juce::dsp::FFT fft (fftOrder);
        fft.performFrequencyOnlyForwardTransform (fftData.data());

        auto binMag = [&fftData] (int bin) { return fftData[(size_t) bin]; };

        const auto fundamental = juce::jmax (1.0e-12f, binMag (fundamentalBin));
        const auto even = binMag (fundamentalBin * 2) + binMag (fundamentalBin * 4);
        const auto odd  = binMag (fundamentalBin * 3) + binMag (fundamentalBin * 5);

        float weighted = 0.0f, total = 0.0f;

        for (int bin = 1; bin < fftSize / 2; ++bin)
        {
            const auto mag = binMag (bin);
            weighted += mag * (float) (bin * rate / fftSize);
            total += mag;
        }

        double sumSquares = 0.0;
        for (auto s : captured)
            sumSquares += (double) s * s;

        const auto outRms = (float) std::sqrt (sumSquares / juce::jmax<size_t> (1, captured.size()));
        const auto inRms = amplitude * juce::MathConstants<float>::sqrt2 * 0.5f;

        DriveSpectrum result;
        result.fundamentalDb = juce::Decibels::gainToDecibels (fundamental, -144.0f);
        result.evenDb = juce::Decibels::gainToDecibels (even / fundamental, -144.0f);
        result.oddDb  = juce::Decibels::gainToDecibels (odd / fundamental, -144.0f);
        result.levelChangeDb = juce::Decibels::gainToDecibels (outRms / inRms, -144.0f);
        juce::ignoreUnused (weighted, total);
        return result;
    }

    /** High-band against low-band energy on noise, i.e. how bright the stage is. */
    float measureTilt (float driveAmount)
    {
        constexpr double rate = 48000.0;
        constexpr int block = 512;
        constexpr int fftOrder = 15;
        constexpr int fftSize = 1 << fftOrder;

        dr::OneKnobDriver driver;
        driver.prepare ({ rate, (juce::uint32) block, 2 });
        driver.setDrive (driveAmount);

        juce::AudioBuffer<float> buffer (2, block);
        juce::Random random (0x0d217e);
        std::vector<float> captured;
        captured.reserve ((size_t) fftSize);

        for (int written = 0; written < fftSize * 2; written += block)
        {
            for (int n = 0; n < block; ++n)
            {
                const auto s = 0.126f * (random.nextFloat() * 2.0f - 1.0f);

                for (int ch = 0; ch < 2; ++ch)
                    buffer.setSample (ch, n, s);
            }

            juce::dsp::AudioBlock<float> audioBlock (buffer);
            driver.process (audioBlock);

            if (written >= fftSize)
                for (int n = 0; n < block && (int) captured.size() < fftSize; ++n)
                    captured.push_back (buffer.getSample (0, n));
        }

        std::vector<float> fftData ((size_t) fftSize * 2, 0.0f);
        std::copy (captured.begin(), captured.end(), fftData.begin());

        juce::dsp::FFT fft (fftOrder);
        fft.performFrequencyOnlyForwardTransform (fftData.data());

        auto bandEnergy = [&fftData, fftSize, rate] (double lowHz, double highHz)
        {
            const auto firstBin = (int) (lowHz * fftSize / rate);
            const auto lastBin  = (int) (highHz * fftSize / rate);

            double sum = 0.0;
            for (int bin = firstBin; bin <= lastBin; ++bin)
                sum += (double) fftData[(size_t) bin] * fftData[(size_t) bin];

            return sum;
        };

        const auto low = bandEnergy (100.0, 1000.0);
        const auto high = bandEnergy (6000.0, 16000.0);

        return (float) (10.0 * std::log10 (juce::jmax (1.0e-30, high)
                                             / juce::jmax (1.0e-30, low)));
    }

    int runDriveSelfTest()
    {
        std::cout << "drive   even(2+4)   odd(3+5)   level      HF tilt" << std::endl;

        std::map<int, DriveSpectrum> results;

        for (const auto amount : { 0, 3, 6, 10 })
        {
            auto spectrum = measureDrive ((float) amount);
            spectrum.tiltDb = measureTilt ((float) amount);
            results[amount] = spectrum;

            std::cout << "  " << std::setw (2) << amount
                      << "   " << std::setw (8) << juce::String (spectrum.evenDb, 1).toStdString()
                      << "   " << std::setw (8) << juce::String (spectrum.oddDb, 1).toStdString()
                      << "   " << std::setw (7) << (juce::String (spectrum.levelChangeDb, 1) + " dB").toStdString()
                      << "   " << std::setw (8) << (juce::String (spectrum.tiltDb, 1) + " dB").toStdString()
                      << std::endl;
        }

        bool ok = true;

        auto fail = [&ok] (const juce::String& why)
        {
            std::cerr << "  FAIL: " << why << std::endl;
            ok = false;
        };

        const auto& clean = results[0];
        const auto& driven = results[10];

        if (clean.evenDb > -60.0f || clean.oddDb > -60.0f)
            fail ("stage is not clean at Drive 0");

        // Both families have to be there: odd from the curve, even from its
        // asymmetry. Without the bias the even column collapses.
        if (driven.evenDb < -40.0f)
            fail ("no even harmonics at Drive 10");

        if (driven.oddDb < -40.0f)
            fail ("no odd harmonics at Drive 10");

        // Saturating must not cost level, which is what a makeup derived from
        // the input gain rather than the shaper's response would do.
        if (std::abs (driven.levelChangeDb) > 6.0f)
            fail ("level moves " + juce::String (driven.levelChangeDb, 1) + " dB at Drive 10");

        // Measured on noise, not on the sine: adding harmonics to a sine
        // raises its centroid no matter how dark the stage is.
        if (driven.tiltDb > clean.tiltDb - 6.0f)
            fail ("driving does not darken the tone, tilt "
                    + juce::String (clean.tiltDb, 1) + " -> " + juce::String (driven.tiltDb, 1) + " dB");

        std::cout << (ok ? "PASS" : "FAILED") << std::endl;
        return ok ? 0 : 1;
    }

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

        // Ratio of the last second of tail to a second three seconds earlier.
        // A decaying tail shrinks; one that is self-oscillating holds or grows.
        float lateGrowth = 0.0f;
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

        auto windowPeak = [&envelope] (int firstBlock, int numBlocks)
        {
            float peak = 0.0f;

            for (int i = firstBlock; i < juce::jmin (firstBlock + numBlocks, (int) envelope.size()); ++i)
                peak = juce::jmax (peak, envelope[(size_t) i]);

            return peak;
        };

        constexpr int oneSecond = (int) (testSampleRate / blockSize);
        const auto lastWindow   = windowPeak (totalBlocks - oneSecond, oneSecond);
        const auto earlyWindow  = windowPeak (totalBlocks - oneSecond * 4, oneSecond);

        report.lateGrowth = earlyWindow > 1.0e-12f ? lastWindow / earlyWindow : 0.0f;
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
                  << "  late growth : " << report.lateGrowth << "x over 3 s\n"
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

        // Long settings legitimately still ring after nine seconds - a 10 s
        // RT60 is supposed to - so what matters is whether the envelope is
        // still falling, not how far down it has got. A feedback path that
        // oscillates holds its level or climbs back up instead.
        if (report.tailAtEnd > report.tailPeak)
            fail ("tail never fell below its own peak");

        if (report.lateGrowth > 0.7f)
            fail ("tail stopped decaying, growth " + juce::String (report.lateGrowth, 3) + "x over 3 s");

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

    if (argc > 1 && juce::String (argv[1]) == "--drive")
        return runDriveSelfTest();

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
