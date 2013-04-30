#!/bin/sh
echo "" > kil_list.txt

for i in 0 1 2 3 4 5
do
	echo ./server -n $i
	FILENAME="servout${i}.txt"
	./server -n $i > $FILENAME &
	echo $! >> kil_list.txt
	
done
#./server -n 0
