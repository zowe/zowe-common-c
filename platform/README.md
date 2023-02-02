# Platform Notes

The zowe-common-c library is primarily driven by the need to have C programming interfaces for
making ZOS programming on C less painful regarding OS aware features.  However, the many parts
of this library can run on other platforms, which makes development and testing easier.  Non-ZOS
platform support files go in sub-directories here.   Since xlc and it's runtime libraries are 
highly POSIX compliant, there is not too much needed for Linux and other Unix's.  Windows needs
more support for quirky non-compatible IO, systems calls, time functions, regular-expressions, etc.

## CPP or not to be

C++ is not used on ZOS, because ZOWE-common-c targets both LE (Posix) runtime environents and Metal 
(traditional MVS) environments.  IBM's xlc C++ does not support Metal and probably never will.  So C++
is out in general.  However, to provide platform support on Windows, C++ is needed because API's are
often *only* published in C++. 
