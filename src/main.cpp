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

#include <sstream>
#include <iostream>
#include <cmath>
#include "Util.hpp"
#include "RawImage.h"
#include "ImageMath.h"
#include "ImageAlgo.h"

void demo()
{
    // read noise from masked pixels (optical black area)
    RawImage::ptr raw = RawImage::load("data/misc/IMG_2597.pgm");
    ImageChannel::ptr plain = raw->getChannel(FilterPattern::FULL());
    ImageSelection::ptr maskedPixels = plain->select(0, 18, 42-2, imgsize_t(plain->height() - 18)); // Canon 400D
    ImageMath::Stats1 statsMasked;
    statsMasked = ImageMath::analyze(maskedPixels);
    std::cout << "read noise (DN) masked area: " << statsMasked.stdev << std::endl;

    // read noise from black frames ISO 100 (shoot with lens cap on)
    RawImage::ptr rawDarkA = RawImage::load("data/black/IMG_2762.pgm");
    RawImage::ptr rawDarkB = RawImage::load("data/black/IMG_2763.pgm");
    ImageChannel::ptr plainDarkA = rawDarkA->getChannel(FilterPattern::FULL());
    ImageChannel::ptr plainDarkB = rawDarkB->getChannel(FilterPattern::FULL());
    ImageMath::Stats2 blackStats = ImageMath::subtract(plainDarkA->select(), plainDarkB->select());
    double readNoise = blackStats.stdev;
    double blackLevel = (blackStats.a.mean + blackStats.b.mean) / 2;
    std::cout << "read noise (DN) whole frame: " << readNoise << std::endl;
    std::cout << "black level (DN): " << blackLevel << std::endl;

    // SNR from white frames ISO 100 (uniform illumination, close to overexposed but at least one channel not clipped)
    RawImage::ptr rawWhiteA = RawImage::load("data/snr/IMG_2790.pgm");
    RawImage::ptr rawWhiteB = RawImage::load("data/snr/IMG_2791.pgm");

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
            ImageAlgo::Highlights highlights = ImageAlgo::getHighlights(histogram);
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
        RawImage::ptr darkA = RawImage::load(VA_STR("data/black/IMG_" << pic++ << ".pgm"));
        RawImage::ptr darkB = RawImage::load(VA_STR("data/black/IMG_" << pic++ << ".pgm"));
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

void histogram2csv(const ImageMath::Histogram& hR, const ImageMath::Histogram& hG1,
                   const ImageMath::Histogram& hG2, const ImageMath::Histogram& hB)
{
    auto ir  = hR.data.cbegin();
    auto ig1 = hG1.data.cbegin();
    auto ig2 = hG2.data.cbegin();
    auto ib  = hB.data.cbegin();

    long v = std::min(std::min(std::min(ir->first, ig1->first), ig2->first), ib->first);

    while ((ir != hR.data.cend()) || (ig1 != hG1.data.cend()) ||
           (ig2 != hG2.data.cend()) || (ib != hB.data.cend()))
    {
        std::cout << v;
        if ((ir  == hR.data.cend())  || (v < ir->first))  std::cout << ";0"; else { std::cout << ";" << ir->second; ++ir; }
        if ((ig1 == hG1.data.cend()) || (v < ig1->first)) std::cout << ";0"; else { std::cout << ";" << ig1->second; ++ig1; }
        if ((ig2 == hG2.data.cend()) || (v < ig2->first)) std::cout << ";0"; else { std::cout << ";" << ig2->second; ++ig2; }
        if ((ib  == hB.data.cend())  || (v < ib->first))  std::cout << ";0"; else { std::cout << ";" << ib->second; ++ib; }
        std::cout << std::endl;
        v++;
    }
}

void noiseHistogram2csv(const std::string& fileName, imgsize_t leftMask, imgsize_t topMask)
{
    RawImage::ptr raw = RawImage::load(fileName.c_str(), leftMask, topMask);
    histogram2csv
    (
        ImageMath::buildHistogram(ImageAlgo::getLeftMask(raw, FilterPattern::RGGB_R())),
        ImageMath::buildHistogram(ImageAlgo::getLeftMask(raw, FilterPattern::RGGB_G1())),
        ImageMath::buildHistogram(ImageAlgo::getLeftMask(raw, FilterPattern::RGGB_G2())),
        ImageMath::buildHistogram(ImageAlgo::getLeftMask(raw, FilterPattern::RGGB_B()))
    );
}

void imageHistogram2csv(const std::string& fileName)
{
    RawImage::ptr raw = RawImage::load(fileName.c_str());
    histogram2csv
    (
        ImageMath::buildHistogram(raw->getChannel(FilterPattern::RGGB_R())->select()),
        ImageMath::buildHistogram(raw->getChannel(FilterPattern::RGGB_G1())->select()),
        ImageMath::buildHistogram(raw->getChannel(FilterPattern::RGGB_G2())->select()),
        ImageMath::buildHistogram(raw->getChannel(FilterPattern::RGGB_B())->select())
    );
}

void analyze(int iso, const FilterPattern& analyzeChannel, RawImage::ptr raw, int white)
{
    ImageChannel::ptr plain = raw->getChannel(analyzeChannel);
    ImageSelection::ptr maskedPixels = ImageAlgo::getLeftMask(raw, analyzeChannel);
    auto stMasked = ImageMath::analyze(maskedPixels);
    ImageSelection::ptr image = plain->select();
    auto stImage = ImageMath::analyze(image);
    double dr = log((1.0 * (white? white : stImage.max) - stMasked.mean) / stMasked.stdev) / log(2);
    double mp = raw->pixelCount() / 1000000.0;
    double dr8 = dr + log(sqrt(mp/8)) / log(2);
    if (iso) std::cout << "ISO " << iso << ": ";
    std::cout << "ReadNoise=" << stMasked.stdev << " DR@" << int(mp+0.5) << "=" << dr
              << " DR@8=" << dr8 << " file { " << "fileName" << " }" << std::endl;
    if (iso) std::cout << "ISO " << iso << ": ";
    std::cout << "image { mean=" << stImage.mean << " min=" << stImage.min << " max=" << stImage.max << " }"
              << " left mask { mean=" << stMasked.mean << " min=" << stMasked.min << " max=" << stMasked.max
              << " crop=" << maskedPixels->width << "x" << maskedPixels->height
                          << "+" << maskedPixels->x << "+" << maskedPixels->y << " }"
              << std::endl;
    if (iso) std::cerr << iso << ";" << dr8 << std::endl; // useful for a CSV file
}

struct ExitNotif { std::string errMsg; };

int main(int argc, char **argv)
{
    try
    {
        int argument = 0;
        std::string command = String::tolower(argc > 1? argv[++argument] : "help");
        ImageAlgo::DPRAW::Action dprawAction;
        ImageAlgo::DPRAW::ProcessMode dprawProcessMode;
        std::string infile1;
        std::string infile2;
        std::string outfile;
        std::shared_ptr<FilterPattern> channel;
        std::pair<imgsize_t, imgsize_t> mask { 0, 0 };
        int whitePoint = 0;
        int iso = 0;
        std::shared_ptr<double> ev;

        if (command == "dpraw")
        {
            if (argument + 1 >= argc) throw ExitNotif { "dpraw requires an action" };
            std::stringstream sact(argument + 1 >= argc? "" : String::toupper(argv[++argument]));
            sact >> dprawAction;
            if (sact.fail()) throw ExitNotif { "dpraw action must be GetA or Blend" };
            if (argument + 1 >= argc) throw ExitNotif { "dpraw requires a processing mode" };
            std::stringstream smod(argument + 1 >= argc? "" : String::toupper(argv[++argument]));
            smod >> dprawProcessMode;
            if (smod.fail()) throw ExitNotif { "dpraw processing mode must be Plain or Bayer" };
        }

        for (++argument; argument < argc; argument++)
        {
            std::string argname = String::tolower(argv[argument]);
            if (argname == "-i")
            {
                if (argument + 1 >= argc) throw ExitNotif { "-i requires a input file name" };
                infile1 = argv[++argument];
            }
            else if (argname == "-i2")
            {
                if (argument + 2 >= argc) throw ExitNotif { "-i2 requires two input file names" };
                infile1 = argv[++argument];
                infile2 = argv[++argument];
            }
            else if (argname == "-o")
            {
                if (argument + 1 >= argc) throw ExitNotif { "-o requires a output file name" };
                outfile = argv[++argument];
            }
            else if (argname == "-c")
            {
                std::stringstream sch(argument + 1 >= argc? "" : String::toupper(argv[++argument]));
                channel = std::make_shared<FilterPattern>();
                sch >> *channel;
                if (sch.fail()) throw ExitNotif { "-c requires R, G1, G2, G, B or RGB" };
            }
            else if (argname == "-m")
            {
                if (argument + 2 >= argc) throw ExitNotif { "-m requires the left and top mask numbers" };
                std::stringstream(argv[++argument]) >> mask.first;
                std::stringstream(argv[++argument]) >> mask.second;
            }
            else if (argname == "-w")
            {
                if (argument + 1 >= argc) throw ExitNotif { "-w requires a integer white point" };
                std::stringstream(argv[++argument]) >> whitePoint;
            }
            else if (argname == "-iso")
            {
                if (argument + 1 >= argc) throw ExitNotif { "-iso requires a integer number" };
                std::stringstream(argv[++argument]) >> iso;
            }
            else if (argname == "-ev")
            {
                if (argument + 1 >= argc) throw ExitNotif { "-ev requires a floating point number" };
                ev = std::make_shared<double>();
                std::stringstream(argv[++argument]) >> *ev;
            }
            else
            {
                throw ExitNotif { VA_STR("argument " << argname << " unknown") };
            }
        }

        if (command == "histogram")
        {
            if (infile1.empty()) throw ExitNotif { "missing input file" };
            if (mask.first)
                noiseHistogram2csv(infile1, mask.first, mask.second);
            else
                imageHistogram2csv(infile1);
        }
        else if (command == "analyze")
        {
            if (infile1.empty()) throw ExitNotif { "missing input file" };
            if (!channel) throw ExitNotif { "image channel must be specified" };
            if (!mask.first || !mask.second) throw ExitNotif { "left and top mask must be specified" };
            auto raw = RawImage::load(infile1, mask.first, mask.second);
            analyze(iso, *channel, raw, whitePoint);
        }
        else if (command == "dpraw")
        {
            if (infile1.empty()) throw ExitNotif { "missing input file" };
            if (infile2.empty()) throw ExitNotif { "missing input file for secondary B image" };
            if (outfile.empty()) throw ExitNotif { "missing output file for result" };
            if (!mask.first || !mask.second) throw ExitNotif { "left and top mask must be specified" };
            if (!whitePoint) throw ExitNotif { "white point must be specified" };
            if (!ev && (dprawAction != ImageAlgo::DPRAW::Action::GetA)) throw ExitNotif { "EV shift must be specified" };

            RawImage::ptr rawAB = RawImage::load(infile1, mask.first, mask.second);
            RawImage::ptr rawB  = RawImage::load(infile2, mask.first, mask.second);
            ImageAlgo::DPRAW dpraw { rawAB, rawB, whitePoint, ev };
            RawImage::ptr result = ImageAlgo::dprawProcess(dpraw, dprawAction, dprawProcessMode);
            result->save(outfile);
        }
        else
        {
            throw ExitNotif {};
        }
    }
    catch (ExitNotif& err)
    {
        if (err.errMsg.empty())
        {
            std::cout
            << std::endl
            << "  HRAW v1.0 - Hacker's open source toolkit for image sensor characterisation" << std::endl
            << "              (c) 2016 Ciriaco Garcia de Celis" << std::endl
            << std::endl
            << "    Arguments:" << std::endl
            << std::endl
            << "      histogram -i file.pgm -c R|G1|G2|G|B|RGB [-m leftMask topMask]" << std::endl
            << "      analyze   -i file.pgm -c R|G1|G2|G|B|RGB -m leftMask topMask [-w white] [-iso ISO]" << std::endl
            << "      dpraw      GetA|Blend Plain|Bayer -i2 f_0.pgm f_1.pgm -o out.dat -m leftMask topMask -w white -e EV"
            << std::endl << std::endl
            << "    PGM files previously generated from camera raw files with dcraw:" << std::endl
            << "      dcraw -E -4 -j -t 0 -s all  (with masked pixels)" << std::endl
            << "      dcraw -D -4 -j -t 0 -s all  (rest of uses)" << std::endl
            << std::endl
            << "    dpraw's output image can also be piped to dcraw to decode it:" << std::endl
            << "      cat out.dat | dcraw -k black -S white -W -w -v -I -c rawFile.cr2 > image.ppm" << std::endl
            << std::endl;
            return 1;
        }
        else
        {
            std::cout << std::endl << "ERROR: " << err.errMsg << std::endl
                      << "Run the application with no arguments for help" << std::endl << std::endl;
            return 2;
        }
    }
    catch (ImageException& e)
    {
        std::cout << e.what() << std::endl;
        return 3;
    }

    return 0;
}
