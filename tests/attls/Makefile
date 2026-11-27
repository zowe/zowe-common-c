###########################################################################
# Must run the make command in the following way:
#    _C89_L6SYSLIB="CEE.SCEEBND2:SYS1.CSSLIB:CSF.SCSFMOD0" make
#
# attls_sx_test    unit test, supplies its own tcpIOControl, needs no AT-TLS
# attls_live_test  real SIOCTTLSCTL ioctl on a loopback connection, see the
#                  usage notes at the top of attls_live_test.c
###########################################################################

CC:=c89

REPO_ROOT:=../..

CC_FLAGS:=-D_XOPEN_SOURCE=600 \
  -D_OPEN_THREADS=1 \
  -DAPF_AUTHORIZED=0 \
  -DUSE_ZOWE_TLS=1 \
  -DNEW_CAA_LOCATIONS=1 \
  -Wc,lp64,dll,expo,langlvl\(extc99\),gonum,goff,hgpr,roconst,ASM,asmlib\('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN'\) \
  -Wc,agg,exp,list,so\(\),off,xref \
  -Wl,lp64 \
  -I ${REPO_ROOT}/h \
  -I ${REPO_ROOT}/platform/posix \
  -I .

LD_FLAGS:=-Wl,lp64

BASE_OBJS:=alloc.o \
  charsets.o \
  collections.o \
  fdpoll.o \
  le.o \
  logging.o \
  recovery.o \
  scheduling.o \
  signalcontrol.o \
  socketmgmt.o \
  timeutls.o \
  utils.o \
  xlate.o \
  zos.o \
  zosfile.o

# The live test needs the real socket layer, which in turn needs GSK.
LIVE_OBJS:=$(BASE_OBJS) \
  bpxskt.o \
  tls.o \
  /usr/lpp/gskssl/lib/GSKSSL64.x \
  /usr/lpp/gskssl/lib/GSKCMS64.x

all: attls_sx_test attls_live_test

attls_sx_test: attls_sx_test.o $(BASE_OBJS)
	$(CC) $(LD_FLAGS) -o $@ $^

attls_live_test: attls_live_test.o $(LIVE_OBJS)
	$(CC) $(LD_FLAGS) -o $@ $^

# Only the unit test is expected to pass anywhere; run the live test by hand.
test: attls_sx_test
	./attls_sx_test

%.o: %.c
	$(CC) $(CC_FLAGS) -c $<

%.o: $(REPO_ROOT)/c/%.c
	$(CC) $(CC_FLAGS) -c $<

%.o: $(REPO_ROOT)/platform/posix/%.c
	$(CC) $(CC_FLAGS) -c $<

.PHONY: all test clean

clean:
	rm -f *.o *.lst attls_sx_test attls_live_test
