#! /bin/sh

export TZ=GMT

rm -f ts ts.exe
gcc -o ts src/timestamp.c > /dev/null 2>&1
date=`private/getdate.sh`
./ts $date
rm -f ts ts.exe
