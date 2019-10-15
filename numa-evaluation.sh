#!/bin/bash

bench_bin="./cache-analyse"
bench_opt="-p random -m 134217728 -M 134217728"
iterations=5

nodes=$(numactl --show | grep physcpubind | sed -e "s/physcpubind: //")
mnodes=$(numactl --show | grep membind | sed -e "s/membind: //")
latency_cycle_log="numa-evaluation.cycle.dat"
latency_time_log="numa-evaluation.latency.dat"

echo -e "mnode\tpyscpu\t$(seq -s '\t' $iterations)" > $latency_cycle_log
echo -e "mnode\tpyscpu\t$(seq -s '\t' $iterations)" > $latency_time_log

for mnode in $mnodes
do
	for node in $nodes
	do 
		echo -e -n "$mnode\t$node\t"
		echo -e -n "$mnode\t$node\t" >> $latency_cycle_log
		echo -e -n "$mnode\t$node\t" >> $latency_time_log
		for i in $(seq $iterations)
		do
			numactl --membind=$mnode --physcpubind=$node $bench_bin $bench_opt
			res=$(awk '$1 == 134217728  {print $4}' *.log)
			echo -e -n "$res\t" >> $latency_cycle_log
			echo -e -n "$res\t"
			res=$(awk '$1 == 134217728  {print 1.0/$3}' *.log)
			echo -e -n "$res\t" >> $latency_time_log
			echo -e -n "$res\t"
		done
		echo "" >> $latency_cycle_log
		echo "" >> $latency_time_log
		echo ""
	done
done
