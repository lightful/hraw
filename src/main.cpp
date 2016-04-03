/*
 *  HRAW - Hacker's toolkit for image sensor characterisation
 *  Copyright 2016 Ciriaco Garcia de Celis
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>
#include <cmath>
#include "Util.hpp"
#include "RawImage.h"
#include "ImageMath.h"

/* PGM images generated from RAW files with:
 *
 *    dcraw -E -4 -j -t 0  (masked pixels)
 *    dcraw -D -4 -j -t 0  (rest of uses)
 */
int main()
{
    try
    {
        // read noise from masked pixels (optical black area)
        RawImage::ptr raw = RawImage::fromPGM("data/misc/IMG_2597.pgm");
        ImageChannel::ptr plain = raw->getChannel(FilterPattern::FULL());
        ImageSelection::ptr maskedPixels = plain->select(0, 18, 42-2, imgsize_t(raw->height - 18)); // Canon 400D
        ImageMath::Stats1 statsMasked;
        statsMasked = ImageMath::analyze(maskedPixels);
        std::cout << "read noise (DN) masked area: " << statsMasked.stdev << std::endl;

        // read noise from black frames ISO 100 (shoot with lens cap on)
        RawImage::ptr rawDarkA = RawImage::fromPGM("data/black/IMG_2762.pgm");
        RawImage::ptr rawDarkB = RawImage::fromPGM("data/black/IMG_2763.pgm");
        ImageChannel::ptr plainDarkA = rawDarkA->getChannel(FilterPattern::FULL());
        ImageChannel::ptr plainDarkB = rawDarkB->getChannel(FilterPattern::FULL());
        ImageMath::Stats2 blackStats = ImageMath::subtract(plainDarkA->select(), plainDarkB->select());
        double readNoise = blackStats.stdev;
        double blackLevel = (blackStats.a.mean + blackStats.b.mean) / 2;
        std::cout << "read noise (DN) whole frame: " << readNoise << std::endl;
        std::cout << "black level (DN): " << blackLevel << std::endl;

        // SNR from white frames ISO 100 (uniform illumination, close to overexposed but at least one channel not clipped)
        RawImage::ptr rawWhiteA = RawImage::fromPGM("data/snr/IMG_2790.pgm");
        RawImage::ptr rawWhiteB = RawImage::fromPGM("data/snr/IMG_2791.pgm");

        double maxSignal = 0, maxSignalSaturation = 0;
        bitdepth_t maxWhiteLevel = 0;

        for (int cf = 0; cf < 4; cf++)
        {
            FilterPattern filterPattern = (cf == 0)? FilterPattern::RGGB_R()  :
                                          (cf == 1)? FilterPattern::RGGB_G1() :
                                          (cf == 2)? FilterPattern::RGGB_G2() : FilterPattern::RGGB_B();

            std::cout << "-------------------" << std::endl;
            std::cout << "Color filter " << (cf == 0? "red" : cf == 1? "green1" : cf == 2? "green2" : "blue") << std::endl;
            std::cout << "-------------------" << std::endl;

            ImageChannel::ptr channelWhiteA = rawWhiteA->getChannel(filterPattern);
            ImageChannel::ptr channelWhiteB = rawWhiteB->getChannel(filterPattern);
            ImageSelection::ptr imgWhiteA = channelWhiteA->select();
            ImageSelection::ptr imgWhiteB = channelWhiteB->select();

            ImageMath::Stats2 whiteStats = ImageMath::subtract(imgWhiteA, imgWhiteB);

            double noise = whiteStats.stdev;
            std::cout << "pixels: " << imgWhiteA->width * imgWhiteA->height << std::endl;
            std::cout << "noise (DN): " << noise << std::endl;
            double photonNoise = std::sqrt(std::pow(noise, 2) - std::pow(readNoise, 2));
            std::cout << "photon noise (DN): " << photonNoise << std::endl;

            for (int image = 0; image < 2; image++)
            {
                std::cout << (image == 0? "Image A" : "Image B") << std::endl;
                const ImageMath::Stats1& stats = image == 0? whiteStats.a : whiteStats.b;
                std::cout << "  minDN=" << stats.min << ", maxDN=" << stats.max
                          << ", meanDN=" << stats.mean << ", stdevDN=" << stats.stdev << std::endl;
                ImageMath::Histogram histogram = ImageMath::buildHistogram(image == 0? imgWhiteA : imgWhiteB);
                ImageMath::Highlights highlights = ImageMath::getHighlights(histogram);
                std::cout << "  mode(pixels)=" << histogram.mode << "(" << histogram.data.at(histogram.mode)
                          << "), whiteLevel=" <<  highlights.whiteLevel << " ("
                          << double(highlights.clippedCount) / double(histogram.total) * 100 << "% clipped)" << std::endl;
                if (highlights.whiteLevel > maxWhiteLevel) maxWhiteLevel = highlights.whiteLevel;
                if (highlights.clippedCount == 0)
                {
                    double meanSaturation = stats.mean - blackLevel;
                    std::cout << "  mean saturation (DN): " << meanSaturation << std::endl;
                    double dynamicRange = std::log2(meanSaturation/readNoise);
                    std::cout << "  image dynamic range (stops): " << dynamicRange << std::endl;
                    double snr = meanSaturation / noise;
                    std::cout << "  snr: " << snr << " (" << 20*std::log10(snr) << " dB)" << std::endl;
                    double signal = std::pow(meanSaturation / photonNoise, 2);
                    std::cout << "  signal (e-): " << signal << std::endl;
                    if (signal > maxSignal)
                    {
                        maxSignal = signal;
                        maxSignalSaturation = meanSaturation;
                    }
                }
            }
        }

        double fullwell = ((maxWhiteLevel - blackLevel) / maxSignalSaturation) * maxSignal;
        double gain = fullwell / maxSignalSaturation;
        std::cout << "-------------------" << std::endl;
        std::cout << "sensor full well (e-): " << fullwell << std::endl;
        std::cout << "ISO 100 gain (e-/DN): " << gain << std::endl;

        // read noise from black frames ISO 100-1600
        std::cout << "-------------------" << std::endl;
        int pic = 2762;
        for (int iso = 100; iso <= 1600; iso *= 2)
        {
            RawImage::ptr darkA = RawImage::fromPGM(VA_STR("data/black/IMG_" << pic++ << ".pgm"));
            RawImage::ptr darkB = RawImage::fromPGM(VA_STR("data/black/IMG_" << pic++ << ".pgm"));
            ImageChannel::ptr plainA = darkA->getChannel(FilterPattern::FULL());
            ImageChannel::ptr plainB = darkB->getChannel(FilterPattern::FULL());
            ImageSelection::ptr blackA = plainA->select();
            ImageSelection::ptr blackB = plainB->select();
            double noise = ImageMath::subtract(blackA, blackB).stdev;
            std::cout << "read noise ISO " << iso << ": " << noise << " (" << noise * gain << " e-); "
                      << "sensor dynamic range: " << std::log2(fullwell/noise/gain)
                      << " stops, gain: " << gain << std::endl;
            gain /= 2; fullwell /= 2;
        }
    }
    catch (ImageException& e)
    {
        std::cout << e.what() << std::endl;
        return 1;
    }
    return 0;
}
