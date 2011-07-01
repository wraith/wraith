#! /bin/bash

mkdir -p src/.defs > /dev/null 2>&1

for file in $(git grep -l DLSYM_GLOBAL|grep "\.c$"); do
  defsFile="src/.defs/$(basename $file .c)_defs.h"
  defsDefine=$(echo "_$(basename $file .c)_defs_h"| tr '[:lower:]' '[:upper:]')

  cat > $defsFile << EOF
#ifndef $defsDefine
#define $defsDefine

EOF
  for symbol in $(sed -n -e 's/.*DLSYM_GLOBAL(.*, \([^)]*\).*/\1/p' $file|sort -u); do
    key="_DLST_IDX_${symbol}"
    printf "#define %-25s DLSYM_VAR(%s)\n" $symbol $symbol
  done >> $defsFile
  cat >> $defsFile << EOF

#endif /* $defsDefine */
EOF
done

