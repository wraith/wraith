#! /bin/bash

echo "Generating lib symbols"

mkdir -p src/.defs > /dev/null 2>&1

for file in $(git grep -l DLSYM_GLOBAL|grep "\.c$"); do
  defsFile="src/.defs/$(basename $file .c)_defs.c"
  cat > $defsFile << EOF
extern "C" {
EOF

  for symbol in $(sed -n -e 's/.*DLSYM_GLOBAL(.*, \([^)]*\).*/\1/p' $file|sort -u); do
    grep "^typedef .*(\*${symbol}_t)" $(dirname $file)/$(basename $file .c).h | src/generate_symbol.sh
  done >> $defsFile

  cat >> $defsFile << EOF
}
EOF
done

