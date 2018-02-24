#!/usr/bin/bash
while getopts "b:w:" opt; do
    case $opt in
        b) black="-b $OPTARG" ;;
        w) white="-w $OPTARG" ;;
    esac
done
for f in "$@"
do
    if [ -f "${f}" ]; then
        dcraw -D -4 -j -t 0 "${f}"
        levels=$(hraw clipping ${black} ${white} -v -i "${f%.*}.pgm" -o "${f%.*}_clip.tiff")
        echo "${f}: ${levels}"
        eval ${levels}
        convert "${f%.*}_clip.tiff" "${f%.*}_clip.jpg"
        rm "${f%.*}_clip.tiff"
        exiftool -q -overwrite_original -TagsFromFile "${f}" "${f%.*}_clip.jpg" > /dev/null 2>&1
        hraw histogram -b ${BlackLevel} -w ${WhiteLevel} -i "${f%.*}.pgm" > "${f%.*}.csv"
        gnuplot -c ~/etc/rawhisto.plot "${f%.*}.csv" 2> /dev/null
        rm "${f%.*}.csv"
        hraw histogram -i "${f%.*}.pgm" > "${f%.*}_hist.csv"
        rm "${f%.*}.pgm"
    fi
done
