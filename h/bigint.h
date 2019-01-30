

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __BIGINT__
#define __BIGINT__

#define BI_MAX_WIDTH 4096

#ifndef __ZOWETYPES__
typedef unsigned int uint32;
// typedef unsigned long long uint64;
#endif

typedef struct BigInt_tag{
  char isNegative;
  char wordCount;  /* 32 bits per word */
  char inSLH;
  char res1;
  uint32  *data;
} BigInt;

/* This bigint library is for low-level mainframe C/metal-C/ASM usage.  It resembles in many ways
   the functionality of the java.math.BigInteger library.  It is different and interesting in two ways:

   1) The callers of all functions control memory management for temp values and answers.
      This is a little ugly, but kicks ass for low-level coding and modularity.

   2) The bigints have a fixed width set at allocation time.  Suppose you want to do 2048 bit calculations,
      allocate everything with 4096 bits for multiplication headroom.  Simple as that.

 */

BigInt *biNew(int bitWidth, ShortLivedHeap *slhOrNull);
int biZero(BigInt *x);
int biCopy(BigInt *source, BigInt *dest);
int biAdd2(BigInt *a, BigInt *b);
int biAdd3(BigInt *a, BigInt *b, BigInt *c);
int biMul3(BigInt *a, BigInt *b, BigInt *p);
int biMul3Special(BigInt *a, uint32 b, int leftShift, BigInt *p);
char *biHexFill(char *buffer, BigInt *a);
int biSignificantBits(BigInt *x);
int testBit(BigInt *x, int bitIndex);
int biDiv3(BigInt *dividend, BigInt *divisor, BigInt *quotient, BigInt *remainder, BigInt *temp1, BigInt *temp2);
int biRead(BigInt *x, char *source, int sourceLen);
int biLoad(BigInt *x, char *source, int sourceLen);
int biModPow(BigInt *base, BigInt *exponent, BigInt *modulus, BigInt *result, 
	     BigInt *temp1, BigInt *temp2, BigInt *temp3, BigInt *temp4, BigInt *temp5);

#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

