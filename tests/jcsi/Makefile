CC       := xlclang
CFLAGS   := -q64 -D_XOPEN_SOURCE=600 -D_OPEN_THREADS -DNEW_CAA_LOCATIONS=1 -O2 \
            -Wc,lp64,dll,expo,langlvl\(extc99\),gonum,goff,hgpr,roconst,ASM,asmlib\('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN'\) \
            -Wc,agg,list,so\(\),off,xref
MEMTRACE := -DTRACK_MEMORY -DMALLOC_TRACE_LEVEL=1
INCLUDES := -I. -I../../h
SRCS     := $(wildcard ./*.c) ../../c/jcsi.c ../../c/utils.c ../../c/alloc.c ../../c/timeutls.c
OBJS     := $(SRCS:.c=.o)
DEPS     := $(OBJS:.o=.d)

.PHONY: all clean

all: gethlqs

gethlqs: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) $(MEMTRACE) $(INCLUDES) -c $< -o $@
	$(CC) $(CFLAGS) $(MEMTRACE) $(INCLUDES) -M $< -MF $(@:.o=.d) -MT $@

-include $(DEPS)

clean:
	rm -f $(OBJS) $(DEPS) *.lst *.dbg
