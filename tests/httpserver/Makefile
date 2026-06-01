###########################################################################
# NOTICE: must run the make command in the following way:
#
# _C89_L6SYSLIB="CEE.SCEEBND2:SYS1.CSSLIB:CSF.SCSFMOD0" make
#
###########################################################################


CC:=c89

REPO_ROOT:=../..

CC_FLAGS:=-D_XOPEN_SOURCE=600 \
  -DNOIBMHTTP=1 \
  -D_OPEN_THREADS=1 \
  -DHTTPSERVER_BPX_IMPERSONATION=1 \
  -DAPF_AUTHORIZED=0 \
  -DUSE_ZOWE_TLS=1 \
  -DNEW_CAA_LOCATIONS=1 \
  -Wc,lp64,dll,expo,langlvl\(extc99\),gonum,goff,hgpr,roconst,ASM,asmlib\('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN'\) \
  -Wc,agg,exp,list,so\(\),off,xref \
  -Wl,lp64 \
  -I ${REPO_ROOT}/h \
  -I ${REPO_ROOT}/platform/posix \
  -I ${REPO_ROOT}/jwt/jwt \
  -I ${REPO_ROOT}/jwt/rscrypto \
  -I .

LD_FLAGS:=-Wl,lp64

COMMON_OBJS:=alloc.o \
  discovery.o \
  httpserver.o \
  utils.o \
  collections.o \
  zos.o \
  logging.o \
  json.o \
  crypto.o \
  bpxskt.o \
  charsets.o \
  cmutils.o \
  collections.o \
  crossmemory.o \
  dataservice.o \
  discovery.o \
  dynalloc.o \
  fdpoll.o \
  http.o \
  httpclient.o \
  httpserver.o \
  httpfileservice.o \
  icsf.o \
  idcams.o \
  impersonation.o \
  jcsi.o \
  json.o \
  jsonschema.o \
  le.o \
  nametoken.o \
  zos.o \
  parsetools.o \
  pause-element.o \
  pdsutil.o \
  qsam.o \
  radmin.o \
  rawfd.o \
  recovery.o \
  rusermap.o \
  scheduling.o \
  signalcontrol.o \
  socketmgmt.o \
  stcbase.o \
  storage.o \
  storage_mem.o \
  timeutls.o \
  tls.o \
  utils.o \
  vsam.o \
  xlate.o \
  xml.o \
  zosaccounts.o \
  zosfile.o \
  zvt.o \
  shrmem64.o \
  client.o \
  jwt.o \
  rs_icsfp11.o \
  rs_rsclibc.o \
  psxregex.o \
  /usr/lpp/gskssl/lib/GSKSSL64.x \
  /usr/lpp/gskssl/lib/GSKCMS64.x

all: bigreqtest

bigreqtest: bigreqtest.o $(COMMON_OBJS)
	$(CC) $(LD_FLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CC_FLAGS) -c $<

%.o: $(REPO_ROOT)/c/%.c
	$(CC) $(CC_FLAGS) -c $<

%.o: $(REPO_ROOT)/platform/posix/%.c
	$(CC) $(CC_FLAGS) -c $<

%.o: $(REPO_ROOT)/jwt/jwt/%.c
	$(CC) $(CC_FLAGS) -c $<

%.o: $(REPO_ROOT)/jwt/rscrypto/%.c
	$(CC) $(CC_FLAGS) -c $<

%.o: zis/%.c
	$(CC) $(CC_FLAGS) -c $<

.PHONY: clean

clean:
	rm -f *.o *.lst
