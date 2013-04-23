#!/bin/sh
for i in $(<kil_list.txt)
do
	kill -1 $i
done
