-include cflags.mk

CC := xlc
LIBRARY := lib/librsjwt.a

MAINFRAME_C ?= ..

MODULES := jwt rscrypto

CFLAGS += $(patsubst %,-I%, $(MODULES))
CFLAGS += -I. -I$(MAINFRAME_C)/h
CFLAGS += -D_XOPEN_SOURCE_EXTENDED  -D_LARGE_TIME_API

SRC :=
       
-include $(patsubst %,%/module.mk,$(MODULES))

OBJ := \
  $(patsubst %.c,%.o, $(filter %.c,$(SRC)))

$(LIBRARY): $(OBJ)
	mkdir -p lib
	ar -r $(LIBRARY) $(OBJ)

%.o : %.c
	$(CC) -c $(CFLAGS) $< -o $@ > $@.listing

compile.sh:
	make -nsB > compile.sh

include tests/test.mk

.PHONY: clean
clean:
	rm -f $(LIBRARY) $(OBJ) $(OBJ:.o=.u) $(OBJ:.o=.o.listing) $(TEST_OBJ) \
	$(TEST_OBJ:.o=.u) $(TEST_OBJ:.o=.o.listing) $(TEST_BINARIES) *.lst *.dbg

-include $(OBJ:.o=.u) $(TEST_OBJ:.o=.u)
