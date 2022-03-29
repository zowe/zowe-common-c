#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "zos.h"


/* 
   xlclang -q64 -D_OPEN_SYS_FILE_EXT=1 -D_XOPEN_SOURCE=600 -D_OPEN_THREADS=1 -DSUBPOOL=132 "-Wc,float(ieee),longname,langlvl(extc99),gonum,goff,ASM,asmlib('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN')" "-Wl,ac=1" -I ../h -I ../platform/posix -Wbitwise-op-parentheses -o setkeytest setkeytest.c ../c/zos.c ../c/timeutls.c ../c/utils.c ../c/alloc.c 

 */

static int getPrefix(){
  int32_t prefix = 0;
  __asm(" STPX %0 "
       :
       :"m"(prefix)
       :"r15");
  return prefix;
}


int main(int argc, char **argv){
  if (argc >= 2 && !strcmp(argv[1],"true")){
    supervisorMode(true);
    int oldKey = setKey(0);
    int prefix = getPrefix();
    printf("WITH PRIV: this ZOS has prefix 0x%x\n",prefix);
    setKey(oldKey);
    supervisorMode(false);
    printf("oldKey was = %d\n",oldKey);
  } else{
    int prefix = getPrefix();
    printf("NO PRIV: this ZOS has prefix 0x%x\n",prefix);
  }
  return 0;
}
