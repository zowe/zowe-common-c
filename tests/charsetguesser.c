#include <yaml.h>
/* #include <yaml_private.h> */

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "unixfile.h"
#include "json.h"
#include "xlate.h"


/*
  Notes:

  Windows Build ______________________________

    Assuming clang is installed

    clang -I%YAML%/include -I./src -I../h -I ../platform/windows -Dstrdup=_strdup -D_CRT_SECURE_NO_WARNINGS -o charsetguesser.exe charsetguesser.c ../c/xlate.c ../c/charsets.c ../c/winskt.c ../c/logging.c ../c/collections.c ../c/timeutls.c ../c/utils.c ../c/alloc.c 

  ZOS Build _________________________________

 

  Running the Test ________________________________
     
     charsetguesser <filename>

 */

int main(int argc, char *argv[])
{
    char *filename = argv[1];
    UnixFile *in = NULL;
    int maxChars = 1000;
    int returnCode = 0;
    int reasonCode = 0;
    

    in = fileOpen(filename, FILE_OPTION_READ_ONLY, 0, 1024, &returnCode, &reasonCode);
    /* int status = (in ? fileInfo(filename, &info, &returnCode, &reasonCode) : 12); */
    if (in != NULL){ /*  && status == 0) { */
      CharsetOracle *oracle = makeCharsetOracle();
      
      for (int i=0; i<maxChars; i++){
        int b = fileGetChar(in,&returnCode,&reasonCode);
        /* printf("char at %d 0x%x\n",i,b);*/
        if (b < 0 || i > maxChars){
          break;
        }
        char c = (char)(b&0xff);
        charsetOracleDigest(oracle,&c,1);
      }
      fileClose(in,&returnCode,&reasonCode);
      double confidence; 
      int charset = guessCharset(oracle,&confidence);
      printf("charset guess=0x%08x, confidence=%f\n",charset,confidence);
      for (int i=0; i<16; i++){
        printf("  0x%02X: %d\n",i<<4,oracle->rangeBuckets[i]);
      }
      freeCharsetOracle(oracle);
    } else {
      printf("could not open %s ret=%d reason=0x%x\n",filename,returnCode,reasonCode);
    }

    return 0;
}

