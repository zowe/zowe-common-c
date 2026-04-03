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
CC=xlclang
#BITS=
BITS=lp64

ENHANCED_ASCII=-qascii -D_ENHANCED_ASCII_EXT=0xFFFFFFFF

MAJOR=0
MINOR=2
PATCH=5
VERSION=\"$(MAJOR).$(MINOR).$(PATCH)\"
DEFINESYAML=-DYAML_VERSION_MAJOR=$(MAJOR) -DYAML_VERSION_MINOR=$(MINOR) -DYAML_VERSION_PATCH=$(PATCH) -DYAML_VERSION_STRING=$(VERSION) -DYAML_DECLARE_STATIC=1
DEFINES=-D_XOPEN_SOURCE=600 -D_OPEN_THREADS=1 -D_OPEN_SYS_FILE_EXT=1 -DCONFIG_VERSION=\"2021-03-27\"

CC_FLAGS=${ZWE_XLCLANG_FLAGS} -Iquickjs-portable -Ilibyaml/include -I../h -I../platform/posix $(DEFINES) -Wc,dll,expo,langlvl\(extc99\),gonum,goff,hgpr,roconst,ASM,asmlib\('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN'\) -Wc,agg,list\(\),so\(\),off,xref,$(BITS)
LD_FLAGS=-Wl,$(BITS)

# Flags for unit test executables: no dll/expo so symbols link directly from .o files.
TEST_CC_FLAGS=${ZWE_XLCLANG_FLAGS} -I../h -I../platform/posix $(DEFINES) -Wc,langlvl\(extc99\),gonum,goff,hgpr,roconst,ASM,asmlib\('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN'\) -Wc,agg,list\(\),so\(\),off,xref,$(BITS)
TEST_LD_FLAGS=-Wl,$(BITS)

# Flags for test objects that also need the libyaml header search path.
YAML_TEST_CC_FLAGS=-Ilibyaml/include -I../h -I../platform/posix $(DEFINES) -Wc,langlvl\(extc99\),gonum,goff,hgpr,roconst,ASM,asmlib\('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN'\) -Wc,agg,list\(\),so\(\),off,xref,$(BITS)

LIBYAMLOBJS=api.o reader.o scanner.o parser.o loader.o writer.o emitter.o dumper.o

# libyaml objects compiled without dll/expo for use in the yaml2json unit test.
# Each lives in unit/yaml2json/ to avoid naming collisions with $(LIBYAMLOBJS).
LIBYAML_TEST_OBJS=unit/yaml2json/api.o unit/yaml2json/reader.o unit/yaml2json/scanner.o unit/yaml2json/parser.o unit/yaml2json/loader.o unit/yaml2json/writer.o unit/yaml2json/emitter.o unit/yaml2json/dumper.o
SCHEMATESTOBJS=schematest.o yaml2json.o jsonschema.o json.o xlate.o charsets.o bpxskt.o logging.o collections.o timeutls.o utils.o alloc.o embeddedjs.o zosfile.o zos.o le.o scheduling.o recovery.o psxregex.o
QUICKJSOBS=cutils.o quickjs.o quickjs-libc.o libunicode.o libregexp.o polyfill.o
CONFIGMGROBJ=configmgr.o yaml2json.o jsonschema.o json.o xlate.o charsets.o bpxskt.o logging.o collections.o timeutls.o utils.o alloc.o embeddedjs.o zosfile.o zos.o le.o scheduling.o recovery.o psxregex.o

# Object files required for the XML unit tests.
XMLTESTOBJS=unit/xml/xmltest.o zowetests.o xml.o xlate.o charsets.o bpxskt.o logging.o collections.o timeutls.o utils.o alloc.o zosfile.o zos.o recovery.o scheduling.o le.o psxregex.o

# Object files required for the zosaccounts unit tests.
ZOSACCOUNTSTESTOBJS=unit/zosaccounts/zosaccountstest.o zowetests.o zosaccounts.o xlate.o charsets.o bpxskt.o logging.o collections.o timeutls.o utils.o alloc.o zosfile.o zos.o recovery.o scheduling.o le.o psxregex.o

# Object files required for the JSON unit tests.
# zowetests.o provides the test framework; unit/json/jsontest.o contains the test cases.
# The dependency list mirrors the build line documented inside json.c itself.
JSONTESTOBJS=unit/json/jsontest.o zowetests.o json.o xlate.o charsets.o bpxskt.o logging.o collections.o timeutls.o utils.o alloc.o zosfile.o zos.o recovery.o scheduling.o le.o psxregex.o

# Object files required for the unixfile unit tests.
# No json.o is needed -- only zosfile.o (which implements unixfile.h) and
# the usual set of support libraries.
UNIXFILETESTOBJS=unit/unixfile/unixfiletest.o zowetests.o zosfile.o xlate.o charsets.o bpxskt.o logging.o collections.o timeutls.o utils.o alloc.o zos.o recovery.o scheduling.o le.o psxregex.o

# Object files required for the yaml2json unit tests.
# unit/yaml2json/yaml2json.o is yaml2json.c compiled without dll/expo.
# $(LIBYAML_TEST_OBJS) is libyaml compiled without dll/expo (in unit/yaml2json/).
YAML2JSONTESTOBJS=unit/yaml2json/yaml2jsontest.o unit/yaml2json/yaml2json.o zowetests.o json.o xlate.o charsets.o bpxskt.o logging.o collections.o timeutls.o utils.o alloc.o zosfile.o zos.o recovery.o scheduling.o le.o psxregex.o

.PHONY:	clean all prepare test_configmgr test_json test_yaml2json test_unixfile test_xml test_zosaccounts test

all:	configmgr schematest jsontest yaml2jsontest unixfiletest zosaccountstest

configmgr:	$(CONFIGMGROBJ) $(LIBYAMLOBJS) $(QUICKJSOBS)
	$(CC) $(LD_FLAGS) -o $@ $^

schematest:	$(SCHEMATESTOBJS) $(LIBYAMLOBJS) $(QUICKJSOBS)
	$(CC) $(LD_FLAGS) -o $@ $^

jsontest:	$(JSONTESTOBJS)
	$(CC) $(TEST_LD_FLAGS) -o $@ $^

yaml2jsontest:	$(YAML2JSONTESTOBJS) $(LIBYAML_TEST_OBJS)
	$(CC) $(TEST_LD_FLAGS) -o $@ $^

unixfiletest:	$(UNIXFILETESTOBJS)
	$(CC) $(TEST_LD_FLAGS) -o $@ $^

xmltest:	$(XMLTESTOBJS)
	$(CC) $(TEST_LD_FLAGS) -o $@ $^

zosaccountstest:	$(ZOSACCOUNTSTESTOBJS)
	$(CC) $(TEST_LD_FLAGS) -o $@ $^

schematest.o:	schematest.c
	$(CC) $(CC_FLAGS) -c schematest.c

unit/json/jsontest.o:	unit/json/jsontest.c
	$(CC) $(TEST_CC_FLAGS) -c unit/json/jsontest.c -o unit/json/jsontest.o

zowetests.o:	../c/zowetests.c
	$(CC) $(TEST_CC_FLAGS) -c ../c/zowetests.c -o zowetests.o

json.o:	../c/json.c
	$(CC) $(TEST_CC_FLAGS) -c ../c/json.c -o json.o

xlate.o:	../c/xlate.c
	$(CC) $(TEST_CC_FLAGS) -c ../c/xlate.c -o xlate.o

charsets.o:	../c/charsets.c
	$(CC) $(TEST_CC_FLAGS) -c ../c/charsets.c -o charsets.o

bpxskt.o:	../c/bpxskt.c
	$(CC) $(TEST_CC_FLAGS) -c ../c/bpxskt.c -o bpxskt.o

logging.o:	../c/logging.c
	$(CC) $(TEST_CC_FLAGS) -c ../c/logging.c -o logging.o

collections.o:	../c/collections.c
	$(CC) $(TEST_CC_FLAGS) -c ../c/collections.c -o collections.o

timeutls.o:	../c/timeutls.c
	$(CC) $(TEST_CC_FLAGS) -c ../c/timeutls.c -o timeutls.o

utils.o:	../c/utils.c
	$(CC) $(TEST_CC_FLAGS) -c ../c/utils.c -o utils.o

alloc.o:	../c/alloc.c
	$(CC) $(TEST_CC_FLAGS) -c ../c/alloc.c -o alloc.o

zosfile.o:	../c/zosfile.c
	$(CC) $(TEST_CC_FLAGS) -c ../c/zosfile.c -o zosfile.o

zos.o:	../c/zos.c
	$(CC) $(TEST_CC_FLAGS) -c ../c/zos.c -o zos.o

recovery.o:	../c/recovery.c
	$(CC) $(TEST_CC_FLAGS) -c ../c/recovery.c -o recovery.o

scheduling.o:	../c/scheduling.c
	$(CC) $(TEST_CC_FLAGS) -c ../c/scheduling.c -o scheduling.o

le.o:	../c/le.c
	$(CC) $(TEST_CC_FLAGS) -c ../c/le.c -o le.o

psxregex.o:	../platform/posix/psxregex.c
	$(CC) $(TEST_CC_FLAGS) -c ../platform/posix/psxregex.c -o psxregex.o

unit/unixfile/unixfiletest.o:	unit/unixfile/unixfiletest.c
	$(CC) $(TEST_CC_FLAGS) -c unit/unixfile/unixfiletest.c -o unit/unixfile/unixfiletest.o

xml.o:	../c/xml.c
	$(CC) $(TEST_CC_FLAGS) -c ../c/xml.c -o xml.o

zosaccounts.o:	../c/zosaccounts.c
	$(CC) $(TEST_CC_FLAGS) -c ../c/zosaccounts.c -o zosaccounts.o

unit/xml/xmltest.o:	unit/xml/xmltest.c
	$(CC) $(TEST_CC_FLAGS) -c unit/xml/xmltest.c -o unit/xml/xmltest.o

unit/zosaccounts/zosaccountstest.o:	unit/zosaccounts/zosaccountstest.c
	$(CC) $(TEST_CC_FLAGS) -c unit/zosaccounts/zosaccountstest.c -o unit/zosaccounts/zosaccountstest.o

# ------------------------------------------------------------------
# Rules for yaml2json unit test objects
# All compiled with YAML_TEST_CC_FLAGS (no dll/expo, libyaml headers).
# ------------------------------------------------------------------

unit/yaml2json/yaml2jsontest.o:	unit/yaml2json/yaml2jsontest.c
	$(CC) $(YAML_TEST_CC_FLAGS) -c unit/yaml2json/yaml2jsontest.c -o unit/yaml2json/yaml2jsontest.o

unit/yaml2json/yaml2json.o:	../c/yaml2json.c
	$(CC) $(YAML_TEST_CC_FLAGS) -c ../c/yaml2json.c -o unit/yaml2json/yaml2json.o

# libyaml sources compiled without dll/expo and with ENHANCED_ASCII + DEFINESYAML.
unit/yaml2json/api.o:	libyaml/src/api.c
	$(CC) $(YAML_TEST_CC_FLAGS) $(DEFINESYAML) $(ENHANCED_ASCII) -c libyaml/src/api.c -o unit/yaml2json/api.o

unit/yaml2json/reader.o:	libyaml/src/reader.c
	$(CC) $(YAML_TEST_CC_FLAGS) $(DEFINESYAML) $(ENHANCED_ASCII) -c libyaml/src/reader.c -o unit/yaml2json/reader.o

unit/yaml2json/scanner.o:	libyaml/src/scanner.c
	$(CC) $(YAML_TEST_CC_FLAGS) $(DEFINESYAML) $(ENHANCED_ASCII) -c libyaml/src/scanner.c -o unit/yaml2json/scanner.o

unit/yaml2json/parser.o:	libyaml/src/parser.c
	$(CC) $(YAML_TEST_CC_FLAGS) $(DEFINESYAML) $(ENHANCED_ASCII) -c libyaml/src/parser.c -o unit/yaml2json/parser.o

unit/yaml2json/loader.o:	libyaml/src/loader.c
	$(CC) $(YAML_TEST_CC_FLAGS) $(DEFINESYAML) $(ENHANCED_ASCII) -c libyaml/src/loader.c -o unit/yaml2json/loader.o

unit/yaml2json/writer.o:	libyaml/src/writer.c
	$(CC) $(YAML_TEST_CC_FLAGS) $(DEFINESYAML) $(ENHANCED_ASCII) -c libyaml/src/writer.c -o unit/yaml2json/writer.o

unit/yaml2json/emitter.o:	libyaml/src/emitter.c
	$(CC) $(YAML_TEST_CC_FLAGS) $(DEFINESYAML) $(ENHANCED_ASCII) -c libyaml/src/emitter.c -o unit/yaml2json/emitter.o

unit/yaml2json/dumper.o:	libyaml/src/dumper.c
	$(CC) $(YAML_TEST_CC_FLAGS) $(DEFINESYAML) $(ENHANCED_ASCII) -c libyaml/src/dumper.c -o unit/yaml2json/dumper.o

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
	rm -f configmgr schematest jsontest yaml2jsontest unixfiletest xmltest zosaccountstest *.o unit/json/*.o unit/yaml2json/*.o unit/unixfile/*.o unit/xml/*.o unit/zosaccounts/*.o

test_configmgr:	configmgr
	./configmgr -s ./schemadata -p 'FILE(./schemadata/zoweoverrides.yaml):FILE(./schemadata/zowebase.yaml)' validate
	./configmgr -s ./schemadata -p 'FILE(./schemadata/zoweoverrides.yaml):FILE(./schemadata/zowebase.yaml)' env out.env && cat out.env

test_json:	jsontest
	./jsontest

test_yaml2json:	yaml2jsontest
	./yaml2jsontest

test_unixfile:	unixfiletest
	./unixfiletest

test_xml:	xmltest
	./xmltest

test_zosaccounts:	zosaccountstest
	./zosaccountstest

test:	test_json test_yaml2json test_unixfile test_xml test_zosaccounts


################################################################################
#  This program and the accompanying materials are
#  made available under the terms of the Eclipse Public License v2.0 which accompanies
#  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
#
#  SPDX-License-Identifier: EPL-2.0
#
#  Copyright Contributors to the Zowe Project.
################################################################################
