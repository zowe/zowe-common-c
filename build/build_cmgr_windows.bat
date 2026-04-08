@echo off

:: This program and the accompanying materials are
:: made available under the terms of the Eclipse Public License v2.0 which accompanies
:: this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
::
:: SPDX-License-Identifier: EPL-2.0
::
:: Copyright Contributors to the Zowe Project.

:: Build script for configmgr on Windows using LLVM clang.
::
:: Prerequisites
::   - clang / clang++ available on PATH  (https://releases.llvm.org/)
::   - git available on PATH (for cloning dependencies when not yet present)
::   - Microsoft CRT headers (from Visual Studio Build Tools or the Windows SDK)
::     clang on Windows defaults to targeting the MSVC ABI and therefore
::     requires the MSVC CRT headers.  The easiest way to obtain them is to
::     install "Desktop development with C++" from the Visual Studio Installer.
::
:: Usage
::   build_cmgr_windows.bat                  (uses clang / clang++ from PATH)
::   set CC=clang-cl && build_cmgr_windows.bat
::
:: Output
::   bin\configmgr.exe relative to the zowe-common-c root.

setlocal enabledelayedexpansion

echo ********************************************************************************
echo Building configmgr for Windows...
echo ********************************************************************************

:: ---------------------------------------------------------------------------
:: Locate the build\ directory and the zowe-common-c root (its parent).
:: ---------------------------------------------------------------------------
set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"

pushd "%SCRIPT_DIR%\.."
set "COMMON=%CD%"
popd

:: ---------------------------------------------------------------------------
:: Load project metadata from configmgr.proj.env.
:: The file uses sh-style  KEY="VALUE"  or  KEY=VALUE  lines.
:: We strip double-quote delimiters from values after reading.
:: ---------------------------------------------------------------------------
echo Loading project environment from configmgr.proj.env...
for /f "usebackq tokens=1* delims==" %%a in ("%SCRIPT_DIR%\configmgr.proj.env") do (
    set "_k=%%a"
    set "_v=%%b"
    :: Strip surrounding double-quotes
    set "_v=!_v:"=!"
    if "!_k!" == "PROJECT"        set "PROJECT=!_v!"
    if "!_k!" == "VERSION"        set "VERSION=!_v!"
    if "!_k!" == "QUICKJS"        set "QUICKJS=!_v!"
    if "!_k!" == "QUICKJS_SOURCE" set "QUICKJS_SOURCE=!_v!"
    if "!_k!" == "QUICKJS_BRANCH" set "QUICKJS_BRANCH=!_v!"
    if "!_k!" == "LIBYAML"        set "LIBYAML=!_v!"
    if "!_k!" == "LIBYAML_SOURCE" set "LIBYAML_SOURCE=!_v!"
    if "!_k!" == "LIBYAML_BRANCH" set "LIBYAML_BRANCH=!_v!"
)

echo   Project : %PROJECT%
echo   Version : %VERSION%

if "%PROJECT%"==""  ( echo ERROR: PROJECT not set in configmgr.proj.env & exit /b 1 )
if "%VERSION%"==""  ( echo ERROR: VERSION not set in configmgr.proj.env  & exit /b 1 )
if "%QUICKJS%"==""  ( echo ERROR: QUICKJS not set in configmgr.proj.env  & exit /b 1 )
if "%LIBYAML%"==""  ( echo ERROR: LIBYAML not set in configmgr.proj.env  & exit /b 1 )

:: ---------------------------------------------------------------------------
:: Split VERSION string (e.g. "3.5.0") into MAJOR / MINOR / PATCH components.
:: ---------------------------------------------------------------------------
for /f "tokens=1,2,3 delims=." %%a in ("%VERSION%") do (
    set "MAJOR=%%a"
    set "MINOR=%%b"
    set "PATCH=%%c"
)

:: ---------------------------------------------------------------------------
:: Resolve dependency paths.
:: ---------------------------------------------------------------------------
set "DEPS_DESTINATION=%COMMON%\deps\%PROJECT%"
set "LIBYAML_SRC=%DEPS_DESTINATION%\%LIBYAML%\src"
set "LIBYAML_INC=%DEPS_DESTINATION%\%LIBYAML%\include"
set "QJS=%DEPS_DESTINATION%\%QUICKJS%"

:: ---------------------------------------------------------------------------
:: Check / clone dependencies (matches dependencies.sh logic).
:: ---------------------------------------------------------------------------
echo ********************************************************************************
echo Checking dependencies...
if not exist "%DEPS_DESTINATION%" mkdir "%DEPS_DESTINATION%"

if not exist "%DEPS_DESTINATION%\%QUICKJS%" (
    echo Cloning QuickJS from %QUICKJS_SOURCE% @ %QUICKJS_BRANCH%...
    git clone --branch "%QUICKJS_BRANCH%" "%QUICKJS_SOURCE%" "%DEPS_DESTINATION%\%QUICKJS%"
    if errorlevel 1 ( echo ERROR: Failed to clone QuickJS. & exit /b 1 )
) else (
    echo   QuickJS already present: %DEPS_DESTINATION%\%QUICKJS%
)

if not exist "%DEPS_DESTINATION%\%LIBYAML%" (
    echo Cloning libyaml from %LIBYAML_SOURCE% @ %LIBYAML_BRANCH%...
    git clone --branch "%LIBYAML_BRANCH%" "%LIBYAML_SOURCE%" "%DEPS_DESTINATION%\%LIBYAML%"
    if errorlevel 1 ( echo ERROR: Failed to clone libyaml. & exit /b 1 )
) else (
    echo   libyaml already present: %DEPS_DESTINATION%\%LIBYAML%
)
echo Dependency setup complete

:: ---------------------------------------------------------------------------
:: Output binary.
:: ---------------------------------------------------------------------------
set "OUTPUT=%COMMON%\bin\configmgr.exe"
if not exist "%COMMON%\bin" mkdir "%COMMON%\bin"
if exist "%OUTPUT%" del /f /q "%OUTPUT%"

:: ---------------------------------------------------------------------------
:: Compiler / linker selection.
:: Allow callers to override via the CC / CXX environment variables.
:: ---------------------------------------------------------------------------
if "%CC%"==""  set "CC=clang"
if "%CXX%"=="" set "CXX=clang++"

echo Using C   compiler: %CC%
echo Using C++ compiler: %CXX%

:: ---------------------------------------------------------------------------
:: Create a temporary build directory for intermediate .o files.
:: ---------------------------------------------------------------------------
set "TMP_DIR=%TEMP%\configmgr_build_%RANDOM%"
mkdir "%TMP_DIR%"
if errorlevel 1 ( echo ERROR: Cannot create temp directory %TMP_DIR% & exit /b 1 )

:: ---------------------------------------------------------------------------
:: Write version / string-literal defines to a response file so that
:: embedded double-quote characters do not need CMD batch-level escaping.
:: clang supports @<file> response files on Windows.
:: ---------------------------------------------------------------------------
set "RSP=%TMP_DIR%\string_defs.rsp"
(
    echo -DCONFIG_VERSION="2021-03-27"
    echo -DYAML_VERSION_STRING="%MAJOR%.%MINOR%.%PATCH%"
) > "%RSP%"

:: ---------------------------------------------------------------------------
:: Common compiler flags.
::
::   -D_CRT_SECURE_NO_WARNINGS   suppress MSVC CRT security deprecation notices
::   -Dstrdup=_strdup            MSVC CRT uses _strdup instead of strdup
::   -DCMGRTEST=1                enable the main() entry point in configmgr.c
::   -DCONFIG_BIGNUM=1           enable libbf inside QuickJS (matches z/OS build)
::   -DYAML_DECLARE_STATIC       link libyaml inline rather than as a DLL
::
:: Include paths:
::   %COMMON%\h                  zowe-common-c public headers
::   %COMMON%\platform\windows   Windows-specific headers (winregex.h, etc.)
::   %LIBYAML_INC%               libyaml public headers
::   %QJS%                       quickjs-portable root (quickjs.h, cutils.h, …)
::   %QJS%\porting               Windows porting shims (winstdio.h, wintime.h, …)
:: ---------------------------------------------------------------------------
set "CFLAGS=-std=gnu11 -Wall -Wno-unused-function -Wno-unused-variable"
set "CFLAGS=%CFLAGS% -g -O2"
set "CFLAGS=%CFLAGS% -I%COMMON%\h"
set "CFLAGS=%CFLAGS% -I%COMMON%\platform\windows"
set "CFLAGS=%CFLAGS% -I%LIBYAML_INC%"
set "CFLAGS=%CFLAGS% -I%QJS%"
set "CFLAGS=%CFLAGS% -I%QJS%\porting"
set "CFLAGS=%CFLAGS% -D_CRT_SECURE_NO_WARNINGS"
set "CFLAGS=%CFLAGS% -Dstrdup=_strdup"
set "CFLAGS=%CFLAGS% -DCMGRTEST=1"
set "CFLAGS=%CFLAGS% -DCONFIG_BIGNUM=1"
set "CFLAGS=%CFLAGS% -DYAML_VERSION_MAJOR=%MAJOR%"
set "CFLAGS=%CFLAGS% -DYAML_VERSION_MINOR=%MINOR%"
set "CFLAGS=%CFLAGS% -DYAML_VERSION_PATCH=%PATCH%"
set "CFLAGS=%CFLAGS% -DYAML_DECLARE_STATIC=1"

:: Extra flags for QuickJS sources only.
::   Pre-include platform\common\quickjs_windows_compat.h to ensure ssize_t is
::   correctly typed as int64_t before quickjs.c's internal typedef.  In MSVC
::   mode the guard inside that header is a no-op because winstdio.h already
::   defined ssize_t via _SSIZE_T_DEFINED; in MinGW / non-MSVC clang mode the
::   header provides the definition.  Either way the subsequent compatible
::   typedef redeclaration in quickjs.c is suppressed by the flags below.
set "QJS_EXTRA_CFLAGS=-Wno-typedef-redefinition -Wno-error=typedef-redefinition"
set "QJS_EXTRA_CFLAGS=%QJS_EXTRA_CFLAGS% -include %COMMON%\platform\windows\quickjs_windows_compat.h"

:: C++ flags for the Windows regex wrapper.
set "CXXFLAGS=-std=c++14 -Wall"
set "CXXFLAGS=%CXXFLAGS% -I%COMMON%\platform\windows"
set "CXXFLAGS=%CXXFLAGS% -D_CRT_SECURE_NO_WARNINGS"

echo Compiling configmgr...

:: ===========================================================================
:: Phase 1: C++ regex wrapper.
::   Windows does not ship a POSIX regex library; the C++ standard library's
::   <regex> is wrapped by platform/windows/cppregex.cpp + winregex.cpp to
::   present the POSIX regcomp/regexec API expected by jsonschema.c and
::   configmgr.c (via winregex.h).
:: ===========================================================================
echo   [C++] platform\windows\cppregex.cpp
%CXX% %CXXFLAGS% -c -o "%TMP_DIR%\cppregex.o" "%COMMON%\platform\windows\cppregex.cpp"
if errorlevel 1 ( echo FAILED: cppregex.cpp & goto :cleanup_fail )

echo   [C++] platform\windows\winregex.cpp
%CXX% %CXXFLAGS% -c -o "%TMP_DIR%\winregex.o" "%COMMON%\platform\windows\winregex.cpp"
if errorlevel 1 ( echo FAILED: winregex.cpp & goto :cleanup_fail )

:: ===========================================================================
:: Phase 2: QuickJS sources (portable fork with Windows porting layer).
::   winpthread / wintime / windirent / winunistd supply POSIX-ish APIs that
::   QuickJS depends on but that are absent from the Windows CRT.
:: ===========================================================================
set "_i=0"
for %%f in (
    "%QJS%\cutils.c"
    "%QJS%\quickjs.c"
    "%QJS%\quickjs-libc.c"
    "%QJS%\libunicode.c"
    "%QJS%\libbf.c"
    "%QJS%\libregexp.c"
    "%QJS%\porting\winpthread.c"
    "%QJS%\porting\wintime.c"
    "%QJS%\porting\windirent.c"
    "%QJS%\porting\winunistd.c"
) do (
    set /a "_i=!_i!+1"
    echo   [QJS] %%~nxf
    %CC% %CFLAGS% %QJS_EXTRA_CFLAGS% @"%RSP%" -c -o "%TMP_DIR%\qjs_!_i!_%%~nf.o" %%f
    if errorlevel 1 ( echo FAILED: %%f & goto :cleanup_fail )
)

:: ===========================================================================
:: Phase 3: libyaml sources.
:: ===========================================================================
set "_i=0"
for %%f in (
    "%LIBYAML_SRC%\api.c"
    "%LIBYAML_SRC%\reader.c"
    "%LIBYAML_SRC%\scanner.c"
    "%LIBYAML_SRC%\parser.c"
    "%LIBYAML_SRC%\loader.c"
    "%LIBYAML_SRC%\writer.c"
    "%LIBYAML_SRC%\emitter.c"
    "%LIBYAML_SRC%\dumper.c"
) do (
    set /a "_i=!_i!+1"
    echo   [YAML] %%~nxf
    %CC% %CFLAGS% @"%RSP%" -c -o "%TMP_DIR%\yaml_!_i!_%%~nf.o" %%f
    if errorlevel 1 ( echo FAILED: %%f & goto :cleanup_fail )
)

:: ===========================================================================
:: Phase 4: zowe-common-c sources.
::
:: winfile.c     replaces platform/posix/posixfile.c  (Windows file I/O)
:: stub_zos_modules.c  provides no-op initialisers for the z/OS and network
::               QuickJS modules (same role as platform/posix/stub_zos_modules.c
::               on Linux/macOS).
::
:: Omitted intentionally:
::   pdsutil.c      z/OS partitioned data set access; guarded by
::                  #ifdef __ZOWE_OS_ZOS in configmgr.c - safe to exclude.
::   qjszos.c       z/OS-only QuickJS module; replaced by stub_zos_modules.c.
::   qjsnet.c       z/OS network QuickJS module; replaced by stub_zos_modules.c.
::   tls.c          GSKit TLS; not needed for core YAML / schema work.
::   http.c         HTTP server; not needed for core work.
::   httpclient.c   HTTP client; not needed for core work.
::   bpxskt.c       z/OS socket layer; not needed for core work.
::   socketmgmt.c   socket management; not needed for core work.
::   fdpoll.c       file-descriptor polling; not needed for core work.
::   jcsi.c         z/OS security; not needed for core work.
:: ===========================================================================
set "_i=0"
for %%f in (
    "%COMMON%\c\alloc.c"
    "%COMMON%\c\charsets.c"
    "%COMMON%\c\collections.c"
    "%COMMON%\c\configmgr.c"
    "%COMMON%\c\embeddedjs.c"
    "%COMMON%\c\json.c"
    "%COMMON%\c\jsonschema.c"
    "%COMMON%\c\logging.c"
    "%COMMON%\c\microjq.c"
    "%COMMON%\c\parsetools.c"
    "%COMMON%\c\timeutls.c"
    "%COMMON%\c\utils.c"
    "%COMMON%\c\xlate.c"
    "%COMMON%\c\yaml2json.c"
    "%COMMON%\platform\windows\winfile.c"
    "%COMMON%\platform\common\stub_zos_modules.c"
) do (
    set /a "_i=!_i!+1"
    echo   [SRC] %%~nxf
    %CC% %CFLAGS% @"%RSP%" -c -o "%TMP_DIR%\src_!_i!_%%~nf.o" %%f
    if errorlevel 1 ( echo FAILED: %%f & goto :cleanup_fail )
)

:: ===========================================================================
:: Phase 5: Link.
::   -lWs2_32  Winsock2 (required by the QuickJS porting layer and winfile.c)
::   -lm       math functions used by libbf and jsonschema.c
::
:: winfile.c also carries  #pragma comment(lib, "Ws2_32.lib")  which the MSVC
:: linker (lld-link) will honour automatically; the explicit -lWs2_32 here
:: ensures the flag is present even when using a different linker driver.
:: ===========================================================================
echo Linking...
set "ALL_OBJS="
for %%f in ("%TMP_DIR%\*.o") do set "ALL_OBJS=!ALL_OBJS! "%%f""

%CC% %CFLAGS% -o "%OUTPUT%" %ALL_OBJS% -lWs2_32 -lm
if errorlevel 1 ( echo Build FAILED. & goto :cleanup_fail )

echo.
echo Build successful: %OUTPUT%

:: ---------------------------------------------------------------------------
:: Remove temporary objects.
:: ---------------------------------------------------------------------------
if exist "%TMP_DIR%" rd /s /q "%TMP_DIR%"
endlocal
exit /b 0

:cleanup_fail
if exist "%TMP_DIR%" rd /s /q "%TMP_DIR%"
endlocal
exit /b 1

:: This program and the accompanying materials are
:: made available under the terms of the Eclipse Public License v2.0 which accompanies
:: this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
::
:: SPDX-License-Identifier: EPL-2.0
::
:: Copyright Contributors to the Zowe Project.
