#! /bin/sh
if [ "$#" -eq 1 ]
then
  cp ../bin/zwecfg31 "//'$1(ZWECFG31)'"
  cp ../bin/zwecfg64 "//'$1(ZWECFG64)'"
  cp ../bin/zwecfgle "//'$1(ZWERXCFG)'"
else
  echo "Bad arguments"
fi


