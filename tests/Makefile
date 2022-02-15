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
CC:=c89
BITS:=
#BITS:=lp64

ifeq ($(BITS),lp64)
	ENHANCED_ASCII:=-D_ENHANCED_ASCII_EXT=0xFFFFFFFF -Wc,ASCII
else
	ENHANCED_ASCII:=-D_ENHANCED_ASCII_EXT=0xFFFFFFFF -Wc,ASCII,NOXPLINK 
endif

MAJOR:=0
MINOR:=2
PATCH:=5
VERSION:=\"$(MAJOR).$(MINOR).$(PATCH)\"
DEFINESYAML:=-DYAML_VERSION_MAJOR=$(MAJOR) -DYAML_VERSION_MINOR=$(MINOR) -DYAML_VERSION_PATCH=$(PATCH) -DYAML_VERSION_STRING=$(VERSION)
DEFINES:=-D_XOPEN_SOURCE=600 -D_OPEN_THREADS=1

CC_FLAGS:=-Ilibyaml/include -I../h -I../platform/posix $(DEFINES) -Wc,dll,expo,langlvl\(extc99\),gonum,goff,hgpr,roconst,ASM,asmlib\('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN'\) -Wc,agg,list\(\),so\(\),off,xref,$(BITS)
LD_FLAGS:=-Wl,$(BITS)

LIBYAMLOBJS:=api.o reader.o scanner.o parser.o loader.o writer.o emitter.o dumper.o
OBJS:=schematest.o  yaml2json.o jsonschema.o json.o xlate.o charsets.o bpxskt.o logging.o collections.o timeutls.o timeutls.o utils.o alloc.o zosfile.o zos.o le.o scheduling.o recovery.o psxregex.o

.PHONY:	clean all prepare

all:	schematest

schematest:	$(OBJS) $(LIBYAMLOBJS)
	$(CC) $(LD_FLAGS) -o $@ $^

schematest.o:	schematest.c
	$(CC) $(CC_FLAGS) -c schematest.c	

%.o:	../c/%.c
	$(CC) $(CC_FLAGS) -c $<	

%.o:	../platform/posix/%.c
	$(CC) $(CC_FLAGS) -c $<	

%.o:	libyaml/src/%.c
	$(CC) $(CC_FLAGS) $(DEFINESYAML) $(ENHANCED_ASCII) -c $<	

libyaml:	
	git clone git@github.com:yaml/libyaml.git

prepare: libyaml
	
clean:	
	rm -f schematest *.o

################################################################################
#  This program and the accompanying materials are
#  made available under the terms of the Eclipse Public License v2.0 which accompanies
#  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
#
#  SPDX-License-Identifier: EPL-2.0
#
#  Copyright Contributors to the Zowe Project.
################################################################################