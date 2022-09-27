#! /bin/sh
if [ "$#" -eq 1 ]
then
  cp ../bin/ZWECFG31 "//'$1(ZWECFG31)'"
  cp ../bin/ZWECFG64 "//'$1(ZWECFG64)'"
  cp ../bin/ZWERXCFG "//'$1(ZWERXCFG)'"
else
  echo "Bad arguments"
fi


