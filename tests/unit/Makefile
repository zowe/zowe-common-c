################################################################################
#  This program and the accompanying materials are
#  made available under the terms of the Eclipse Public License v2.0 which accompanies
#  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
#
#  SPDX-License-Identifier: EPL-2.0
#
#  Copyright Contributors to the Zowe Project.
################################################################################

# Makefile for z/OS unit tests
# Usage:
#   make            - build and run all tests
#   make build      - build all test executables
#   make test       - run all tests and report results
#   make clean      - remove build artifacts

CC := xlclang

BITS :=
#BITS := lp64

DEFINES := -D_XOPEN_SOURCE=600 -D_OPEN_THREADS=1 -D_OPEN_SYS_FILE_EXT=1

CC_FLAGS := -I../../h -I../../platform/posix $(DEFINES) \
	-qascii \
	-Wc,dll,expo,langlvl\(extc99\),gonum,goff,hgpr,roconst,ASM,asmlib\('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN'\) \
	-Wc,agg,list\(\),so\(\),off,xref,$(BITS)

LD_FLAGS := -Wl,$(BITS)

# Common object files needed by all tests
COMMON_OBJS := alloc.o utils.o collections.o timeutls.o

# Test executables
TEST_BINS := test_collections test_utils

.PHONY: all build test clean

all: build test

build: $(TEST_BINS)

test: $(TEST_BINS)
	@PASS=0; FAIL=0; TOTAL=0; \
	for t in $(TEST_BINS); do \
		TOTAL=$$((TOTAL + 1)); \
		echo ""; \
		echo "======================================"; \
		echo "Running $$t"; \
		echo "======================================"; \
		./$$t; \
		if [ $$? -eq 0 ]; then \
			PASS=$$((PASS + 1)); \
		else \
			FAIL=$$((FAIL + 1)); \
		fi; \
	done; \
	echo ""; \
	echo "======================================"; \
	echo "Test suites: $$TOTAL total, $$PASS passed, $$FAIL failed"; \
	echo "======================================"; \
	if [ $$FAIL -ne 0 ]; then exit 1; fi

# --- test_collections ---
test_collections: test_collections.o $(COMMON_OBJS)
	$(CC) $(LD_FLAGS) -o $@ $^

test_collections.o: test_collections.c zowe_test.h
	$(CC) $(CC_FLAGS) -I. -c $<

# --- test_utils ---
test_utils: test_utils.o $(COMMON_OBJS)
	$(CC) $(LD_FLAGS) -o $@ $^

test_utils.o: test_utils.c zowe_test.h
	$(CC) $(CC_FLAGS) -I. -c $<

# --- Common source compilation ---
%.o: ../../c/%.c
	$(CC) $(CC_FLAGS) -c $<

clean:
	rm -f $(TEST_BINS) *.o *.lst

################################################################################
#  This program and the accompanying materials are
#  made available under the terms of the Eclipse Public License v2.0 which accompanies
#  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
#
#  SPDX-License-Identifier: EPL-2.0
#
#  Copyright Contributors to the Zowe Project.
################################################################################
