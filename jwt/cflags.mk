TARGET := ZOSV2R1
MACHINE :=  ZSERIES-6
ARCH_6 := 6
ARCH_10 := 10

#FIXME MNT0130
INFO_FLAGS = INFO(ALL),CHECKOUT(ALL) #noals,cmp,gen,cns,eff,enu,par,por,pro,rea,ret,trd,use)
#CCN3196   Initialization between types "&1" and "&2" is not allowed
#CCN3224   Incorrect pragma ignored.
#CCN3280   Function argument assignment between types "&1" and "&2" is not allowed.
#CCN3304   No function prototype given for "&1".
#CCN3438  The variable "&1" might be used before it is set.
#CCN3449  Missing return expression.
#CCN3450  Obsolete non-prototype-style function declaration.
SEVERITY_E = CCN3196,CCN3224,CCN3280,CCN3304,CCN3438,CCN4332,CCN3449,CCN3450
#CCN3419 Converting 3637953233 to type "int" does not preserve its value.
#CCN3420 An unsigned comparison is performed between an unsigned value and a negative constant.
#CCN3426 An assignment expression is used as a condition. An equality comparison ( :=  := ) may have been intended.
#CCN3434 The left-hand side of a bitwise right shift expression has a signed promoted type.
#CCN3451 The target integral type cannot hold all possible values of the source integral type.
SEVERITY_W = CCN3419,CCN3420,CCN3426,CCN3434,CCN3451,CCN3280
#CN3997   Structure members cannot follow a flexible array member/zero extent array.
SEVERITY_I = CCN3997
#CCN3409 The static variable "..." is defined but never referenced.
#CCN3413 A goto statement is used. -- We're using goto to free resources (and only for that!)
#CCN3415 The external function definition "..." is never referenced. -- Way too many of these, nothing we can do to fix
#CCN3425 The condition is always false. -- Lots of false positives on TRACE
#CCN3446 Array element(s) [1] ... [4] will be initialized with a default value of 0. -- So what???
#CCN3447 The member(s) starting from "&1" will be initialized with a default value of 0. -- So what???
#CCN3457 File /RZ202B/usr/include/stdint.h has already been included. -- Way too many of these, nothing we can do to fix
#CCN3493 The external variable "" is defined but never referenced. -- Way too many of these, nothing we can do to fix
#CCN3495 Pointer type conversion found -- typically followed by another message with the specifics
SUPPRESS =  CCN3409,CCN3413,CCN3415,CCN3425,CCN3446,CCN3447,CCN3457,CCN3493,CCN3435,CCN3495


ifeq ($(BUILD_TYPE),DEBUG)
  ARCH := $(ARCH_10)
  CFLAGS_RTCHECK := -Wc,RTCHECK
  CFLAGS_DEBUG := -g -DJWT_DEBUG
else
  ARCH := $(ARCH_6)
  CFLAGS_RTCHECK :=
endif

CFLAGS_TARGET = -Wc,"TARGET($(TARGET)),ARCH($(ARCH))"
CFLAGS_INFO = -Wc,"$(INFO_FLAGS)"
CFLAGS_SEVERITY = -Wc,"SEVERITY(E($(SEVERITY_E)))" -Wc,"SEVERITY(W($(SEVERITY_W)))" \
                   -Wc,"SEVERITY(I($(SEVERITY_I)))" -Wc,"SUPPRESS($(SUPPRESS))"
CFLAGS_LISTING := -Wc,list,aggregate,xref

CFLAGS = -M -Wa,goff \
         -Wc,"LANGLVL(EXTC99),FLOAT(HEX),agg,exp,list,aggregate,xref,offset" \
         -Wc,"source,expmac,so(),goff" \
         -Wc,"gonum,roconst,ASM,ASMLIB('SYS1.MACLIB'),dll" \
         -Wc,"LANGLVL(EXTC99),FLOAT(HEX),agg,exp,list(),so(),goff,xref" \
				 $(CFLAGS_INFO) $(CFLAGS_SEVERITY) \
         $(CFLAGS_RTCHECK) $(CFLAGS_TARGET) $(CFLAGS_LISTING) $(CFLAGS_DEBUG)
         -Wl,ac=1
