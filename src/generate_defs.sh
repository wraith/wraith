#! /bin/sh

if [ -z "$SED" -o -z "$CXX" ]; then
  echo "This must be ran by configure" >&2
  exit 1
fi
echo "Generating lib symbols"
INCLUDES="-I${TCLINC} ${SSL_INCLUDES}"

mkdir -p src/.defs > /dev/null 2>&1
TMPFILE=$(mktemp "/tmp/pre.XXXXXX")
for file in $(grep -l DLSYM_GLOBAL src/*.c|grep -v "src/_"); do
  defsFile_wrappers="src/.defs/$(basename $file .c)_defs.c"
  defsFile_pre="src/.defs/$(basename $file .c)_pre.h"
  defsFile_post="src/.defs/$(basename $file .c)_post.h"

  rm -f $defsFile_pre $defsFile_post $defsFile_wrappers > /dev/null 2>&1
  touch $defsFile_pre $defsFile_post $defsFile_wrappers
done

for file in $(grep -l DLSYM_GLOBAL src/*.c|grep -v "src/_"); do
  defsFile_wrappers="src/.defs/$(basename $file .c)_defs.c"
  defsFile_pre="src/.defs/$(basename $file .c)_pre.h"
  defsFile_post="src/.defs/$(basename $file .c)_post.h"

  echo "extern \"C\" {" > $defsFile_wrappers
  echo "extern \"C\" {" > $defsFile_post
  touch $defsFile_pre
  cd src >/dev/null 2>&1
  $CXX -E -I. -I.. -I../lib ${INCLUDES} -DHAVE_CONFIG_H ../${file} > $TMPFILE
  # Fix wrapped prototypes
  $SED -i -e ':a;N;$!ba;s/,\n/,/g' $TMPFILE
  cd .. >/dev/null 2>&1

  for symbol in $($SED -n -e 's/.*DLSYM_GLOBAL(.*, \([^)]*\).*/\1/p' $file|sort -u); do
    echo "#define ${symbol} ORIGINAL_SYMBOL_${symbol}" >> $defsFile_pre
    echo "#undef ${symbol}" >> $defsFile_post
    # Check if the typedef is already defined ...
    typedef=$(grep "^typedef .*(\*${symbol}_t)" $(dirname $file)/$(basename $file .c).h)
    # ... if not, generate it
    if [ -z "$typedef" ]; then
      # Trim off any extern "C", trim out the variable names, cleanup whitespace issues
      typedef=$($SED -n -e "/\<${symbol}\>/p" $TMPFILE | head -n 1 | $SED -e 's/extern "C" *//' -e "s/\(.*\) *${symbol} *(\(.*\));\?/typedef \1 (*${symbol}_t)(\2);/" -e 's/[_0-9A-Za-z]*\(,\)/\1/g' -e 's/[_0-9A-Za-z]*\();\)/\1/g' -e 's/  */ /g' -e 's/ \([,)]\)/\1/g' -e 's/ *()/(void)/g')
      echo "$typedef" >> $defsFile_post
    fi

    if [ "${typedef%;}" = "${typedef}" ]; then
      echo "Error: Unable to generate typedef for: ${symbol}" >&2
      echo "$typedef"
      rm -rf $TMPFILE
      exit 1
    fi

    #pipe typedef into generate_symbol.sh
    echo "$typedef" | src/generate_symbol.sh >> $defsFile_wrappers 2>> $defsFile_post
  done

  echo "}" >> $defsFile_wrappers
  echo "}" >> $defsFile_post
done
rm -f $TMPFILE
