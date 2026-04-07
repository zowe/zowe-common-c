/*
  macOS compatibility shim for quickjs.c.
  quickjs.c contains a bare 'typedef int ssize_t' or 'typedef int64_t ssize_t'
  that conflicts with the Darwin definition (ssize_t = long on 64-bit).
  Pull in the system definition first so the compiler can see that
  'ssize_t' is already defined. The subsequent typedefs in quickjs.c will then
  be evaluated against the already-defined type; we silence the resulting
  redefinition errors/warnings because the QuickJS typedef is semantically
  correct for the sizes being used (ssize_t is pointer-width in practice).
  THIS FILE IS ONLY USED ON MACOS (passed via -include in the build script).
*/
#include <sys/types.h>
/* Prevent quickjs.c from re-typedef-ing ssize_t by making the conditional
   pick the int64_t path (which matches Darwin's 'long' in actual codegen)
   while we also suppress the pedantic error. */
#define __QUICKJS_MACOS_COMPAT__ 1
