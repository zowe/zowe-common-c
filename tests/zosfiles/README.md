# Sample unit tests for zosfile.c

## Test targets

- directoryListEntries
- fileReadLink2
- directoryCopy
  - directoryChangeModeRecursive
  - directoryChangeOwnerRecursive
  - directoryDeleteRecursive

_(One test case in each line. Indent indicates dependencies.)_

## How to run

- Must build and run in z/OS _(USS)_.
- Requires `xlclang` and `make` _(tested with GNU make 4.1)_.

0. Enter `{repo}/tests/zosfiles` and run `make`.
1. Run `./prepare_test.sh`. It will generate a directory in `/tmp`. Pay attention to the line `Test data is in /tmp/xxxx`. You may check the execution result in the directory and you are responsible for deleting it when it is no longer needed.
2. Run `./zosfiles /tmp/xxxx`. /tmp/xxxx is the path printed by prepare_test.sh. If all test cases are passed, it will print `Done. Total: 6. Failed:0.` at the last line.

Hints: a single-line command that runs the 2 steps --> `./prepare_test.sh | xargs ./zosfiles`. Do not forget to delete the test directory in the end.