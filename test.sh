#!/bin/bash

npads="0 8 16 24 32 40 56 120"
#npads="0 8 16 24 32 40 56 120 128 248 256"

for pad in ${npads}
do
	echo -n "Padding: ${pad} ... "
    START=$(date +%s.%N)
	make -B NPAD=${pad} cache-analyse > /dev/null
	./cache-analyse
    END=$(date +%s.%N)
    DIFF=$(echo "$END - $START" | bc)
    echo "took $DIFF"
done
