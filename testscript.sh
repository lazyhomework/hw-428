#!bin/bash
echo "" > kil_list.txt

for i in {1,2}
do
	echo ./server -n $i
	FILENAME="servout${i}.txt"
	./server -n $i > $FILENAME &
	echo $! >> kil_list.txt
	
done
./server -n 0
