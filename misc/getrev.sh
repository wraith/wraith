#! /bin/sh

parse_info() {
  echo "$info" | grep "Last Changed Rev" | sed -e 's/.*Last Changed Rev: \(.*\).*/\1/'
}

info=$(svn info 2> /dev/null)
if [ 0 -eq $? ]; then
  rev=$(parse_info "$info")
else
# All of this queries the local svk cache
#  DEPOTPATH=$(svk info |grep "Depot Path:"|sed -e 's/Depot Path: \/\/\(.*\)$/\1/')
#  url="file://$HOME/.svk/local/$DEPOTPATH"
# Or from the mirrored repo
#  url=$(svk info|grep "Mirrored From"|sed -e 's/Mirrored From: \(.*\),.*/\1/')
#  info=$(svn info $url)
#  rev=$(parse_info "$info")
  rev=$(svk info|grep "Mirrored From"|sed -e 's/Mirrored From: .*, Rev. \([0-9]*\)/\1/')
fi
echo $rev
