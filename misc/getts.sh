#! /bin/sh

rm -f ts ts.exe
gcc -o ts misc/ts.c > /dev/null 2>&1
date=$(misc/getdate.sh)
./ts $date
rm -f ts ts.exe
