#!/bin/bash

npads="0 1 3 7 15 31 63 127"
CC=gcc

for npad in ${npads}
do
	echo ${npad}
	echo "${CC} -lm -DNPAD=${npad} -o cache-analyse_npad-${npad} cache-analyse.c"
	${CC} -lm cache-analyse.c -DNPAD=${npad} -o cache-analyse_npad-${npad}
	./cache-analyse_npad-${npad} > cache-analyse_npad-${npad}.log
done
