#ifndef TEST_SRB_HARNESS_H_
#define TEST_SRB_HARNESS_H_

#ifdef METTLE
#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdint.h>
#include "metalio.h"
#else
#include "stddef.h"
#include "stdint.h"
#endif

int run_in_srb(void (*func)(uint32_t parm), uint32_t parm);

#endif /* TEST_SRB_HARNESS_H_ */
