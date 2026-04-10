#!/bin/sh
# Run dumptest1 with LE ESPIE disabled so ESTAEX gets the abend directly.
#
# For an SVC dump, pre-arm a SLIP trap on the operator console first:
#   SLIP SET,COMP=0C4,ACTION=SVCD,JOBNAME=<yourjob>,END
#
# The trap is one-shot (auto-disables after first match) and transient
# (gone at next IPL).  Use SLIP LIST to see active traps.

HERE="$(cd "$(dirname "$0")" && pwd)"
export _CEE_RUNOPTS="TRAP(OFF)"
exec "${HERE}/dumptest1" "$@"
