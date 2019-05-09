#! /rsusr/local/bin/bash

if [[ $# != 1 ]] ; then
  echo "usage: $0 DLL_NAME"
  echo '  The list of symbols is read from stdin; the side file'
  echo '  statements (properly padded) are written to stdout.'
  exit 1
fi

DLL_NAME=$1

awk -v DLL_NAME=$DLL_NAME "
/^ *#/ { next; } # ignore comment lines \(lines starting with #\)
/^ *$/ { next; } # also ignore blank lines
 { 
   # for everything else
   binder_stmt =  \" IMPORT CODE,'\" DLL_NAME \"','\" \$1 \"'\";
   # Must be 80 character lines (including the newline)
   printf \"%-79s\\n\", binder_stmt
 }
" 