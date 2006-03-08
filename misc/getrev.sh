#! /bin/sh
svn info | grep "Last Changed Rev" | sed -e 's/^Last Changed Rev: \(.*\)$/\1/'
