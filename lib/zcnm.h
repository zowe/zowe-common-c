#ifndef ZCNM_H
#define ZCNM_H

#include "zmetal.h"
#include "zcntype.h"

#if defined(__cplusplus) && (defined(__IBMCPP__) || defined(__IBMC__))
extern "OS"
{
#elif defined(__cplusplus)
extern "C"
{
#endif

int ZCNACT(ZCN *, char [8]);
int ZCNPUT(ZCN *, const char *);
int ZCNGET(ZCN *, char *);
int ZCNDACT(ZCN *);

#if defined(__cplusplus)
}
#endif

#endif