#! /bin/sh
svn info | grep "Last Changed Date" | sed "s/.*: \(.* \)-.*/\1/"

