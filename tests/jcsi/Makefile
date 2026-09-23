CC       := xlclang
CC_FLAGS := -D_XOPEN_SOURCE=600 \
            -DNOIBMHTTP=1 \
            -D_OPEN_THREADS=1 \
            -DHTTPSERVER_BPX_IMPERSONATION=1 \
            -DAPF_AUTHORIZED=0 \
            -DUSE_ZOWE_TLS=1 \
            -DNEW_CAA_LOCATIONS=1 \
            -Wc,lp64,dll,expo,langlvl\(extc99\),gonum,goff,hgpr,roconst,ASM,asmlib\('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN'\) \
            -Wc,agg,list,so\(\),off,xref
LD_FLAGS := -Wl,lp64
MEMTRACE := -DTRACK_MEMORY
INCLUDES := -I. -I../../h
SRCS     := $(wildcard ./*.c) \
            ../../c/jcsi.c \
			../../c/utils.c \
			../../c/alloc.c \
			../../c/timeutls.c \
			../../c/logging.c \
			../../c/collections.c \
			../../c/le.c \
			../../c/scheduling.c  \
			../../c/recovery.c \
			../../c/zos.c
OBJS     := $(SRCS:.c=.o)
DEPS     := $(OBJS:.o=.d)

.PHONY: all clean

all: gethlqs

gethlqs: $(OBJS)
	$(CC) $(CC_FLAGS) $(LD_FLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CC_FLAGS) $(MEMTRACE) $(INCLUDES) -c $< -o $@
	$(CC) $(CC_FLAGS) $(MEMTRACE) $(INCLUDES) -M $< -MF $(@:.o=.d) -MT $@

-include $(DEPS)

clean:
	rm -f $(OBJS) $(DEPS) *.lst *.dbg
