################################################################################
#  This program and the accompanying materials are
#  made available under the terms of the Eclipse Public License v2.0 which accompanies
#  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
#
#  SPDX-License-Identifier: EPL-2.0
#
#  Copyright Contributors to the Zowe Project.
################################################################################

###########################################################################
# jsonPrinter conversion / IO error-flag tests (z/OS build).
#
#   make           # build json-conversion-test
#   make test      # build + run it (exit code == number of failed checks)
#   make clean
#
# No ZIS, no TLS, no server and no datasets are involved: the object list is
# a small subset of the schematest one in ../Makefile -- just the JSON printer
# and the charset converter underneath it.
###########################################################################

CC:=c89

REPO_ROOT:=../..

CC_FLAGS:=-D_XOPEN_SOURCE=600 \
  -D_OPEN_THREADS=1 \
  -Wc,lp64,dll,expo,langlvl\(extc99\),gonum,goff,hgpr,roconst,ASM,asmlib\('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN'\) \
  -Wc,agg,exp,list,so\(\),off,xref \
  -Wl,lp64 \
  -I ${REPO_ROOT}/h \
  -I ${REPO_ROOT}/platform/posix \
  -I .

LD_FLAGS:=-Wl,lp64

COMMON_OBJS:=json.o \
  charsets.o \
  xlate.o \
  alloc.o \
  utils.o \
  logging.o \
  collections.o \
  timeutls.o \
  bpxskt.o \
  zos.o \
  zosfile.o \
  le.o \
  scheduling.o \
  recovery.o \
  psxregex.o

all: json-conversion-test

json-conversion-test: json-conversion-test.o $(COMMON_OBJS)
	$(CC) $(LD_FLAGS) -o $@ $^

# more test units go here...

%.o: %.c
	$(CC) $(CC_FLAGS) -c $<

%.o: $(REPO_ROOT)/c/%.c
	$(CC) $(CC_FLAGS) -c $<

%.o: $(REPO_ROOT)/platform/posix/%.c
	$(CC) $(CC_FLAGS) -c $<

.PHONY: all test clean

test: json-conversion-test
	./json-conversion-test

clean:
	rm -f *.o *.lst json-conversion-test

################################################################################
#  This program and the accompanying materials are
#  made available under the terms of the Eclipse Public License v2.0 which accompanies
#  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
#
#  SPDX-License-Identifier: EPL-2.0
#
#  Copyright Contributors to the Zowe Project.
################################################################################
