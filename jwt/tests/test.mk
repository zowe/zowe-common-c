

TEST_SRC := $(MAINFRAME_C)/c/alloc.c $(MAINFRAME_C)/c/zos.c \
            $(MAINFRAME_C)/c/utils.c $(MAINFRAME_C)/c/json.c \
            $(MAINFRAME_C)/c/charsets.c $(MAINFRAME_C)/c/timeutls.c \
            $(MAINFRAME_C)/c/xlate.c $(MAINFRAME_C)/c/zosfile.c \
            tests/jwt-test.c

TEST_OBJ := $(TEST_SRC:.c=.o)

TEST_BINARIES := tests/jwt-test

tests/jwt-test: SEVERITY_W += $(SEVERITY_E)
tests/jwt-test: SEVERITY_E :=
tests/jwt-test: CFLAGS += -DTRACK_MEMORY
 
tests/jwt-test: $(filter $(MAINFRAME_C)/%.o,$(TEST_OBJ)) $(LIBRARY) tests/jwt-test.o
	$(CC) $(CFLAGS) -o $@ -Llib tests/jwt-test.o \
	   $(filter $(MAINFRAME_C)/%.o,$(TEST_OBJ)) -lrsjwt /usr/lib/CSFDLL31.x || rm tests/jwt-test

.PHONY: test
test: $(TEST_BINARIES)
	./tests/jwt-test
