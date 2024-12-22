#
#  This program and the accompanying materials are
#  made available under the terms of the Eclipse Public License v2.0
#  which accompanies this distribution, and is available at
#  https://www.eclipse.org/legal/epl-v20.html
#
#  SPDX-License-Identifier: EPL-2.0
#
#  Copyright Contributors to the Zowe Project.
#
CXX=xlc++
CC=xlc
ASM=as
BIND = ld

BIND_FLAGS = -bRMODE=ANY

MTL_OPTS=metal,\
 langlvl(extended),\
 sscom,\
 nolongname,\
 inline,\
 genasm,\
 inlrpt,\
 csect,\
 nose,\
 list,\
 warn64,\
 optimize(2),\
 list,\
 showinc,\
 showmacro,\
 source,\
 aggregate

MTL_OPTS64=$(MTL_OPTS),lp64
MTL_FLAGS=-S -W "c,$(MTL_OPTS)"
MTL_FLAGS64=-S -W "c,$(MTL_OPTS64)"

MACLIBS=-ISYS1.MACLIB \
 -ICBC.SCCNSAM

MTL_HEADERS=-I/usr/include/metal \
 -I./chdsect

CPP_BND_FLAGS=-W "l,lp64,xplink,map,list"\
 -qsource \
 -g

CPP_BND_FLAGS_AUTH=-W "l,lp64,xplink,map,list,ac=1"\
 -qsource \
 -g

DLL_CPP_FLAGS=-W "c,lp64,langlvl(extended),dll,xplink,exportall,showinc"\
 -qsource \
 -g \
 -c \
 -I./chdsect

DLL_BND_FLAGS=-W "l,lp64,dll,dynam=dll,xplink,map,list" \
 -g \
 -qsource

# -g generates debugging information
CPP_FLAGS=-W "c,lp64,langlvl(extended),dll,xplink,showinc"\
 -qsource \
 -g \
 -c

ASM_FLAGS=-mrent

all: zowex zowexx extattr

#
# Utilities
#
zutilm.s: zutilm.c
	$(CC) $(MTL_FLAGS64) -qlist=$*.mtl.lst $(MTL_HEADERS) -o $@ $^

zutilm.o: zutilm.s
	$(ASM) $(ASM_FLAGS) -a=$*.asm.lst $(MACLIBS) -o $@ $^

zutilm31.s: zutilm31.c
	$(CC) $(MTL_FLAGS) -qlist=$*.mtl.lst $(MTL_HEADERS) -o $@ $^

zutilm31.o: zutilm31.s
	$(ASM) $(ASM_FLAGS) -a=$*.asm.lst $(MACLIBS) -o $@ $^

#
# Jobs
#
zjbm.s: zjbm.c
	$(CC) $(MTL_FLAGS64) -qlist=$*.mtl.lst $(MTL_HEADERS) -o $@ $^

zjbm.o: zjbm.s
	$(ASM) $(ASM_FLAGS) -a=$*.asm.lst $(MACLIBS) -o $@ $^

zjb.o: zjb.cpp
	$(CXX) $(DLL_CPP_FLAGS) -qlist=$*.cpp.lst -o $@ $^

zssi31.s: zssi31.c
	$(CC) $(MTL_FLAGS) -qlist=$*.mtl.lst $(MTL_HEADERS) -o $@ $^

zssi31.o: zssi31.s
	$(ASM) $(ASM_FLAGS) -a=$*.asm.lst $(MACLIBS) -o $@ $^

#
# Console
#
zcn.o: zcn.cpp
	$(CXX) $(DLL_CPP_FLAGS) -qlist=$*.cpp.lst -o $@ $^

zutil.o: zutil.cpp
	$(CXX) $(DLL_CPP_FLAGS) -qlist=$*.cpp.lst -o $@ $^

zcnm.s: zcnm.c
	$(CC) $(MTL_FLAGS64) -qlist=$*.mtl.lst $(MTL_HEADERS) -o $@ $^

zcnm.o: zcnm.s
	$(ASM) $(ASM_FLAGS) -a=$*.asm.lst $(MACLIBS) -o $@ $^

zcnm31.s: zcnm31.c
	$(CC) $(MTL_FLAGS) -qlist=$*.mtl.lst $(MTL_HEADERS) -o $@ $^

zcnm31.o: zcnm31.s
	$(ASM) $(ASM_FLAGS) -a=$*.asm.lst $(MACLIBS) -o $@ $^

libzcn.so: zcn.o zcnm.o zcnm31.o
	$(CXX) $(DLL_BND_FLAGS) -o $@ $^ > $*.bind.lst

#
#
#
zowex: zowex.o
	$(CXX) $(CPP_BND_FLAGS) -o $@ $^ > $*.bind.lst

zowexx: zowex.o
	$(CXX) $(CPP_BND_FLAGS_AUTH) -o $@ $^ > $*.bind.lst

zowex.o: zowex.cpp
	$(CXX) $(CPP_FLAGS) -qlist=$*.cpp.lst -c $^

# zcli.o: zcli.cpp
# 	$(CXX) $(CPP_FLAGS) -qlist=$*.cpp.lst -c $^

#
# Misc targets
#
# extattr +ap libz.so
extattr:
	extattr +ap zowexx

clean: cleanu cleandump
	rm -f *.o
	rm -f *.dbg
	rm -f *.so
	rm -f *.x
	rm -f *.lst
	rm -f CEEDUMP.*