################################################################################
#  This program and the accompanying materials are
#  made available under the terms of the Eclipse Public License v2.0 which accompanies
#  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
#
#  SPDX-License-Identifier: EPL-2.0
#
#  Copyright Contributors to the Zowe Project.
################################################################################

# Makefile for z/OS
# make prepare && make
CC:=xlclang
BITS:=
#BITS:=lp64

ifeq ($(BITS),lp64)
	ENHANCED_ASCII:=-qascii -D_ENHANCED_ASCII_EXT=0xFFFFFFFF
else
	ENHANCED_ASCII:=-qascii -D_ENHANCED_ASCII_EXT=0xFFFFFFFF -Wc,NOXPLINK 
endif

MAJOR:=0
MINOR:=2
PATCH:=5
VERSION:=\"$(MAJOR).$(MINOR).$(PATCH)\"
DEFINESYAML:=-DYAML_VERSION_MAJOR=$(MAJOR) -DYAML_VERSION_MINOR=$(MINOR) -DYAML_VERSION_PATCH=$(PATCH) -DYAML_VERSION_STRING=$(VERSION) -DYAML_DECLARE_STATIC=1
DEFINES:=-D_XOPEN_SOURCE=600 -D_OPEN_THREADS=1 -D_OPEN_SYS_FILE_EXT=1 -DCONFIG_VERSION=\"2021-03-27\" 

CC_FLAGS:=-Iquickjs-portable -Ilibyaml/include -I../h -I../platform/posix $(DEFINES) -Wc,dll,expo,langlvl\(extc99\),gonum,goff,hgpr,roconst,ASM,asmlib\('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN'\) -Wc,agg,list\(\),so\(\),off,xref,$(BITS)
LD_FLAGS:=-Wl,$(BITS)

LIBYAMLOBJS:=api.o reader.o scanner.o parser.o loader.o writer.o emitter.o dumper.o
SCHEMATESTOBJS:=schematest.o  yaml2json.o jsonschema.o json.o xlate.o charsets.o bpxskt.o logging.o collections.o timeutls.o timeutls.o utils.o alloc.o embeddedjs.o zosfile.o zos.o le.o scheduling.o recovery.o psxregex.o
QUICKJSOBS:=cutils.o quickjs.o quickjs-libc.o libunicode.o libregexp.o polyfill.o
CONFIGMGROBJ:=configmgr.o  yaml2json.o jsonschema.o json.o xlate.o charsets.o bpxskt.o logging.o collections.o timeutls.o timeutls.o utils.o alloc.o embeddedjs.o zosfile.o zos.o le.o scheduling.o recovery.o psxregex.o

.PHONY:	clean all prepare test_configmgr

all:	configmgr schematest

configmgr:	$(CONFIGMGROBJ) $(LIBYAMLOBJS) $(QUICKJSOBS)
	$(CC) $(LD_FLAGS) -o $@ $^

schematest:	$(SCHEMATESTOBJS) $(LIBYAMLOBJS) $(QUICKJSOBS)
	$(CC) $(LD_FLAGS) -o $@ $^

schematest.o:	schematest.c
	$(CC) $(CC_FLAGS) -c schematest.c	

%.o:	../c/%.c
	$(CC) $(CC_FLAGS) -c $<	

%.o:	../platform/posix/%.c
	$(CC) $(CC_FLAGS) -c $<	

%.o:	libyaml/src/%.c
	$(CC) $(CC_FLAGS) $(DEFINESYAML) $(ENHANCED_ASCII) -c $<	

%.o:	quickjs-portable/%.c
	$(CC) $(CC_FLAGS) $(DEFINESYAML) $(ENHANCED_ASCII) -c $<	

%.o:	quickjs-portable/porting/%.c
	$(CC) $(CC_FLAGS) $(DEFINESYAML) $(ENHANCED_ASCII) -c $<	

libyaml:	
	git clone git@github.com:yaml/libyaml.git

quickjs-portable:	
	git clone git@github.com:JoeNemo/quickjs-portable.git

prepare: libyaml quickjs-portable
	
clean:	
	rm -f configmgr schematest *.o

test_configmgr:	configmgr
	./configmgr -s ./schemadata -p 'FILE(./schemadata/zoweoverrides.yaml):FILE(./schemadata/zowebase.yaml)' validate
	./configmgr -s ./schemadata -p 'FILE(./schemadata/zoweoverrides.yaml):FILE(./schemadata/zowebase.yaml)' env out.env && cat out.env
	

################################################################################
#  This program and the accompanying materials are
#  made available under the terms of the Eclipse Public License v2.0 which accompanies
#  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
#
#  SPDX-License-Identifier: EPL-2.0
#
#  Copyright Contributors to the Zowe Project.
################################################################################