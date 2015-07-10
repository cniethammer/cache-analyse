#!/bin/bash

bench_bin="./cache-analyse"
bench_opt="-p random -m 134217728 -M 134217728"
iterations=5

nodes=$(numactl --show | grep physcpubind | sed -e "s/physcpubind: //")
mnodes=$(numactl --show | grep membind | sed -e "s/membind: //")

echo -e "mnode\tpyscpu\t$(seq -s '\t' $iterations)"
for mnode in $mnodes
do
	for node in $nodes
	do 
		echo -e -n "$mnode\t$node\t"
		for i in $(seq $iterations)
		do
			numactl --membind=$mnode --physcpubind=$node $bench_bin $bench_opt
			res=$(awk '$1 == 134217728  {print $4}' *.log)
			echo -e -n "$res\t"
		done
		echo ""
	done
done
