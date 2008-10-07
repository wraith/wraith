#! /bin/bash

langfile="$1"
for define in $(grep "#define.*\"" $langfile|awk '{print $2}'); do
  str="$(grep "#define \<${define}\>.*\"" $langfile|grep -v STR|sed -e 's/.*"\(.*\)"/\1/')"
  # Cleanup newline
  str="$(echo "$str"|sed -e 's/\\n/\\\\n/g')"
  # Cleanup /
#  str="$(echo "$str"|sed -e 's:/:\/:g')"
  str=${str/\//\\/}
  str=${str/(/\\(}
  str=${str/)/\\)}
  str=${str/]/\\]}
  str=${str/[/\\[}
  replaced=0
  echo "$define -> $str"
  for file in $(find src -type f \! -name $(basename $langfile) -name "*.c"|grep -v svn); do 
    grep "\<$define\>" $file > /dev/null 2>&1 && sed -i -e "s/\<$define\>/\"${str}\"/g" $file && echo "replace $define in $file" && replaced=1
    #grep "[,(?:]*\<$define\>" $file > /dev/null 2>&1 && sed -i -e "s/\([,(?:]\) *\<$define\>/\1 \"${str}\"/g" $file && echo "replace $define in $file" && replaced=1
  done

  if [ $replaced -eq 1 ]; then
    cp $langfile tmp
    grep -v "#define \<${define}\>" tmp > $langfile
  fi
done
