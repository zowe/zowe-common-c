#ifndef ZCNM31_H
#define ZCNM31_H

#include "zmetal.h"
#include "zcntype.h"

int zcnm1act(ZCN *, char[8]) ATTRIBUTE(amode31);
int zcnm1put(ZCN *, const char *) ATTRIBUTE(amode31);
int zcnm1get(ZCN *, char *) ATTRIBUTE(amode31,armode);
int zcnm1dea(ZCN *) ATTRIBUTE(amode31);

#endif