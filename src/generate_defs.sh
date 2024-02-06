#! /bin/sh
set -e

### Export LC_ALL=C so sort(1) stays consistent
export LC_ALL=C

if [ -z "$SED" -o -z "$CXX" ]; then
  echo "This must be ran by configure" >&2
  exit 1
fi
#echo "==== Generating lib symbols ===="
: ${INCLUDES:="${TCL_INCLUDES} ${SSL_INCLUDES}"}

mkdir -p src/.defs > /dev/null 2>&1
TMPFILE=$(mktemp "/tmp/pre.XXXXXX")
allfiles=$(grep -l DLSYM_GLOBAL src/*.cc|grep -v "src/_")

if [ "$#" -eq 0 ]; then
	for file in ${allfiles}; do
	  suffix=${file##*.}
	  basename=${file%%.*}
	  basename=${basename##*/}

	  defsFile_wrappers="src/.defs/${basename}_defs.${suffix}"
	  defsFile_pre="src/.defs/${basename}_pre.h"
	  defsFile_post="src/.defs/${basename}_post.h"

	  rm -f "${defsFile_pre}" "${defsFile_post}" "${defsFile_wrappers}" > /dev/null 2>&1
	  if [ ! -f "${defsFile_pre}" ]; then
		  : > "${defsFile_pre}"
	  fi
	  if [ ! -f "${defsFile_post}" ]; then
		  : > "${defsFile_post}"
	  fi
	  if [ ! -f "${defsFile_wrappers}" ]; then
		  : > "${defsFile_wrappers}"
	  fi
	done
fi

: ${files:=${1-$(grep -l DLSYM_GLOBAL src/*.cc|grep -v "src/_")}}
for file in ${files}; do
  suffix=${file##*.}
  basename=${file%%.*}
  basename=${basename##*/}
  dirname=${file%%/*}

  echo -n "Generating symbols for ${basename}... "

  exportsFile="src/.defs/${basename}_exports"
  {
	  #echo "{"
	  :
  } > "${exportsFile}.new"

  defsFile_wrappers="src/.defs/${basename}_defs.${suffix}"
  defsFile_pre="src/.defs/${basename}_pre.h"
  defsFile_post="src/.defs/${basename}_post.h"

  {
	  #echo "#ifndef GENERATING_DEFS"
	  echo "extern \"C\" {"
  } > "${defsFile_wrappers}.new"
  {
	  #echo "#ifndef GENERATING_DEFS"
	  echo "extern \"C\" {"
  } > "${defsFile_post}.new"

  cd src
  $CXX $CXXFLAGS -E -I. -I.. -I../lib ${INCLUDES} -DHAVE_CONFIG_H -DGENERATING_DEFS "../${file}" > "${TMPFILE}"
  # Fix wrapped prototypes
  $SED -e :a -e N -e '$!ba' -e 's/,\n/,/g' "${TMPFILE}" > "${TMPFILE}.sed"
  mv "${TMPFILE}.sed" "${TMPFILE}"
  cd ..

  $SED -n -e 's/.*\(DLSYM_GLOBAL[^ (]*\)(.*, \([^)]*\).*/\2 \1/p' "${TMPFILE}" | \
    sort -u | while read symbol dlsym; do
    # Check if the typedef is already defined ...
    typedef=$(grep "^typedef .*(\*${symbol}_t)" "${dirname}/${basename}.h") || :
    # ... if not, generate it
    if [ -z "$typedef" ]; then
      if ! grep -v "DLSYM" "${TMPFILE}" | grep -qw "${symbol}"; then
        echo "Unable to parse symbol ${symbol}. Is there a missing header?" >&2
	errors=1
	rm -f "${TMPFILE}"
	exit 1
      fi

      # Trim off any extern "C", trim out the variable names, cleanup whitespace issues
      typedef=$(grep -w "${symbol}" "${TMPFILE}" |
	      head -n 1 |
	      $SED \
	         -e 's/extern "C" *//' \
		 -e "s/\(.*\) *${symbol} *(\(.*\)).*/typedef \1 (*${symbol}_t)(\2);/" \
		 -e 's/[_0-9A-Za-z]*\(,\)/\1/g' \
		 -e 's/[_0-9A-Za-z]*\();\)/\1/g' \
		 -e 's/  */ /g' \
		 -e 's/ \([,)]\)/\1/g' \
		 -e 's/ *()/(void)/g' \
	 ) || :
      existing_typedef=0
    else
      existing_typedef=1
    fi

    if [ "${typedef%;}" = "${typedef}" ]; then
      echo "Error: Unable to generate typedef for: ${symbol}" >&2
      if [ -n "${typedef}" ]; then
	      echo "$typedef" >&2
      fi
      continue
    fi

    #pipe typedef into generate_symbol.sh
    if [ -z "${typedef}" ]; then
	    continue
    fi
    if [ "${dlsym}" = "DLSYM_GLOBAL_FWDCOMPAT" ]; then
      echo "_${symbol};" >> "${exportsFile}.new"
    fi
    echo "${symbol} ${existing_typedef} ${typedef}"
  done | src/generate_symbol.sh "${defsFile_wrappers}" "${defsFile_pre}" "${defsFile_post}"

  {
	  echo "}"
	  #echo "#endif"
  } >> "${defsFile_wrappers}.new"
  {
	  echo "}"
	  #echo "#endif"
  } >> "${defsFile_post}.new"

  {
	  :
	  #echo "};"
  } >> "${exportsFile}.new"

  changed=0
  for file in ${defsFile_wrappers} ${defsFile_pre} ${defsFile_post} ${exportsFile}; do
	  if ! cmp -s "${file}.new" "${file}"; then
		  changed=1
		  mv -f "${file}.new" "${file}"
	  else
		  rm -f "${file}.new"
		  # Needed because generate_defs.sh may have triggered this
		  # (or other deps like dl.h)
		  touch "${file}"
	  fi
  done
  if [ "${changed}" -eq 1 ]; then
	  echo "done"
  else
	  echo "unchanged"
  fi
done
rm -f "${TMPFILE}"
