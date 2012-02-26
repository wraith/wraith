#! /bin/sh

defsFile_wrappers="$1"
defsFile_pre="$2"
defsFile_post="$3"

# X="typedef int (*Tcl_Eval_t)(Tcl_Interp*, const char*);"

while read symbol existing_typedef typedef; do
  echo "#define ${symbol} ORIGINAL_SYMBOL_${symbol}" >> $defsFile_pre
  echo "#undef ${symbol}" >> $defsFile_post
  test $existing_typedef -eq 0 && echo "$typedef" >> $defsFile_post

  returntype=$(echo "$typedef" | sed -e 's/typedef \([^(]*\) (\*\([^ ]*\)_t)(\(.*\));/\1/')
  name=$(echo "$typedef" | sed -e 's/typedef \([^(]*\) (\*\([^ ]*\)_t)(\(.*\));/\2/')
  params=$(echo "$typedef" | sed -e 's/typedef \([^(]*\) (\*\([^ ]*\)_t)(\(.*\));/\3/')

  SAVE_IFS=$IFS
  IFS=,

  params_full=""
  param_names=""

  # Set params to $1,$2, etc
  set $params
  paramCount=$#
  lastParam=$(expr $paramCount - 1)
  i=0

  while [ $i -lt $paramCount ]; do
    x="x${i}"
    if [ $1 = "void" ]; then
      params_full="void"
      param_names=""
    else
      params_full="${params_full}${1} ${x}"
      param_names="${param_names}${x}"
      if [ $i -ne $lastParam ]; then
        params_full="${params_full},"
        param_names="${param_names}, "
      fi
    fi
    shift
    i=$((i + 1))
  done

  cat >> $defsFile_wrappers << EOF
$returntype $name ($params_full) {
  return DLSYM_VAR($name)($param_names);
}
EOF

  echo "$returntype $name ($params_full);" >> $defsFile_post

  IFS=$SAVE_IFS
done
