#! /bin/sh

rm -f ts ts.exe
gcc -o ts misc/ts.c
date=$(misc/getdate.sh)
./ts $date
rm -f ts ts.exe
