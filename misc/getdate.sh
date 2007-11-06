#! /usr/bin/env bash

parse_date() {
  echo "$1" | grep "Last Changed Date" | sed "s/Last Changed Date: \(.* \)[-+].*$/\1/"
}

info=$(svn info 2> /dev/null)
if [ 0 -eq $? ]; then
  date=$(parse_date "$info")
else
  url=$(svk info|grep "Mirrored From"|sed -e 's/Mirrored From: \(.*\),.*/\1/')

  info=$(svn info $url)
  date=$(parse_date "$info")
fi
echo $date

