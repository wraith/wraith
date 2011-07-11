#! /bin/bash

echo "Generating lib symbols"

mkdir -p src/.defs > /dev/null 2>&1

for file in $(grep -l DLSYM_GLOBAL src/*.c|grep -v "src/_"); do
  defsFile_wrappers="src/.defs/$(basename $file .c)_defs.c"
  defsFile_pre="src/.defs/$(basename $file .c)_pre.h"
  defsFile_post="src/.defs/$(basename $file .c)_post.h"

  rm -f $defsFile_pre $defsFile_post > /dev/null 2>&1

  echo "extern \"C\" {" > $defsFile_wrappers
  echo "extern \"C\" {" > $defsFile_post

  for symbol in $(sed -n -e 's/.*DLSYM_GLOBAL(.*, \([^)]*\).*/\1/p' $file|sort -u); do
    echo "#define ${symbol} ORIGINAL_SYMBOL_${symbol}" >> $defsFile_pre
    echo "#undef ${symbol}" >> $defsFile_post
    grep "^typedef .*(\*${symbol}_t)" $(dirname $file)/$(basename $file .c).h | src/generate_symbol.sh >> $defsFile_wrappers 2>> $defsFile_post
  done

  echo "}" >> $defsFile_wrappers
  echo "}" >> $defsFile_post
done

