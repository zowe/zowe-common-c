c89 -D_OPEN_THREADS -D_XOPEN_SOURCE=600 -DAPF_AUTHORIZED=0 -DNOIBMHTTP \
"-Wa,goff" "-Wc,langlvl(EXTC99),float(HEX),agg,list(),so(),search(),\
goff,xref,gonum,roconst,gonum,asm,asmlib('SYS1.MACLIB'),asmlib('CEE.SCEEMAC'),LP64" "-Wl,lp64,xplink" \
-I ../h \
-o loggingtest \
./loggingtest.c \
../c/utils.c \
../c/logging.c \
../c/alloc.c \
../c/collections.c \
../c/zos.c \
../c/timeutls.c \
../c/le.c \
../c/scheduling.c \
../c/recovery.c

