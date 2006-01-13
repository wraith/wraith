#! /bin/sh

gcc -Wall -O2 -o ts misc/ts.c
./ts `misc/getdate.sh`
rm -f ts ts.exe
