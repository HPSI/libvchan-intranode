#!/bin/bash

# Plot results as a function of the ring size (512 to 1M) or the granularity size (512 to number of bytes)

INFILE=$1

for x in $(seq 9 24)
do
    bytes=`echo "2 ^ $x" | bc -l`
    #vs. ring size

    if [ $bytes -gt 1024 ]; then
        bytes_title=`echo "$bytes / 1024" | bc`KB
    else
        if [ $bytes -gt 1048576 ]; then
            bytes_title=`echo "$bytes / 1048576" | bc`MB
        else
            bytes_title="$bytes"KB
        fi
    fi
    for y in $(seq 9 $x)
    do 
        gran=`echo "2 ^ $y" | bc -l`
        #ring=`echo "2 ^ 9" | bc -l`
        FILE="${bytes}_${gran}"
        ./parse_results.py $INFILE ${bytes} ${gran} > results_${FILE}.txt
        cat template_results results_${FILE}.txt | sed s/'TITLE'/$bytes_title\ messages\ gran=${gran}/g | sed s/'XLABEL'/'Ring size (bytes)'/g > results_tmp
        perl bargraph.pl -eps results_tmp > results_${FILE}.eps
    done
    #vs. gran size
    for y in $(seq 9 20)
    do 
        if [ $y -gt $x ]; then
            continue
        fi
        ring=`echo "2 ^ $y" | bc -l`
        #ring=`echo "2 ^ 9" | bc -l`
        FILE="${bytes}_0_${ring}"
        ./parse_results.py $INFILE ${bytes} 0 ${ring} > results_${FILE}.txt
        cat template_results results_${FILE}.txt | sed s/'TITLE'/$bytes_title\ messages\ ring=${ring}/g | sed s/'XLABEL'/'Gran size (bytes)'/g > results_tmp
        perl bargraph.pl -eps results_tmp > results_${FILE}.eps
    done
done
