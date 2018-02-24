rawhisto=ARG1
fname=rawhisto[:strstrt(rawhisto, '.')-1]
set title fname.' RAW Histogram'
#set term svg
set terminal png size 1280,640 noenhanced font "DroidSans-Bold,12"
set output fname.'_hist.png'
set datafile separator ";"

vmaxy = 0
do for [i=2:5] {
    stats rawhisto using i name 'yv' nooutput
    if (yv_max > vmaxy) { vmaxy = yv_max }
}

#set key inside right top horizontal tc variable
unset key

set style data lines
set tics out

unset ytics
set y2tics
set format y2 "%9.0f"
set logscale y2 2
set logscale x 2
set for [i=0:16] xtics (0,2**i)
set for [i=0:16] y2tics (0,2**(i*2))
set y2range[:2**(1+int(log(vmaxy)/log(2)))]

plot rawhisto using 1:2 lc rgb "red" title "R" axis x1y2, \
     rawhisto using 1:3 lc rgb "#00F000" title "G1" axis x1y2, \
     rawhisto using 1:4 lc rgb "#008000" title "G2" axis x1y2, \
     rawhisto using 1:5 lc rgb "blue" title "B" axis x1y2
