/*
 *  HRAW - Hacker's toolkit for image sensor characterisation
 *  Copyright 2016-2018 Ciriaco Garcia de Celis
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
    ImageChannel::ptr plain = raw->getChannel(ImageFilter::RGB());
    ImageSelection::ptr maskedPixels = plain->select(0, 18, 42-2, imgsize_t(plain->height() - 18)); // Canon 400D
    ImageMath::Stats1 statsMasked;
    statsMasked = ImageMath::analyze(maskedPixels);
    std::cout << "read noise (DN) masked area: " << statsMasked.stdev << std::endl;

    // read noise from black frames ISO 100 (shoot with lens cap on)
    RawImage::ptr rawDarkA = RawImage::load("data/black/IMG_2762.pgm");
    RawImage::ptr rawDarkB = RawImage::load("data/black/IMG_2763.pgm");
    ImageChannel::ptr plainDarkA = rawDarkA->getChannel(ImageFilter::RGB());
    ImageChannel::ptr plainDarkB = rawDarkB->getChannel(ImageFilter::RGB());
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
        ImageFilter imageFilter = (cf == 0)? ImageFilter::R()  :
                                  (cf == 1)? ImageFilter::G1() :
                                  (cf == 2)? ImageFilter::G2() : ImageFilter::B();

        std::cout << "-------------------" << std::endl;
        std::cout << "Color filter " << (cf == 0? "red" : cf == 1? "green1" : cf == 2? "green2" : "blue") << std::endl;
        std::cout << "-------------------" << std::endl;

        ImageChannel::ptr channelWhiteA = rawWhiteA->getChannel(imageFilter);
        ImageChannel::ptr channelWhiteB = rawWhiteB->getChannel(imageFilter);
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
            ImageMath::Histogram::ptr histogram = ImageMath::buildHistogram(image == 0? imgWhiteA : imgWhiteB);
            ImageAlgo::Highlights highlights = ImageAlgo::getHighlights(histogram);
            std::cout << "  mode(pixels)=" << histogram->mode << "(" << histogram->data.at(histogram->mode)
                      << "), whiteLevel=" <<  highlights.whiteLevel << " ("
                      << double(highlights.clippedCount) / double(histogram->total) * 100 << "% clipped)" << std::endl;
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
        ImageChannel::ptr plainA = darkA->getChannel(ImageFilter::RGB());
        ImageChannel::ptr plainB = darkB->getChannel(ImageFilter::RGB());
        ImageSelection::ptr blackA = plainA->select();
        ImageSelection::ptr blackB = plainB->select();
        double noise = ImageMath::subtract(blackA, blackB).stdev;
        std::cout << "read noise ISO " << iso << ": " << noise << " (" << noise * gain << " e-); "
                  << "sensor dynamic range: " << std::log2(fullwell/noise/gain)
                  << " stops, gain: " << gain << std::endl;
        gain /= 2; fullwell /= 2;
    }
}

struct ExitNotif { std::string errMsg; };

struct Loop { int deltaX, deltaY, count; };

template <typename T, typename Iter> struct IterationKit
{
    T object;
    Iter cur, last;
};

void histogram2csv(const RawImage::ptr& image, const std::shared_ptr<ImageCrop>& crop)
{
    typedef IterationKit<ImageMath::Histogram::ptr, ImageMath::Histogram::Frequencies::const_iterator> HistoIter;
    std::vector<HistoIter> histoIter;
    double sumBlack = 0;
    auto wclip = image->whiteLevel; // note: when this parameter is provided a *FAKE* histogram is generated

    auto appendHistogram = [&](const ImageFilter& filter)
    {
        auto channel = image->getChannel(filter);
        auto area = channel->select(crop);
        sumBlack += image->hasBlackLevel()? channel->blackLevel() : 0;
        auto histogram = ImageMath::buildHistogram(area);
        if (wclip && !histogram->data.empty())
        {
            auto it = histogram->data.cbegin();
            auto itnext = ++it;
            while (itnext != histogram->data.cend()) // "smooth" the scaling if white clipping provided
            {
                if ((itnext->first - it->first == 2) && (!wclip || (itnext->first < *wclip)))
                    histogram->data.emplace(it->first + 1, (it->second + itnext->second) / 2);
                it = itnext;
                ++itnext;
            }
        }
        histoIter.emplace_back(HistoIter { histogram, histogram->data.cbegin(), histogram->data.cend() });
        std::cout << ";" << filter.code;
    };

    appendHistogram(ImageFilter::R());
    appendHistogram(ImageFilter::G1());
    appendHistogram(ImageFilter::G2());
    appendHistogram(ImageFilter::B());
    std::cout << std::endl;

    bitdepth_t blackLevel = bitdepth_t(0.5 + sumBlack / double(histoIter.size()));

    int64_t val = std::numeric_limits<int64_t>::max();
    for (const auto& i : histoIter) if (i.cur->first < val) val = i.cur->first; // starting point

    std::stringstream line;
    for (bool isEof = false; !isEof;)
    {
        line.str(std::string());
        isEof = true;
        for (auto i = histoIter.begin(); i != histoIter.end(); ++i)
        {
            if ((i->cur == i->last) || (val < i->cur->first)) line << ";0";
            else
            {
                line << ";" << i->cur->second;
                ++i->cur;
            }
            if (wclip && (*wclip == val)) i->last = i->cur;
            if (i->cur != i->last) isEof = false;
        }
        if (isEof && wclip) break;
        std::cout << (val - blackLevel) << line.str() << std::endl;
        val++;
    }

    if (wclip) // make overexposed area more "readable" if white clipping provided
    {
        auto overexp = val;
        val -= blackLevel;
        int zleft = 0;
        int zright = int(histoIter.size() - 1);
        for (auto i = histoIter.begin(); i != histoIter.end(); ++i)
        {
            line.str(std::string());
            for (auto c = 0; c < zleft; c++) line << ";0";
            auto expmax = i->last;
            --expmax;
            line << ";" << (expmax->first == overexp? expmax->second : 0);
            for (auto c = 0; c < zright; c++) line << ";0";
            int bwidth = int(double(val) * 0.02);
            for (auto right = val + bwidth; val < right; val++) std::cout << val << ";0;0;0;0" << std::endl;
            for (auto right = val + bwidth; val < right; val += 2)
                std::cout << val << line.str() << std::endl << (val + 1) << ";0;0;0;0" << std::endl;
            zleft++; zright--;
        }
    }
}

void stats(const RawImage::ptr& raw, const ImageFilter& analyzeChannel, const std::shared_ptr<ImageCrop>& crop)
{
    ImageChannel::ptr channel = raw->getChannel(analyzeChannel);
    ImageSelection::ptr area = channel->select(crop);
    auto stArea = ImageMath::analyze(area);
    auto whiteLevel = raw->whiteLevel? *raw->whiteLevel : stArea.max;
    auto blackLevel = raw->hasBlackLevel()? channel->blackLevel() : stArea.mean;
    double dr = log((whiteLevel - blackLevel) / stArea.stdev) / log(2);
    double mp = raw->pixelCount() / 1000000.0;
    double dr8 = dr + log(sqrt(mp/8)) / log(2);
    std::cout << "min;max;mean;stdev;DR@" << int(mp+0.5) << ";DR@8" << std::endl;
    std::cout << stArea.min << ";" << stArea.max << ";" << stArea.mean << ";" << stArea.stdev
              << ";" << dr << ";" << dr8 << std::endl;
}

void mskstats(const RawImage::ptr& raw, const ImageFilter& analyzeChannel)
{
    ImageChannel::ptr channel = raw->getChannel(analyzeChannel);
    ImageSelection::ptr maskedPixels = channel->getLeftMask();
    auto stMasked = ImageMath::analyze(maskedPixels);
    ImageSelection::ptr image = channel->select();
    auto stImage = ImageMath::analyze(image);
    auto whitePoint = raw->whiteLevel;
    double dr = log((1.0 * (whitePoint? *whitePoint : stImage.max) - stMasked.mean) / stMasked.stdev) / log(2);
    double mp = raw->pixelCount() / 1000000.0;
    double dr8 = dr + log(sqrt(mp/8)) / log(2);
    std::cout << "ReadNoise=" << stMasked.stdev << " DR@" << int(mp+0.5) << "=" << dr
              << " DR@8=" << dr8 << " file { " << raw->name << " }" << std::endl;
    std::cout << "image { mean=" << stImage.mean << " min=" << stImage.min << " max=" << stImage.max << " }"
              << " left mask { mean=" << stMasked.mean << " min=" << stMasked.min << " max=" << stMasked.max
              << " crop=" << maskedPixels->width << "x" << maskedPixels->height
                          << "+" << maskedPixels->x << "+" << maskedPixels->y << " }"
              << std::endl;
}

void rgbStats2csv(const RawImage::ptr& raw, const std::shared_ptr<ImageCrop>& crop, const std::shared_ptr<Loop>& loop)
{
    ImageChannel::ptr red = raw->getChannel(ImageFilter::R());
    ImageChannel::ptr gr1 = raw->getChannel(ImageFilter::G1());
    ImageChannel::ptr gr2 = raw->getChannel(ImageFilter::G2());
    ImageChannel::ptr blu = raw->getChannel(ImageFilter::B());

    imgsize_t cx = crop? crop->x : 0;
    imgsize_t cy = crop? crop->y : 0;

    imgsize_t width = crop? crop->width : gr1->width(); // from 1 (actually 4 RGGB) pixel to the entire image
    imgsize_t height = crop? crop->height : gr1->height();

    int deltaX = loop? loop->deltaX : 0; // movement in each axis
    int deltaY = loop? loop->deltaY : 0;

    int count = loop? loop->count : 1; // iterations count (less than 2: no movement)

    std::string csvpad = std::string(std::size_t((count > 1? 1 : 13) + (deltaX? 1 : 0) + (deltaY? 1 : 0)), ';');
    std::cout << "width;height;X;Y" << csvpad << std::endl
              << width << ";" << height << ";" << cx << ";" << cy << csvpad << std::endl << std::endl;

    if (count > 1)
        std::cout << (deltaX? "X;" : "") << (deltaY? "Y;" : "") << "R;G1;G2;B;" << std::endl; // only mean reported
    else
        std::cout << "R mean;R min;R max;R stdev;G1 mean;G1 min;G1 max;G1 stdev;"
                     "G2 mean;G2 min;G2 max;G2 stdev;B mean;B min;B max;B stdev;" << std::endl; // full stats

    auto black_red = raw->hasBlackLevel()? red->blackLevel() : 0;
    auto black_gr1 = raw->hasBlackLevel()? gr1->blackLevel() : 0;
    auto black_gr2 = raw->hasBlackLevel()? gr2->blackLevel() : 0;
    auto black_blu = raw->hasBlackLevel()? blu->blackLevel() : 0;

    for (int iter = count; iter > 0; iter--)
    {
        ImageSelection::ptr area = red->select(cx, cy, width, height);
        auto stats = ImageMath::analyze(area);
        if (deltaX) std::cout << cx << ";";
        if (deltaY) std::cout << cy << ";";
        std::cout << stats.mean - black_red;
        if (count < 2) std::cout << ";" << stats.min - black_red << ";" << stats.max - black_red << ";" << stats.stdev;
        area = gr1->select(cx, cy, width, height);
        stats = ImageMath::analyze(area);
        std::cout << ";" << stats.mean - black_gr1;
        if (count < 2) std::cout << ";" << stats.min - black_gr1 << ";" << stats.max - black_gr1 << ";" << stats.stdev;
        area = gr2->select(cx, cy, width, height);
        stats = ImageMath::analyze(area);
        std::cout << ";" << stats.mean - black_gr2;
        if (count < 2) std::cout << ";" << stats.min - black_gr2 << ";" << stats.max - black_gr2 << ";" << stats.stdev;
        area = blu->select(cx, cy, width, height);
        stats = ImageMath::analyze(area);
        std::cout << ";" << stats.mean - black_blu;
        if (count < 2) std::cout << ";" << stats.min - black_blu << ";" << stats.max - black_blu << ";" << stats.stdev;
        std::cout << ";" << std::endl;
        cx += imgsize_t(deltaX);
        cy += imgsize_t(deltaY);
    }

    std::cout << std::endl << std::endl;
}

int main(int argc, char **argv)
{
    try
    {
        int argument = 0;
        std::string command = String::tolower(argc > 1? argv[++argument] : "help");
        ImageAlgo::DPRAW::Action dprawAction = ImageAlgo::DPRAW::Action::GetA;
        ImageAlgo::DPRAW::ProcessMode dprawProcessMode = ImageAlgo::DPRAW::ProcessMode::Plain;

        std::string infile1;
        std::string infile2;
        RawImage::Masked::ptr opticalBlack;
        std::string outfile;
        std::shared_ptr<std::vector<double>> blackPoints;
        std::shared_ptr<bitdepth_t> whitePoint;
        std::shared_ptr<ImageFilter> channel;
        std::shared_ptr<double> ev;
        std::shared_ptr<ImageCrop> crop;
        std::shared_ptr<Loop> loop;

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
            else if (argname == "-m")
            {
                if (argument + 2 >= argc) throw ExitNotif { "-m requires the left and top mask numbers" };
                opticalBlack = std::make_shared<RawImage::Masked>();
                std::stringstream(argv[++argument]) >> opticalBlack->left;
                std::stringstream(argv[++argument]) >> opticalBlack->top;
            }
            else if (argname == "-o")
            {
                if (argument + 1 >= argc) throw ExitNotif { "-o requires a output file name" };
                outfile = argv[++argument];
            }
            else if (argname == "-b")
            {
                blackPoints = std::make_shared<std::vector<double>>(4);
                std::size_t cnt = 0;
                while ((cnt < 4) && (++argument < argc) && std::stringstream(argv[argument]) >> blackPoints->at(cnt)) cnt++;
                if (cnt == 1) --argument; else if (cnt != 4) throw ExitNotif { "-b requires one or four blackpoints" };
                blackPoints->resize(cnt);
            }
            else if (argname == "-w")
            {
                if (argument + 1 >= argc) throw ExitNotif { "-w requires a integer white point" };
                whitePoint = std::make_shared<bitdepth_t>();
                std::stringstream(argv[++argument]) >> *whitePoint;
            }
            else if (argname == "-c")
            {
                std::stringstream sch(argument + 1 >= argc? "" : String::toupper(argv[++argument]));
                ImageFilter::Code fc;
                sch >> fc;
                if (sch.fail()) throw ExitNotif { "-c requires R, G1, G2, G, B or RGB" };
                channel = std::make_shared<ImageFilter>(ImageFilter::create(fc));
            }
            else if (argname == "-ev")
            {
                if (argument + 1 >= argc) throw ExitNotif { "-ev requires a floating point number" };
                ev = std::make_shared<double>();
                std::stringstream(argv[++argument]) >> *ev;
            }
            else if (argname == "-crop")
            {
                if (argument + 4 >= argc) throw ExitNotif { "-crop requires: cx cy width height" };
                imgsize_t x, y, width, height;
                std::stringstream(argv[++argument]) >> x;
                std::stringstream(argv[++argument]) >> y;
                std::stringstream(argv[++argument]) >> width;
                std::stringstream(argv[++argument]) >> height;
                crop = std::shared_ptr<ImageCrop>(new ImageCrop { x, y, width, height });
            }
            else if (argname == "-loop")
            {
                if (argument + 2 >= argc) throw ExitNotif { "-loop requires deltaX, deltaY and count numbers" };
                loop = std::make_shared<Loop>();
                std::stringstream(argv[++argument]) >> loop->deltaX;
                std::stringstream(argv[++argument]) >> loop->deltaY;
                std::stringstream(argv[++argument]) >> loop->count;
            }
            else
            {
                throw ExitNotif { VA_STR("argument " << argname << " unknown") };
            }
        }

        if (command == "histogram")
        {
            if (infile1.empty()) throw ExitNotif { "missing input file" };
            RawImage::ptr raw = RawImage::load(infile1, opticalBlack);
            ImageAlgo::setBlackLevel(raw, blackPoints);
            ImageAlgo::setWhiteLevel(raw, whitePoint);
            histogram2csv(raw, crop);
        }
        else if (command == "zebras")
        {
            if (infile1.empty()) throw ExitNotif{ "missing input file" };
            if (outfile.empty())
            {
                auto ep = infile1.find_last_of(".");
                if (ep == std::string::npos) ep = infile1.length();
                outfile = infile1.substr(0, ep) + ".tiff";
            }
            if (!whitePoint) throw ExitNotif{ "missing white point" };
            RawImage::ptr raw = RawImage::load(infile1, opticalBlack);
            ImageAlgo::setBlackLevel(raw, blackPoints);
            if (!raw->hasBlackLevel()) throw ExitNotif{ "missing black point(s)" };
            ImageAlgo::setWhiteLevel(raw, whitePoint);
            RawImage::ptr result = ImageAlgo::zebras(raw);
            result->save(outfile);
        }
        else if (command == "stats")
        {
            if (infile1.empty()) throw ExitNotif { "missing input file" };
            auto raw = RawImage::load(infile1, opticalBlack);
            ImageAlgo::setBlackLevel(raw, blackPoints);
            ImageAlgo::setWhiteLevel(raw, whitePoint);
            stats(raw, channel? *channel : ImageFilter::RGB(), crop);
        }
        else if (command == "mskstats")
        {
            if (infile1.empty()) throw ExitNotif { "missing input file" };
            if (!channel) throw ExitNotif { "image channel must be specified" };
            if (!opticalBlack) throw ExitNotif { "left and top mask must be specified" };
            auto raw = RawImage::load(infile1, opticalBlack);
            ImageAlgo::setWhiteLevel(raw, whitePoint);
            mskstats(raw, *channel);
        }
        else if (command == "rgbstats")
        {
            if (infile1.empty()) throw ExitNotif { "missing input file" };
            RawImage::ptr raw = RawImage::load(infile1, opticalBlack);
            ImageAlgo::setBlackLevel(raw, blackPoints);
            rgbStats2csv(raw, crop, loop);
        }
        else if (command == "dpraw")
        {
            if (infile1.empty()) throw ExitNotif { "missing input file" };
            if (infile2.empty()) throw ExitNotif { "missing input file for secondary B image" };
            if (outfile.empty()) throw ExitNotif { "missing output file for result" };
            if (!whitePoint) throw ExitNotif { "white point must be specified" };
            if (!ev && (dprawAction != ImageAlgo::DPRAW::Action::GetA)) throw ExitNotif { "EV shift must be specified" };
            RawImage::ptr rawAB = RawImage::load(infile1, opticalBlack);
            RawImage::ptr rawB  = RawImage::load(infile2, opticalBlack);
            ImageAlgo::setBlackLevel(rawAB, blackPoints);
            ImageAlgo::setBlackLevel(rawB, blackPoints);
            ImageAlgo::DPRAW dpraw { rawAB, rawB, *whitePoint, ev };
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
            << "  HRAW v1.1 - Hacker's open source toolkit for image sensor characterisation" << std::endl
            << "              (c) 2016-2018 Ciriaco Garcia de Celis" << std::endl
            << std::endl
            << "    Commands:" << std::endl
            << "      histogram -i [-b|-m] [-w] [-crop]" << std::endl
            << "      zebras    -i -b|-m -w [-o(tiff/ppm)]" << std::endl
            << "      stats     -i [-c] [-b] [-w] [-crop]" << std::endl
            << "      mskstats  -i -c -m [-w]" << std::endl
            << "      rgbstats  -i [-b|-m] [-crop] [-loop]" << std::endl
            << "      dpraw      GetA|Blend Plain|Bayer -i2 AB_0.pgm B_1.pgm -o(dat/pgm) -m|-b -w [-ev]" << std::endl
            << std::endl
            << "    Arguments:" << std::endl
            << "      -i fileName.pgm            single input file" << std::endl
            << "      -i2 file1.pgm file2.pgm    two input files" << std::endl
            << "      -m leftMask topMask        masked pixels count (optical black area)" << std::endl
            << "      -o fileName.ext            output file (.dat .pgm .ppm or .tiff depending on command)" << std::endl
            << "      -b blackPoint(s)           a single floating point number or 4 (one for each channel)" << std::endl
            << "      -w whitePoint              integer number (black point not substracted)" << std::endl
            << "      -c R|G1|G2|G|B|RGB         color filter selection" << std::endl
            << "      -ev EV                     exposure adjust (positive or negative)" << std::endl
            << "      -crop cx cy width height   rectangle selection (bayer coordinates: half width & height)" << std::endl
            << "      -loop deltaX deltaY count  multiline output moving the selection" << std::endl
            << std::endl
            << "    Input PGM files previously generated from camera raw files with dcraw:" << std::endl
            << "      dcraw -D -4 -j -t 0 -s all  (plain non demosaiced raw image data)" << std::endl
            << "      dcraw -E -4 -j -t 0 -s all  (request including the masked pixels for the -m option)" << std::endl
            << std::endl
            << "    dpraw's output (.dat) image can also be piped to dcraw to be decoded:" << std::endl
            << "      cat fileName.dat | dcraw -k black -S white -W -w -v -I -c rawFile.cr2 > image.ppm" << std::endl
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
