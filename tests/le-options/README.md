# le-options: asking Language Environment which heap options are in effect

`getEDB()`, `getOCB()` and `getLEHeapOptions()` in `h/le.h` walk LE's
control blocks from the CAA to the Options Control Block (OCB) and report
whether `HEAPPOOLS`, `HEAPPOOLS64` and `HEAPZONES` are in effect for the
running enclave. This test builds that code the way the shipping products
build it and checks it against live heap behaviour under several
`_CEE_RUNOPTS` settings.

## Why

Code that peeks at heap element headers (for example a `malloc_usable_size`
stand-in) is only correct on a stock LE heap. `HEAPPOOLS64(ON)` replaces the
length word with a pool index and `HEAPZONES` appends a check zone, either
of which turns such code into a heap overwrite. Rather than parse
`_CEE_RUNOPTS` (which misses CEEPRMxx defaults) or guess from behaviour, ask
LE: the options in effect are six loads away.

## The chain

    AMODE 64                                     AMODE 31
    PSA+0x4B8  PSALAA        -> LAA              R12 -> CAA
    LAA+0x58   CEELAA_LCA64  -> LCA
    LCA+0x08   CEELCA_CAA    -> CAA  (self-pointer at CAA+0x3A0)
    CAA+0x388  CEECAAEDB     -> EDB  'CEEEDB'    CAA+0x2F0
    EDB+0x110  CEEEDBOPTCB   -> OCB  'CELQOCB'   EDB+0x10   'CEEOCB'
    OCB+0x1E4  CEEOCB_HEAPPOOLS_BIT_FLAG     X'80' = ON
    OCB+0x20C  CEEOCB_HEAPPOOLS64_BIT_FLAG   X'80' = ON
    OCB+0x250  CEEOCB_HEAPZONES_SUB_OPTIONS  -> +0x04 SIZE31, +0x0C SIZE64

The offsets were computed by HLASM from the `CEELAA`, `CEELCA`, `CEECAA`,
`CEEEDB` and `CEEOCB` mappings in `CEE.SCEEMAC` (`SYSSTATE AMODE64=YES`
selects the 64-bit CAA and EDB forms; the OCB layout is the same in both
modes). The OCB is described in the LE Vendor Interfaces book. Three things
the mappings do not say, all learned the hard way:

- the 64-bit LE's OCB eyecatcher is `CELQOCB`, not `CEEOCB`;
- `CEEOCB_*_SUB_OPTIONS` is declared `DS A` but holds an offset from the
  start of the OCB, not an address; following it as one takes an 0C4;
- `HEAPZONES` has no ON bit. Its flag byte reads X'01' either way; the
  option is in effect when the size in its sub-options is nonzero.

## Running

    cd tests/le-options
    sh run-zos.sh              # xlclang 64-bit, xlc 31-bit (as ZSS builds), ibm-clang64
    sh run-zos.sh xlc31        # one build

Each build runs five cases: no options, `HEAPPOOLS64(ON)`, `HEAPPOOLS(ON)`,
`HEAPZONES(32,MSG,32,MSG)`, and both pool options together. The test asserts
the values `getLEHeapOptions` reports and, in 64-bit builds, cross-checks
them against the header word of a fresh `malloc(1)`: 0x20 on a stock heap,
a small pool index under `HEAPPOOLS64`, larger than 0x20 under `HEAPZONES`.
The two sources have to agree, so the test stays honest if a field moves.

Expected: every case prints `0 failures`, and the script exits 0.
