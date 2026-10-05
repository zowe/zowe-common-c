/* This program and the accompanying materials are made available under */
/* the terms of the Eclipse Public License v2.0 which accompanies this */
/* distribution, and is available at https://www.eclipse.org/legal/epl-v20.html */
/* SPDX-License-Identifier: EPL-2.0 */
/* Copyright Contributors to the Zowe Project. */

/*
 * zowe_bpx_prototypes.h
 *
 * C prototypes for the IBM z/OS UNIX System Services (USS) Assembler
 * Callable Services (the BPX1xxx / BPX4xxx family). These are HLASM-defined
 * entry points and are not declared in any standard IBM C header; this file
 * exists so that C23 strict-prototype compilers (Open XL / ibm-clang64) do
 * not emit implicit-declaration diagnostics.
 *
 * Authoritative source:
 *   "z/OS UNIX System Services Programming: Assembler Callable Services
 *    Reference" (SA23-2281, v3r1, bpxb100_v3r1.pdf). Page citations below
 *    are pages of that PDF; chapter 1 ("Invocation details") defines the
 *    linkage, and chapter 2 the per-service parameter lists.
 *
 * Linkage (book pp.3-4):
 *   Register 1 points to a parameter list; each list entry is the address
 *   of a parameter. Every parameter is therefore passed BY REFERENCE. The
 *   last entry has its high-order bit set (AMODE 31 only).
 *
 *   Each .c file that calls a BPX service is responsible for issuing the
 *   per-symbol linkage pragmas BEFORE including this header, in the
 *   long-standing zowe-common-c idiom:
 *
 *       #ifdef _LP64
 *       #  pragma linkage(BPX4xxx, OS)
 *       #  define BPXxxx BPX4xxx
 *       #else
 *       #  pragma linkage(BPX1xxx, OS)
 *       #  define BPXxxx BPX1xxx
 *       #endif
 *       #include "zowe_bpx_prototypes.h"
 *
 *   The header does NOT issue its own #pragma linkage lines. An earlier
 *   experiment (2026-04-24) tried that and ibm-clang warned "function
 *   already has a linkage specified, ignoring pragma linkage" -- the .c
 *   file's pragma had already taken effect on the symbol by the time the
 *   header was processed, so the header's pragma was redundant noise.
 *
 *   This header declares only the two real symbols (BPX1xxx and BPX4xxx)
 *   per service, sharing a single per-service argument-list macro so the
 *   two declarations cannot drift. The unprefixed name BPXxxx is only
 *   ever a macro alias defined per-file via `#define BPXxxx BPXNxxx`
 *   before the include; declaring it here as well caused xlclang to see
 *   BPX4xxx declared twice (once via macro expansion of the unprefixed
 *   line, once explicit) and that second declaration was apparently
 *   shedding the OS linkage attribute, producing IEW2469E REASON 2
 *   ("xplink attributes of reference and target do not match") at link
 *   time. Removed in 2026-04-27 round.
 *
 * C-type mapping (book p.4, "Parameter descriptions"):
 *
 *   IBM data type                             C parameter type
 *   ----------------------------------------- -----------------
 *   Integer, fullword/doubleword, in-only,    int   (caller passes the value)
 *     passed by value at call sites
 *   Integer, fullword, returned or in/out     int *
 *   Integer, fullword, in-only passed by      int *
 *     address at call sites
 *   Character string, length N                char *
 *   Structure, length L (variable)            void *
 *   Structure, fullword (a bit-packed word)   int   or int * -- follow callers
 *   Address / Pointer, fullword/doubleword    void *
 *
 *   Under XL C's #pragma linkage(NAME, OS), "Integer/Fullword by value" and
 *   "Integer/Fullword by address" produce identical ABI calls: the compiler
 *   always sets up R1 -> [address of fullword-holding-the-value], either
 *   directly from the caller's &var or via a hidden temp it spills for a
 *   scalar. The choice is only about caller syntax. This header therefore
 *   matches each parameter's C type to how the existing zowe-common-c
 *   callers already write the call: no call-site churn is required for the
 *   header to compile clean.
 *
 *   Note on Address parameters. The book says these are "a fullword
 *   (doubleword) field that contains a pointer". A strict reading of the
 *   by-reference linkage would make the C type `void **` -- pointer to a
 *   word that holds a pointer. We use `void *` uniformly instead, because
 *   existing zowe-common-c callers pass *both* `int **` handles (e.g.
 *   `timeoutSpecHandle` in bpxskt.c) and `&zero` fullword-holders (e.g.
 *   the ECB slot) into these positions, and `void *` is the only C type
 *   that accepts both without a strict-prototype diagnostic. `void **`
 *   would be compatible with neither idiom (`void **` is NOT a generic
 *   pointer in C; only `void *` is). The small strictness loss is
 *   deliberate and is the reason the old (pre-strict) code compiled.
 *
 * Per-service style:
 *   - Each service defines a single BPX_<suffix>_ARGS macro holding the
 *     argument list. Both declarations (BPX1, BPX4) share that macro,
 *     so they cannot drift.
 *   - Parameter names track the IBM book (lowercased with underscores).
 *   - Directions are coded as "in", "out", "i/o" in comments.
 *   - Each service block cites the PDF page where its entry starts.
 */

#ifndef ZOWE_BPX_PROTOTYPES_H
#define ZOWE_BPX_PROTOTYPES_H

#include "zowetypes.h"

#ifdef __ZOWE_OS_ZOS

/* ------------------------------------------------------------------
 * File I/O
 * ------------------------------------------------------------------ */

/* close (BPX1CLO, BPX4CLO) -- p.159
 * CALL BPX1CLO,(File_descriptor, Return_value, Return_code, Reason_code) */
#define BPX_CLO_ARGS                                                           \
    int *file_descriptor,                 /* in  -- Integer,   fullword */     \
    int *return_value,                    /* out -- Integer,   fullword */     \
    int *return_code,                     /* out -- Integer,   fullword */     \
    int *reason_code                      /* out -- Integer,   fullword */
extern int BPX1CLO(BPX_CLO_ARGS);
extern int BPX4CLO(BPX_CLO_ARGS);

/* open (BPX1OPN, BPX4OPN) -- p.549
 * CALL BPX1OPN,(Pathname_length, Pathname, Options, Mode,
 *               Return_value, Return_code, Reason_code) */
#define BPX_OPN_ARGS                                                           \
    int  *pathname_length,                /* in  -- Integer,   fullword */     \
    char *pathname,                       /* in  -- Character, len=pathname_length */ \
    int  *options,                        /* in  -- Structure, fullword (O_* bits) */ \
    int  *mode,                           /* in  -- Structure, fullword (BPXYMODE) */ \
    int  *return_value,                   /* out -- Integer,   fullword */     \
    int  *return_code,                    /* out -- Integer,   fullword */     \
    int  *reason_code                     /* out -- Integer,   fullword */
extern int BPX1OPN(BPX_OPN_ARGS);
extern int BPX4OPN(BPX_OPN_ARGS);

/* read (BPX1RED, BPX4RED) -- p.690
 * CALL BPX1RED,(File_descriptor, Buffer_address, Buffer_ALET, Read_count,
 *               Return_value, Return_code, Reason_code) */
#define BPX_RED_ARGS                                                           \
    int  *file_descriptor,                /* in   -- Integer,  fullword */     \
    void *buffer_address,                 /* i/o  -- Address,  fullword/dword */\
    int  *buffer_alet,                    /* in   -- Integer,  fullword */     \
    int  *read_count,                     /* in   -- Integer,  fullword */     \
    int  *return_value,                   /* out  -- Integer,  fullword */     \
    int  *return_code,                    /* out  -- Integer,  fullword */     \
    int  *reason_code                     /* out  -- Integer,  fullword */
extern int BPX1RED(BPX_RED_ARGS);
extern int BPX4RED(BPX_RED_ARGS);

/* write (BPX1WRT, BPX4WRT) -- p.1070
 * CALL BPX1WRT,(File_descriptor, Buffer_address, Buffer_ALET, Write_count,
 *               Return_value, Return_code, Reason_code) */
#define BPX_WRT_ARGS                                                           \
    int  *file_descriptor,                /* in  -- Integer,  fullword */      \
    void *buffer_address,                 /* in  -- Address,  fullword/dword */\
    int  *buffer_alet,                    /* in  -- Integer,  fullword */      \
    int  *write_count,                    /* in  -- Integer,  fullword */      \
    int  *return_value,                   /* out -- Integer,  fullword */      \
    int  *return_code,                    /* out -- Integer,  fullword */      \
    int  *reason_code                     /* out -- Integer,  fullword */
extern int BPX1WRT(BPX_WRT_ARGS);
extern int BPX4WRT(BPX_WRT_ARGS);

/* fcntl (BPX1FCT, BPX4FCT) -- p.254
 * CALL BPX1FCT,(File_descriptor, Action, Argument,
 *               Return_value, Return_code, Reason_code) */
#define BPX_FCT_ARGS                                                           \
    int  *file_descriptor,                /* in   -- Integer,   fullword */    \
    int  *action,                         /* in   -- Structure, fullword (F_*)*/\
    void *argument,                       /* i/o  -- Structure, fullword/dword (type depends on action) */ \
    int  *return_value,                   /* out  -- Integer,   fullword */    \
    int  *return_code,                    /* out  -- Integer,   fullword */    \
    int  *reason_code                     /* out  -- Integer,   fullword */
extern int BPX1FCT(BPX_FCT_ARGS);
extern int BPX4FCT(BPX_FCT_ARGS);

/* w_ioctl (BPX1IOC, BPX4IOC) -- p.1041
 * CALL BPX1IOC,(File_descriptor, Command, Argument_length, Argument,
 *               Return_value, Return_code, Reason_code) */
#define BPX_IOC_ARGS                                                           \
    int  *file_descriptor,                /* in   -- Integer,  fullword */     \
    int  *command,                        /* in   -- Integer,  fullword (BPXYIOCC) */ \
    int  *argument_length,                /* in/o -- Integer,  fullword */     \
    void *argument,                       /* i/o  -- pfs-defined, len=argument_length */\
    int  *return_value,                   /* out  -- Integer,  fullword */     \
    int  *return_code,                    /* out  -- Integer,  fullword */     \
    int  *reason_code                     /* out  -- Integer,  fullword */
extern int BPX1IOC(BPX_IOC_ARGS);
extern int BPX4IOC(BPX_IOC_ARGS);

/* lstat (BPX1LST, BPX4LST) -- p.447
 * CALL BPX1LST,(Pathname_length, Pathname, Status_area_length, Status_area,
 *               Return_value, Return_code, Reason_code) */
#define BPX_LST_ARGS                                                           \
    int  *pathname_length,                /* in   -- Integer,  fullword */     \
    char *pathname,                       /* in   -- Character, len=pathname_length */ \
    int  *status_area_length,             /* i/o  -- Integer,  fullword */     \
    void *status_area,                    /* i/o  -- Structure, BPXYSTAT */    \
    int  *return_value,                   /* out  -- Integer,  fullword */     \
    int  *return_code,                    /* out  -- Integer,  fullword */     \
    int  *reason_code                     /* out  -- Integer,  fullword */
extern int BPX1LST(BPX_LST_ARGS);
extern int BPX4LST(BPX_LST_ARGS);

/* stat (BPX1STA, BPX4STA) -- p.931
 * CALL BPX1STA,(Pathname_length, Pathname, Status_area_length, Status_area,
 *               Return_value, Return_code, Reason_code) */
#define BPX_STA_ARGS                                                           \
    int  *pathname_length,                /* in   -- Integer,  fullword */     \
    char *pathname,                       /* in   -- Character, len=pathname_length */ \
    int  *status_area_length,             /* i/o  -- Integer,  fullword */     \
    void *status_area,                    /* i/o  -- Structure, BPXYSTAT */    \
    int  *return_value,                   /* out  -- Integer,  fullword */     \
    int  *return_code,                    /* out  -- Integer,  fullword */     \
    int  *reason_code                     /* out  -- Integer,  fullword */
extern int BPX1STA(BPX_STA_ARGS);
extern int BPX4STA(BPX_STA_ARGS);

/* pipe (BPX1PIP, BPX4PIP) -- p.589
 * CALL BPX1PIP,(Read_file_descriptor, Write_file_descriptor,
 *               Return_value, Return_code, Reason_code) */
#define BPX_PIP_ARGS                                                           \
    int *read_file_descriptor,            /* out -- Integer,  fullword */      \
    int *write_file_descriptor,           /* out -- Integer,  fullword */      \
    int *return_value,                    /* out -- Integer,  fullword */      \
    int *return_code,                     /* out -- Integer,  fullword */      \
    int *reason_code                      /* out -- Integer,  fullword */
extern int BPX1PIP(BPX_PIP_ARGS);
extern int BPX4PIP(BPX_PIP_ARGS);

/* ------------------------------------------------------------------
 * Directory
 * ------------------------------------------------------------------ */

/* opendir (BPX1OPD, BPX4OPD) -- p.561
 * CALL BPX1OPD,(Directory_name_length, Directory_name,
 *               Return_value, Return_code, Reason_code) */
#define BPX_OPD_ARGS                                                           \
    int  *directory_name_length,          /* in  -- Integer,  fullword */      \
    char *directory_name,                 /* in  -- Character, len=directory_name_length */ \
    int  *return_value,                   /* out -- Integer,  fullword (dirp or -1) */ \
    int  *return_code,                    /* out -- Integer,  fullword */      \
    int  *reason_code                     /* out -- Integer,  fullword */
extern int BPX1OPD(BPX_OPD_ARGS);
extern int BPX4OPD(BPX_OPD_ARGS);

/* closedir (BPX1CLD, BPX4CLD) -- p.162
 * CALL BPX1CLD,(Directory_file_descriptor,
 *               Return_value, Return_code, Reason_code) */
#define BPX_CLD_ARGS                                                           \
    int *directory_file_descriptor,       /* in  -- Integer, fullword */       \
    int *return_value,                    /* out -- Integer, fullword */       \
    int *return_code,                     /* out -- Integer, fullword */       \
    int *reason_code                      /* out -- Integer, fullword */
extern int BPX1CLD(BPX_CLD_ARGS);
extern int BPX4CLD(BPX_CLD_ARGS);

/* readdir (BPX1RDD, BPX4RDD) -- p.694
 * CALL BPX1RDD,(Directory_file_descriptor, Buffer_address, Buffer_ALET,
 *               Buffer_length, Return_value, Return_code, Reason_code) */
#define BPX_RDD_ARGS                                                           \
    int  *directory_file_descriptor,      /* i/o -- Integer, fullword */       \
    void *buffer_address,                 /* i/o -- Address, fullword/dword */ \
    int  *buffer_alet,                    /* in  -- Integer, fullword */       \
    int  *buffer_length,                  /* in  -- Integer, fullword */       \
    int  *return_value,                   /* out -- Integer, fullword */       \
    int  *return_code,                    /* out -- Integer, fullword */       \
    int  *reason_code                     /* out -- Integer, fullword */
extern int BPX1RDD(BPX_RDD_ARGS);
extern int BPX4RDD(BPX_RDD_ARGS);

/* mkdir (BPX1MKD, BPX4MKD) -- p.458
 * CALL BPX1MKD,(Pathname_length, Pathname, Mode,
 *               Return_value, Return_code, Reason_code) */
#define BPX_MKD_ARGS                                                           \
    int  *pathname_length,                /* in  -- Integer,  fullword */      \
    char *pathname,                       /* in  -- Character, len=pathname_length */ \
    int  *mode,                           /* in  -- Structure, fullword (BPXYMODE) */ \
    int  *return_value,                   /* out -- Integer,  fullword */      \
    int  *return_code,                    /* out -- Integer,  fullword */      \
    int  *reason_code                     /* out -- Integer,  fullword */
extern int BPX1MKD(BPX_MKD_ARGS);
extern int BPX4MKD(BPX_MKD_ARGS);

/* rmdir (BPX1RMD, BPX4RMD) -- p.741
 * CALL BPX1RMD,(Directory_name_length, Directory_name,
 *               Return_value, Return_code, Reason_code) */
#define BPX_RMD_ARGS                                                           \
    int  *directory_name_length,          /* in  -- Integer,  fullword */      \
    char *directory_name,                 /* in  -- Character, len=directory_name_length */ \
    int  *return_value,                   /* out -- Integer,  fullword */      \
    int  *return_code,                    /* out -- Integer,  fullword */      \
    int  *reason_code                     /* out -- Integer,  fullword */
extern int BPX1RMD(BPX_RMD_ARGS);
extern int BPX4RMD(BPX_RMD_ARGS);

/* ------------------------------------------------------------------
 * Path manipulation
 * ------------------------------------------------------------------ */

/* rename (BPX1REN, BPX4REN) -- p.728
 * CALL BPX1REN,(Old_name_length, Old_name, New_name_length, New_name,
 *               Return_value, Return_code, Reason_code) */
#define BPX_REN_ARGS                                                           \
    int  *old_name_length,                /* in  -- Integer,  fullword */      \
    char *old_name,                       /* in  -- Character, len=old_name_length */ \
    int  *new_name_length,                /* in  -- Integer,  fullword */      \
    char *new_name,                       /* in  -- Character, len=new_name_length */ \
    int  *return_value,                   /* out -- Integer,  fullword */      \
    int  *return_code,                    /* out -- Integer,  fullword */      \
    int  *reason_code                     /* out -- Integer,  fullword */
extern int BPX1REN(BPX_REN_ARGS);
extern int BPX4REN(BPX_REN_ARGS);

/* unlink (BPX1UNL, BPX4UNL) -- p.1004
 * CALL BPX1UNL,(Name_length, Name,
 *               Return_value, Return_code, Reason_code) */
#define BPX_UNL_ARGS                                                           \
    int  *name_length,                    /* in  -- Integer,  fullword */      \
    char *name,                           /* in  -- Character, len=name_length */ \
    int  *return_value,                   /* out -- Integer,  fullword */      \
    int  *return_code,                    /* out -- Integer,  fullword */      \
    int  *reason_code                     /* out -- Integer,  fullword */
extern int BPX1UNL(BPX_UNL_ARGS);
extern int BPX4UNL(BPX_UNL_ARGS);

/* readlink (BPX1RDL, BPX4RDL) -- p.704
 * CALL BPX1RDL,(Link_name_length, Link_name, Buffer_length, Buffer_address,
 *               Return_value, Return_code, Reason_code) */
#define BPX_RDL_ARGS                                                           \
    int  *link_name_length,               /* in  -- Integer,  fullword */      \
    char *link_name,                      /* in  -- Character, len=link_name_length */ \
    int  *buffer_length,                  /* in  -- Integer,  fullword */      \
    void *buffer_address,                 /* in  -- Address,  fullword/dword */\
    int  *return_value,                   /* out -- Integer,  fullword */      \
    int  *return_code,                    /* out -- Integer,  fullword */      \
    int  *reason_code                     /* out -- Integer,  fullword */
extern int BPX1RDL(BPX_RDL_ARGS);
extern int BPX4RDL(BPX_RDL_ARGS);

/* chmod (BPX1CHM, BPX4CHM) -- p.139
 * CALL BPX1CHM,(Pathname_length, Pathname, Mode,
 *               Return_value, Return_code, Reason_code) */
#define BPX_CHM_ARGS                                                           \
    int   pathname_length,                /* in  -- Integer,  fullword (by value) */ \
    char *pathname,                       /* in  -- Character, len=pathname_length */ \
    int   mode,                           /* in  -- Structure, fullword (BPXYMODE, by value) */ \
    int  *return_value,                   /* out -- Integer,  fullword */      \
    int  *return_code,                    /* out -- Integer,  fullword */      \
    int  *reason_code                     /* out -- Integer,  fullword */
extern int BPX1CHM(BPX_CHM_ARGS);
extern int BPX4CHM(BPX_CHM_ARGS);

/* chattr (BPX1CHR, BPX4CHR) -- p.120    NOTE: this is chattr, NOT chown.
 * CALL BPX1CHR,(Pathname_length, Pathname, Attributes_length, Attributes,
 *               Return_value, Return_code, Reason_code) */
#define BPX_CHR_ARGS                                                           \
    int   pathname_length,                /* in  -- Integer,  fullword (by value) */ \
    char *pathname,                       /* in  -- Character, len=pathname_length */ \
    int   attributes_length,              /* in  -- Integer,  fullword (by value) */ \
    void *attributes,                     /* in  -- Structure, BPXYATT */      \
    int  *return_value,                   /* out -- Integer,  fullword */      \
    int  *return_code,                    /* out -- Integer,  fullword */      \
    int  *reason_code                     /* out -- Integer,  fullword */
extern int BPX1CHR(BPX_CHR_ARGS);
extern int BPX4CHR(BPX_CHR_ARGS);

/* lchown (BPX1LCO, BPX4LCO) -- p.418
 * CALL BPX1LCO,(Pathname_length, Pathname, Owner_UID, Group_ID,
 *               Return_value, Return_code, Reason_code) */
#define BPX_LCO_ARGS                                                           \
    int  *pathname_length,                /* in  -- Integer,  fullword */      \
    char *pathname,                       /* in  -- Character, len=pathname_length */ \
    int   owner_uid,                      /* in  -- Integer,  fullword (by value) */ \
    int   group_id,                       /* in  -- Integer,  fullword (by value) */ \
    int  *return_value,                   /* out -- Integer,  fullword */      \
    int  *return_code,                    /* out -- Integer,  fullword */      \
    int  *reason_code                     /* out -- Integer,  fullword */
extern int BPX1LCO(BPX_LCO_ARGS);
extern int BPX4LCO(BPX_LCO_ARGS);

/* umask (BPX1UMK, BPX4UMK) -- p.998       NOTE: only 2 args, no rc/rsn.
 * CALL BPX1UMK,(File_mode_creation_mask, Return_value) */
#define BPX_UMK_ARGS                                                           \
    int  file_mode_creation_mask,         /* in  -- Integer,  fullword (by value) */ \
    int *return_value                     /* out -- Integer,  fullword */
extern int BPX1UMK(BPX_UMK_ARGS);
extern int BPX4UMK(BPX_UMK_ARGS);

/* symlink (BPX1SYM, BPX4SYM)
 * CALL BPX1SYM,(Pathname_length, Pathname, Link_name_length, Link_name,
 *               Return_value, Return_code, Reason_code) */
#define BPX_SYM_ARGS                                                           \
    int  *Pathname_length,                /* in  -- Integer,  fullword */      \
    char *Pathname,                       /* in  -- Character, len=Pathname_length */ \
    int  *Link_name_length,               /* in  -- Integer,  fullword */      \
    char *Link_name,                      /* in  -- Character, len=Link_name_length */ \
    int  *return_value,                   /* out -- Integer,  fullword */      \
    int  *return_code,                    /* out -- Integer,  fullword */      \
    int  *reason_code                     /* out -- Integer,  fullword */
extern int BPX1SYM(BPX_SYM_ARGS);
extern int BPX4SYM(BPX_SYM_ARGS);

/* ------------------------------------------------------------------
 * Sockets
 * ------------------------------------------------------------------ */

/* socket or socketpair (BPX1SOC, BPX4SOC) -- p.903
 * CALL BPX1SOC,(Domain, Type, Protocol, Dimension, Socket_vector,
 *               Return_value, Return_code, Reason_code) */
#define BPX_SOC_ARGS                                                           \
    int  domain,                          /* in  -- Integer,  fullword (AF_*, by value) */ \
    int  type,                            /* in  -- Integer,  fullword (SOCK_*, by value) */ \
    int  protocol,                        /* in  -- Integer,  fullword (by value) */ \
    int  dimension,                       /* in  -- Integer,  fullword (1 or 2, by value) */ \
    int *socket_vector,                   /* out -- Integer,  doubleword (fd[2])*/\
    int *return_value,                    /* out -- Integer,  fullword */      \
    int *return_code,                     /* out -- Integer,  fullword */      \
    int *reason_code                      /* out -- Integer,  fullword */
extern int BPX1SOC(BPX_SOC_ARGS);
extern int BPX4SOC(BPX_SOC_ARGS);

/* bind (BPX1BND, BPX4BND) -- p.114
 * CALL BPX1BND,(Socket_descriptor, Sockaddr_length, Sockaddr,
 *               Return_value, Return_code, Reason_code) */
#define BPX_BND_ARGS                                                           \
    int  *socket_descriptor,              /* in  -- Integer,  fullword */      \
    int  *sockaddr_length,                /* in  -- Integer,  fullword */      \
    void *sockaddr,                       /* in  -- BPXYSOCK, len=sockaddr_length */ \
    int  *return_value,                   /* out -- Integer,  fullword */      \
    int  *return_code,                    /* out -- Integer,  fullword */      \
    int  *reason_code                     /* out -- Integer,  fullword */
extern int BPX1BND(BPX_BND_ARGS);
extern int BPX4BND(BPX_BND_ARGS);

/* listen (BPX1LSN, BPX4LSN) -- p.429
 * CALL BPX1LSN,(Socket_descriptor, Backlog,
 *               Return_value, Return_code, Reason_code) */
#define BPX_LSN_ARGS                                                           \
    int *socket_descriptor,               /* in  -- Integer,  fullword */      \
    int *backlog,                         /* in  -- Integer,  fullword */      \
    int *return_value,                    /* out -- Integer,  fullword */      \
    int *return_code,                     /* out -- Integer,  fullword */      \
    int *reason_code                      /* out -- Integer,  fullword */
extern int BPX1LSN(BPX_LSN_ARGS);
extern int BPX4LSN(BPX_LSN_ARGS);

/* accept (BPX1ACP, BPX4ACP) -- p.51
 * CALL BPX1ACP,(Socket_descriptor, Sockaddr_length, Sockaddr,
 *               Return_value, Return_code, Reason_code) */
#define BPX_ACP_ARGS                                                           \
    int  *socket_descriptor,              /* i/o -- Integer,  fullword */      \
    int  *sockaddr_length,                /* i/o -- Integer,  fullword */      \
    void *sockaddr,                       /* i/o -- BPXYSOCK, len=sockaddr_length */ \
    int  *return_value,                   /* out -- Integer,  fullword */      \
    int  *return_code,                    /* out -- Integer,  fullword */      \
    int  *reason_code                     /* out -- Integer,  fullword */
extern int BPX1ACP(BPX_ACP_ARGS);
extern int BPX4ACP(BPX_ACP_ARGS);

/* connect (BPX1CON, BPX4CON) -- p.177
 * CALL BPX1CON,(Socket_descriptor, Sockaddr_length, Sockaddr,
 *               Return_value, Return_code, Reason_code) */
#define BPX_CON_ARGS                                                           \
    int  *socket_descriptor,              /* in  -- Integer,  fullword */      \
    int  *sockaddr_length,                /* in  -- Integer,  fullword */      \
    void *sockaddr,                       /* in  -- BPXYSOCK, len=sockaddr_length */ \
    int  *return_value,                   /* out -- Integer,  fullword */      \
    int  *return_code,                    /* out -- Integer,  fullword */      \
    int  *reason_code                     /* out -- Integer,  fullword */
extern int BPX1CON(BPX_CON_ARGS);
extern int BPX4CON(BPX_CON_ARGS);

/* getsockname or getpeername (BPX1GNM, BPX4GNM) -- p.357
 * CALL BPX1GNM,(Socket_descriptor, Operation, Sockaddr_length, Sockaddr,
 *               Return_value, Return_code, Reason_code)
 * NOTE: this is the 7-arg getsockname/getpeername service; do NOT confuse
 * with BPX1GNI (getnameinfo, 10 args), which is documented adjacent to it.
 * Socket_descriptor is by-address to match every other socket service in
 * this header (ACP, BND, CON, LSN, OPT, RFM, STO). Callers must pass
 * &socket->sd, not the bare struct pointer. */
#define BPX_GNM_ARGS                                                           \
    int  *socket_descriptor,              /* in   -- Integer,  fullword */     \
    int   operation,                      /* in   -- Integer,  fullword (BPXYSOCK op, by value) */ \
    int  *sockaddr_length,                /* i/o  -- Integer,  fullword */     \
    void *sockaddr,                       /* i/o  -- Character, len=sockaddr_length */ \
    int  *return_value,                   /* out  -- Integer,  fullword */     \
    int  *return_code,                    /* out  -- Integer,  fullword */     \
    int  *reason_code                     /* out  -- Integer,  fullword */
extern int BPX1GNM(BPX_GNM_ARGS);
extern int BPX4GNM(BPX_GNM_ARGS);

/* select/selectex (BPX1SEL, BPX4SEL) -- p.744
 * CALL BPX1SEL,(Number_msgsfds, Read_list_length, Read_list,
 *               Write_list_length, Write_list,
 *               Exception_list_length, Exception_list,
 *               Timeout_pointer, Ecb_pointer, User_option_field,
 *               Return_value, Return_code, Reason_code) */
#define BPX_SEL_ARGS                                                           \
    int  *number_msgsfds,                 /* in   -- Integer,  fullword */     \
    int  *read_list_length,               /* in   -- Integer,  fullword */     \
    void *read_list,                      /* i/o  -- Structure, len=read_list_length */ \
    int  *write_list_length,              /* in   -- Integer,  fullword */     \
    void *write_list,                     /* i/o  -- Structure, len=write_list_length */ \
    int  *exception_list_length,          /* in   -- Integer,  fullword */     \
    void *exception_list,                 /* i/o  -- Structure, len=exception_list_length */ \
    void *timeout_pointer,                /* in   -- Address,  fullword/dword (BPXYSELT or 0) */ \
    void *ecb_pointer,                    /* in   -- Address,  fullword/dword (ECB or 0) */ \
    int  *user_option_field,              /* i/o  -- Integer,  fullword (SEL#BITS{FORWARD,BACKWARD}) */ \
    int  *return_value,                   /* out  -- Integer,  fullword */     \
    int  *return_code,                    /* out  -- Integer,  fullword */     \
    int  *reason_code                     /* out  -- Integer,  fullword */
extern int BPX1SEL(BPX_SEL_ARGS);
extern int BPX4SEL(BPX_SEL_ARGS);

/* poll (BPX1POL, BPX4POL) -- p.602
 * CALL BPX1POL,(PollArrayPtr, NMsgsFds, Timeout,
 *               Return_value, Return_code, Reason_code) */
#define BPX_POL_ARGS                                                           \
    void *poll_array_ptr,                 /* in  -- Address,  fullword/dword (array of pollfd) */ \
    int  *n_msgs_fds,                     /* in  -- Integer,  fullword (hi hw=#MQs, lo hw=#fds) */\
    int  *timeout,                        /* in  -- Integer,  fullword (msec; -1=infinite) */\
    int  *return_value,                   /* out -- Integer,  fullword */      \
    int  *return_code,                    /* out -- Integer,  fullword */      \
    int  *reason_code                     /* out -- Integer,  fullword */
extern int BPX1POL(BPX_POL_ARGS);
extern int BPX4POL(BPX_POL_ARGS);

/* getsockopt or setsockopt (BPX1OPT, BPX4OPT) -- p.360
 * CALL BPX1OPT,(Socket_descriptor, Operation, Level, Option_name,
 *               Option_data_length, Option_data,
 *               Return_value, Return_code, Reason_code) */
#define BPX_OPT_ARGS                                                           \
    int  *socket_descriptor,              /* in   -- Integer,  fullword */     \
    int  *operation,                      /* in   -- Integer,  fullword (GET/SET) */\
    int  *level,                          /* i/o  -- Integer,  fullword */     \
    int  *option_name,                    /* i/o  -- Integer,  fullword */     \
    int  *option_data_length,             /* i/o  -- Integer,  fullword */     \
    char *option_data,                    /* i/o  -- Character, len=option_data_length */ \
    int  *return_value,                   /* out  -- Integer,  fullword */     \
    int  *return_code,                    /* out  -- Integer,  fullword */     \
    int  *reason_code                     /* out  -- Integer,  fullword */
extern int BPX1OPT(BPX_OPT_ARGS);
extern int BPX4OPT(BPX_OPT_ARGS);

/* sendto (BPX1STO, BPX4STO) -- p.776
 * CALL BPX1STO,(Socket_descriptor, Buffer_length, Buffer, Buffer_alet, Flags,
 *               Sockaddr_length, Sockaddr,
 *               Return_value, Return_code, Reason_code) */
#define BPX_STO_ARGS                                                           \
    int  *socket_descriptor,              /* in   -- Integer,  fullword */     \
    int  *buffer_length,                  /* i/o  -- Integer,  fullword */     \
    char *buffer,                         /* in   -- Character, len=buffer_length */ \
    int  *buffer_alet,                    /* in   -- Integer,  fullword */     \
    int  *flags,                          /* in   -- Structure, fullword (BPXYMSGF) */ \
    int  *sockaddr_length,                /* i/o  -- Integer,  fullword */     \
    void *sockaddr,                       /* i/o  -- BPXYSOCK, len=sockaddr_length */ \
    int  *return_value,                   /* out  -- Integer,  fullword */     \
    int  *return_code,                    /* out  -- Integer,  fullword */     \
    int  *reason_code                     /* out  -- Integer,  fullword */
extern int BPX1STO(BPX_STO_ARGS);
extern int BPX4STO(BPX_STO_ARGS);

/* recvfrom (BPX1RFM, BPX4RFM) -- p.721
 * CALL BPX1RFM,(Socket_descriptor, Buffer_length, Buffer, Buffer_alet, Flags,
 *               Sockaddr_length, Sockaddr,
 *               Return_value, Return_code, Reason_code) */
#define BPX_RFM_ARGS                                                           \
    int  *socket_descriptor,              /* in   -- Integer,  fullword */     \
    int  *buffer_length,                  /* i/o  -- Integer,  fullword */     \
    char *buffer,                         /* out  -- Character, len=buffer_length */ \
    int  *buffer_alet,                    /* in   -- Integer,  fullword */     \
    int  *flags,                          /* in   -- Integer,  fullword (BPXYMSGF) */\
    int  *sockaddr_length,                /* i/o  -- Integer,  fullword */     \
    void *sockaddr,                       /* i/o  -- BPXYSOCK, len=sockaddr_length */ \
    int  *return_value,                   /* out  -- Integer,  fullword */     \
    int  *return_code,                    /* out  -- Integer,  fullword */     \
    int  *reason_code                     /* out  -- Integer,  fullword */
extern int BPX1RFM(BPX_RFM_ARGS);
extern int BPX4RFM(BPX_RFM_ARGS);

/* gethostid or gethostname (BPX1HST, BPX4HST) -- p.323
 * CALL BPX1HST,(Domain, Name_length, Name,
 *               Return_value, Return_code, Reason_code) */
#define BPX_HST_ARGS                                                           \
    int   domain,                         /* in  -- Integer,  fullword (AF_*, by value) */ \
    int  *name_length,                    /* i/o -- Integer,  fullword (book says Integer; signedness unspecified, signed wins) */ \
    char *name,                           /* out -- Character, len=name_length */ \
    int  *return_value,                   /* out -- Integer,  fullword */      \
    int  *return_code,                    /* out -- Integer,  fullword */      \
    int  *reason_code                     /* out -- Integer,  fullword */
extern int BPX1HST(BPX_HST_ARGS);
extern int BPX4HST(BPX_HST_ARGS);

/* gethostbyname (BPX1GHN, BPX4GHN) -- p.320
 * CALL BPX1GHN,(Name, Name_length, Hostent_ptr,
 *               Return_value, Return_code, Reason_code) */
#define BPX_GHN_ARGS                                                           \
    char *name,                           /* in  -- Character, len=name_length */ \
    int  *name_length,                    /* in  -- Integer,  fullword */      \
    void *hostent_ptr,                    /* out -- Address,  fullword/dword (-> Hostent) */ \
    int  *return_value,                   /* out -- Integer,  fullword */      \
    int  *return_code,                    /* out -- Integer,  fullword */      \
    int  *reason_code                     /* out -- Integer,  fullword */
extern int BPX1GHN(BPX_GHN_ARGS);
extern int BPX4GHN(BPX_GHN_ARGS);

/* getaddrinfo (BPX1GAI, BPX4GAI) -- p.289
 * CALL BPX1GAI,(Node_Name, Node_Name_Length, Service_Name, Service_Name_Length,
 *               Hints_Ptr, Results_Ptr, Canonical_Length,
 *               Return_value, Return_code, Reason_code) */
#define BPX_GAI_ARGS                                                           \
    char *node_name,                      /* in  -- Character, len=node_name_length */ \
    int  *node_name_length,               /* in  -- Integer,  fullword */      \
    char *service_name,                   /* in  -- Character, len=service_name_length */ \
    int  *service_name_length,            /* in  -- Integer,  fullword */      \
    void *hints_ptr,                      /* in  -- Address,  fullword/dword (-> Addr_Info) */ \
    void *results_ptr,                    /* out -- Address,  fullword/dword (-> Addr_Info list) */ \
    int  *canonical_length,               /* out -- Integer,  fullword */      \
    int  *return_value,                   /* out -- Integer,  fullword */      \
    int  *return_code,                    /* out -- Integer,  fullword */      \
    int  *reason_code                     /* out -- Integer,  fullword */
extern int BPX1GAI(BPX_GAI_ARGS);
extern int BPX4GAI(BPX_GAI_ARGS);

/* freeaddrinfo (BPX1FAI, BPX4FAI) -- p.274
 * CALL BPX1FAI,(Addr_Info_Ptr,
 *               Return_value, Return_code, Reason_code) */
#define BPX_FAI_ARGS                                                           \
    void *addr_info_ptr,                  /* in  -- Address,  fullword/dword */\
    int  *return_value,                   /* out -- Integer,  fullword */      \
    int  *return_code,                    /* out -- Integer,  fullword */      \
    int  *reason_code                     /* out -- Integer,  fullword */
extern int BPX1FAI(BPX_FAI_ARGS);
extern int BPX4FAI(BPX_FAI_ARGS);

/* ------------------------------------------------------------------
 * Identity / user database
 * ------------------------------------------------------------------ */

/* getgrgid (BPX1GGI, BPX4GGI) -- p.306
 * CALL BPX1GGI,(Group_ID, Return_value, Return_code, Reason_code) */
#define BPX_GGI_ARGS                                                           \
    int   group_id,                       /* in  -- Integer, fullword (by value) */ \
    void *return_value,                   /* out -- Address, fullword (-> group struct or 0) */ \
    int  *return_code,                    /* out -- Integer, fullword */       \
    int  *reason_code                     /* out -- Integer, fullword */
extern int BPX1GGI(BPX_GGI_ARGS);
extern int BPX4GGI(BPX_GGI_ARGS);

/* getgrnam (BPX1GGN, BPX4GGN) -- p.309
 * CALL BPX1GGN,(Group_name_length, Group_name,
 *               Return_value, Return_code, Reason_code) */
#define BPX_GGN_ARGS                                                           \
    int  *group_name_length,              /* in  -- Integer,  fullword */      \
    char *group_name,                     /* in  -- Character, len=group_name_length */ \
    void *return_value,                   /* out -- Address,  fullword (-> group struct or 0) */ \
    int  *return_code,                    /* out -- Integer,  fullword */      \
    int  *reason_code                     /* out -- Integer,  fullword */
extern int BPX1GGN(BPX_GGN_ARGS);
extern int BPX4GGN(BPX_GGN_ARGS);

/* getpwnam (BPX1GPN, BPX4GPN) -- p.343
 * CALL BPX1GPN,(User_name_length, User_name,
 *               Return_value, Return_code, Reason_code) */
#define BPX_GPN_ARGS                                                           \
    int  *user_name_length,               /* in  -- Integer,  fullword */      \
    char *user_name,                      /* in  -- Character, len=user_name_length */ \
    void *return_value,                   /* out -- Address,  fullword (-> passwd struct or 0) */ \
    int  *return_code,                    /* out -- Integer,  fullword */      \
    int  *reason_code                     /* out -- Integer,  fullword */
extern int BPX1GPN(BPX_GPN_ARGS);
extern int BPX4GPN(BPX_GPN_ARGS);

/* getpwuid (BPX1GPU, BPX4GPU) -- p.346
 * CALL BPX1GPU,(User_ID, Return_value, Return_code, Reason_code) */
#define BPX_GPU_ARGS                                                           \
    int   user_id,                        /* in  -- Integer, fullword (by value) */ \
    void *return_value,                   /* out -- Address, fullword (-> passwd struct or 0) */ \
    int  *return_code,                    /* out -- Integer, fullword */       \
    int  *reason_code                     /* out -- Integer, fullword */
extern int BPX1GPU(BPX_GPU_ARGS);
extern int BPX4GPU(BPX_GPU_ARGS);

/* getgroupsbyname (BPX1GUG, BPX4GUG) -- p.314
 * CALL BPX1GUG,(User_name_length, User_name,
 *               Group_ID_list_size, Group_ID_list_pointer_address,
 *               Number_of_group_IDs, Return_code, Reason_code) */
#define BPX_GUG_ARGS                                                           \
    int  *user_name_length,               /* in  -- Integer,  fullword */      \
    char *user_name,                      /* in  -- Character, len=user_name_length */ \
    int  *group_id_list_size,             /* in  -- Integer,  fullword */      \
    void *group_id_list_pointer_address,  /* in  -- Address,  fullword/dword */\
    int  *number_of_group_ids,            /* out -- Integer,  fullword */      \
    int  *return_code,                    /* out -- Integer,  fullword */      \
    int  *reason_code                     /* out -- Integer,  fullword */
extern int BPX1GUG(BPX_GUG_ARGS);
extern int BPX4GUG(BPX_GUG_ARGS);

/* __passwd / __passwd__applid (BPX1PWD, BPX4PWD) -- p.568
 * CALL BPX1PWD,(User_name_length, User_name, Pass_length, Pass,
 *               New_Pass_length, New_Pass,
 *               Return_value, Return_code, Reason_code) */
#define BPX_PWD_ARGS                                                           \
    int  *user_name_length,               /* in  -- Integer,  fullword */      \
    char *user_name,                      /* in  -- Character, len=user_name_length */ \
    int  *pass_length,                    /* in  -- Integer,  fullword */      \
    char *pass,                           /* in  -- Character, len=pass_length */ \
    int  *new_pass_length,                /* in  -- Integer,  fullword */      \
    char *new_pass,                       /* in  -- Character, len=new_pass_length */ \
    int  *return_value,                   /* out -- Integer,  fullword */      \
    int  *return_code,                    /* out -- Integer,  fullword */      \
    int  *reason_code                     /* out -- Integer,  fullword */
extern int BPX1PWD(BPX_PWD_ARGS);
extern int BPX4PWD(BPX_PWD_ARGS);

/* ------------------------------------------------------------------
 * Signals / thread security
 * ------------------------------------------------------------------ */

/* sigaction (BPX1SIA, BPX4SIA) -- p.872
 * CALL BPX1SIA,(Signal, New_sa_handler_address, New_sa_mask, New_sa_flags,
 *               Old_sa_handler_address, Old_sa_mask, Old_sa_flags,
 *               User_data, Return_value, Return_code, Reason_code) */
#define BPX_SIA_ARGS                                                           \
    int  *signal,                         /* in   -- Integer,   fullword */    \
    void *new_sa_handler_address,         /* in   -- Address,   fullword/dword */\
    void *new_sa_mask,                    /* in   -- Structure, 8 bytes */     \
    int  *new_sa_flags,                   /* in   -- Structure, fullword (SA_*)*/\
    void *old_sa_handler_address,         /* i/o  -- Address,   fullword/dword */\
    void *old_sa_mask,                    /* out  -- Structure, 8 bytes */     \
    int  *old_sa_flags,                   /* out  -- Structure, fullword */    \
    char *user_data,                      /* in   -- Character, fullword/dword */\
    int  *return_value,                   /* out  -- Integer,   fullword */    \
    int  *return_code,                    /* out  -- Integer,   fullword */    \
    int  *reason_code                     /* out  -- Integer,   fullword */
extern int BPX1SIA(BPX_SIA_ARGS);
extern int BPX4SIA(BPX_SIA_ARGS);

/* pthread_security_np / pthread_security_applid_np (BPX1TLS, BPX4TLS) -- p.639
 * CALL BPX1TLS,(Function_code, Identity_Type, Identity_Length, Identity,
 *               Pass_Length, Pass, Option_Flags,
 *               Return_value, Return_code, Reason_code) */
#define BPX_TLS_ARGS                                                           \
    int   function_code,                  /* in  -- Integer,  fullword (by value) */ \
    int   identity_type,                  /* in  -- Integer,  fullword (by value) */ \
    int   identity_length,                /* in  -- Integer,  fullword (by value) */ \
    void *identity,                       /* in  -- Character or Structure, len=identity_length */ \
    int   pass_length,                    /* in  -- Integer,  fullword (by value) */ \
    char *pass,                           /* in  -- Character, len=pass_length */ \
    int   option_flags,                   /* in  -- Structure, fullword (by value) */ \
    int  *return_value,                   /* out -- Integer,  fullword */      \
    int  *return_code,                    /* out -- Integer,  fullword */      \
    int  *reason_code                     /* out -- Integer,  fullword */
extern int BPX1TLS(BPX_TLS_ARGS);
extern int BPX4TLS(BPX_TLS_ARGS);

/* ------------------------------------------------------------------
 * Miscellaneous
 * ------------------------------------------------------------------ */

/* sleep (BPX1SLP, BPX4SLP) -- p.897      NOTE: only 2 args, no rc/rsn.
 * CALL BPX1SLP,(Seconds, Return_value) */
#define BPX_SLP_ARGS                                                           \
    unsigned int seconds,                 /* in  -- Integer, fullword (unsigned, by value) */ \
    int         *return_value             /* out -- Integer, fullword (remaining seconds) */
extern int BPX1SLP(BPX_SLP_ARGS);
extern int BPX4SLP(BPX_SLP_ARGS);

/* querydub (BPX1QDB, BPX4QDB) -- p.684
 * CALL BPX1QDB,(Return_value, Return_code, Reason_code) */
#define BPX_QDB_ARGS                                                           \
    int *return_value,                    /* out -- Integer, fullword */       \
    int *return_code,                     /* out -- Integer, fullword */       \
    int *reason_code                      /* out -- Integer, fullword */
extern int BPX1QDB(BPX_QDB_ARGS);
extern int BPX4QDB(BPX_QDB_ARGS);

#endif /* __ZOWE_OS_ZOS */
#endif /* ZOWE_BPX_PROTOTYPES_H */
