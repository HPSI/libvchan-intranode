#!/bin/bash

sizes="512 1024 2048 4096 8192 16384 32768 65536 131072 262144 524288 1048576 2097152 4194304 8388608 16777216"
xs_path="/local/domain/1/memory"

for bytes in $sizes
do
	echo bytes=$bytes
	echo -n
	echo -n

	for rings in $sizes
	do
		echo ring=$rings
		echo -n
		echo -n

		if [ $rings -gt $bytes ]; then
			break
		fi
		for gran in $sizes
		do
			echo granularity=$gran
			echo -n
			echo -n

			if [ $gran -gt $bytes ]; then
				break
			fi
			./communication -s -w -d 2 -x $xs_path -b $bytes -l $rings -g $gran > out
		done
	
	done
done
