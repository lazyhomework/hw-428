#!/bin/sh
echo "" > kil_list.txt

for i in 1 2 3
do
	echo ./server -n $i
	FILENAME="servout${i}.txt"
	./server -n $i > $FILENAME &
	echo $! >> kil_list.txt
	
done
valgrind ./server -n 0
wait
