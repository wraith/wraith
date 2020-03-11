#! /bin/sh

### Export LC_ALL=C so sort(1) stays consistent
export LC_ALL=C

if [ -z "$SED" -o -z "$CXX" ]; then
  echo "This must be ran by configure" >&2
  exit 1
fi
echo "==== Generating lib symbols ===="
INCLUDES="${TCL_INCLUDES} ${SSL_INCLUDES}"

mkdir -p src/.defs > /dev/null 2>&1
TMPFILE=$(mktemp "/tmp/pre.XXXXXX")
files=$(grep -l DLSYM_GLOBAL src/*.cc|grep -v "src/_")
exportsFile="src/.defs/exports"

for file in ${files}; do
  suffix=${file##*.}
  basename=${file%%.*}
  basename=${basename##*/}

  defsFile_wrappers="src/.defs/${basename}_defs.${suffix}"
  defsFile_pre="src/.defs/${basename}_pre.h"
  defsFile_post="src/.defs/${basename}_post.h"

  rm -f $defsFile_pre $defsFile_post $defsFile_wrappers > /dev/null 2>&1
  : > $defsFile_pre
  : > $defsFile_post
  : > $defsFile_wrappers
done

echo "{" > $exportsFile
echo "bfd_exports_stub;" >> $exportsFile
for file in ${files}; do
  suffix=${file##*.}
  basename=${file%%.*}
  basename=${basename##*/}
  dirname=${file%%/*}

  echo -n "Generating symbols for ${basename}... "

  defsFile_wrappers="src/.defs/${basename}_defs.${suffix}"
  defsFile_pre="src/.defs/${basename}_pre.h"
  defsFile_post="src/.defs/${basename}_post.h"

  echo "extern \"C\" {" > $defsFile_wrappers
  echo "extern \"C\" {" > $defsFile_post

  cd src
  $CXX $CXXFLAGS -E -I. -I.. -I../lib ${INCLUDES} -DHAVE_CONFIG_H -DGENERATE_DEFS ../${file} > $TMPFILE
  # Fix wrapped prototypes
  $SED -e :a -e N -e '$!ba' -e 's/,\n/,/g' $TMPFILE > $TMPFILE.sed
  mv $TMPFILE.sed $TMPFILE
  cd ..

  $SED -n -e 's/.*\(DLSYM_GLOBAL[^ (]*\)(.*, \([^)]*\).*/\2 \1/p' $TMPFILE | \
    sort -u | while read symbol dlsym; do
    # Check if the typedef is already defined ...
    typedef=$(grep "^typedef .*(\*${symbol}_t)" ${dirname}/${basename}.h)
    # ... if not, generate it
    if [ -z "$typedef" ]; then
      # Trim off any extern "C", trim out the variable names, cleanup whitespace issues
      typedef=$(grep -w "${symbol}" $TMPFILE | head -n 1 | $SED -e 's/extern "C" *//' -e "s/\(.*\) *${symbol} *(\(.*\)).*/typedef \1 (*${symbol}_t)(\2);/" -e 's/[_0-9A-Za-z]*\(,\)/\1/g' -e 's/[_0-9A-Za-z]*\();\)/\1/g' -e 's/  */ /g' -e 's/ \([,)]\)/\1/g' -e 's/ *()/(void)/g')
      existing_typedef=0
    else
      existing_typedef=1
    fi

    if [ "${typedef%;}" = "${typedef}" ]; then
      echo "Error: Unable to generate typedef for: ${symbol}" >&2
      test -n "$typedef" && echo "$typedef" >&2
      continue
    fi

    #pipe typedef into generate_symbol.sh
    [ -z "$typedef" ] && continue
    if [ "${dlsym}" = "DLSYM_GLOBAL_FWDCOMPAT" ]; then
      echo "_${symbol};" >> $exportsFile
    fi
    echo "${symbol} ${existing_typedef} ${typedef}"
  done | src/generate_symbol.sh $defsFile_wrappers $defsFile_pre $defsFile_post

  echo "}" >> $defsFile_wrappers
  echo "}" >> $defsFile_post

  echo "done"
done
echo "};">> $exportsFile
rm -f $TMPFILE
