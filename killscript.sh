#!bin/bash
for i in $(<kil_list.txt)
do
	kill -9 $i
done
