CC       := xlclang
CFLAGS   := -q64 -D_XOPEN_SOURCE=600 -D_OPEN_THREADS -DNEW_CAA_LOCATIONS=1 -O2 \
            -Wc,lp64,dll,expo,langlvl\(extc99\),gonum,goff,hgpr,roconst,ASM,asmlib\('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN'\) \
            -Wc,agg,list,so\(\),off,xref
INCLUDES := -I. -I../../h
SRCS     := $(wildcard ./*.c) ../../c/zosfile.c ../../c/utils.c ../../c/alloc.c ../../c/charsets.c ../../c/logging.c ../../c/collections.c
OBJS     := $(SRCS:.c=.o)
DEPS     := $(OBJS:.o=.d)
TARGET   := zosfiles

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
	$(CC) $(CFLAGS) $(INCLUDES) -M $< -MF $(@:.o=.d) -MT $@

-include $(DEPS)

clean:
	rm -f $(OBJS) $(DEPS) *.lst *.dbg
