#! /bin/bash

# X="typedef int (*Tcl_Eval_t)(Tcl_Interp*, const char*);"

while read line; do
  returntype=$(echo "$line" | sed -e 's/typedef \([^(]*\) (\*\([^ ]*\)_t)(\(.*\));/\1/')
  name=$(echo "$line" | sed -e 's/typedef \([^(]*\) (\*\([^ ]*\)_t)(\(.*\));/\2/')
  params=$(echo "$line" | sed -e 's/typedef \([^(]*\) (\*\([^ ]*\)_t)(\(.*\));/\3/')

  IFS=,

  params_full=""
  param_names=""

  set $params
  paramCount=$#

  for (( i=0; i < $paramCount; i++)); do
    x="x${i}"
    if [ $1 = "void" ]; then
      params_full="void"
      param_names=""
    else
      params_full="${params_full}${1} ${x}"
      param_names="${param_names}${x}"
      if ! [[ $i = $(expr $paramCount - 1) ]]; then
        params_full="${params_full},"
        param_names="${param_names}, "
      fi
    fi
    shift
  done

  cat << EOF
$returntype $name ($params_full) {
  return DLSYM_VAR($name)($param_names);
}
EOF
done
