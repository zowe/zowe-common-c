@echo off

:: This program and the accompanying materials are
:: made available under the terms of the Eclipse Public License v2.0 which accompanies
:: this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
::
:: SPDX-License-Identifier: EPL-2.0
::
:: Copyright Contributors to the Zowe Project.

:: Build script for configmgr on Windows.
::
:: Supports two compiler toolchains, auto-detected in this order:
::   1. MSVC  (cl.exe)   - available after running "Developer Command Prompt for
::                         VS" or vcvarsall.bat / vcvars64.bat.
::   2. LLVM  (clang)    - https://releases.llvm.org/
::
:: Override auto-detection with:
::   --compiler msvc       force MSVC (cl.exe)
::   --compiler clang      force LLVM clang / clang++
::
:: Other overrides (applied after --compiler):
::   set CC=...            override the C compiler command
::   set CXX=...           override the C++ compiler command
::
:: Prerequisites
::   - git on PATH (for cloning dependencies when not yet present)
::   - MSVC:  open a "Developer Command Prompt for VS" or call vcvars64.bat
::            before running this script so that cl.exe is on PATH.
::   - clang: install LLVM and "Desktop development with C++" from the
::            Visual Studio Installer (clang on Windows needs the MSVC CRT).
::
:: Output: bin\configmgr.exe relative to the zowe-common-c root.

setlocal enabledelayedexpansion

:: ---------------------------------------------------------------------------
:: Parse arguments
:: ---------------------------------------------------------------------------
set "FORCE_COMPILER="
:parse_args
if "%~1"=="" goto :args_done
if /i "%~1"=="--compiler" ( set "FORCE_COMPILER=%~2" & shift & shift & goto :parse_args )
if /i "%~1"=="-h"         goto :show_help
if /i "%~1"=="--help"     goto :show_help
echo Unknown argument: %~1
echo Usage: build_cmgr_windows.bat [--compiler msvc^|clang]
exit /b 1
:show_help
echo Usage: build_cmgr_windows.bat [--compiler msvc^|clang]
exit /b 0
:args_done

echo ********************************************************************************
echo Building configmgr for Windows...
echo ********************************************************************************

:: ---------------------------------------------------------------------------
:: Locate directories
:: ---------------------------------------------------------------------------
set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
pushd "%SCRIPT_DIR%\.."
set "COMMON=%CD%"
popd

:: ---------------------------------------------------------------------------
:: Load project metadata from configmgr.proj.env
:: ---------------------------------------------------------------------------
echo Loading project environment...
for /f "usebackq tokens=1* delims==" %%a in ("%SCRIPT_DIR%\configmgr.proj.env") do (
    set "_k=%%a"
    set "_v=%%b"
    set "_v=!_v:"=!"
    if "!_k!"=="PROJECT"        set "PROJECT=!_v!"
    if "!_k!"=="VERSION"        set "VERSION=!_v!"
    if "!_k!"=="QUICKJS"        set "QUICKJS=!_v!"
    if "!_k!"=="QUICKJS_SOURCE" set "QUICKJS_SOURCE=!_v!"
    if "!_k!"=="QUICKJS_BRANCH" set "QUICKJS_BRANCH=!_v!"
    if "!_k!"=="LIBYAML"        set "LIBYAML=!_v!"
    if "!_k!"=="LIBYAML_SOURCE" set "LIBYAML_SOURCE=!_v!"
    if "!_k!"=="LIBYAML_BRANCH" set "LIBYAML_BRANCH=!_v!"
)
if "%PROJECT%"=="" ( echo ERROR: PROJECT not set & exit /b 1 )
if "%VERSION%"=="" ( echo ERROR: VERSION not set  & exit /b 1 )
if "%QUICKJS%"=="" ( echo ERROR: QUICKJS not set  & exit /b 1 )
if "%LIBYAML%"=="" ( echo ERROR: LIBYAML not set  & exit /b 1 )
echo   Project : %PROJECT%  Version : %VERSION%

:: ---------------------------------------------------------------------------
:: Split VERSION into MAJOR / MINOR / PATCH
:: ---------------------------------------------------------------------------
for /f "tokens=1,2,3 delims=." %%a in ("%VERSION%") do (
    set "MAJOR=%%a" & set "MINOR=%%b" & set "PATCH=%%c"
)

:: ---------------------------------------------------------------------------
:: Dependency paths
:: ---------------------------------------------------------------------------
set "DEPS_DESTINATION=%COMMON%\deps\%PROJECT%"
set "LIBYAML_SRC=%DEPS_DESTINATION%\%LIBYAML%\src"
set "LIBYAML_INC=%DEPS_DESTINATION%\%LIBYAML%\include"
set "QJS=%DEPS_DESTINATION%\%QUICKJS%"

:: ---------------------------------------------------------------------------
:: Clone dependencies if not present
:: ---------------------------------------------------------------------------
echo ********************************************************************************
echo Checking dependencies...
if not exist "%DEPS_DESTINATION%" mkdir "%DEPS_DESTINATION%"
if not exist "%DEPS_DESTINATION%\%QUICKJS%" (
    echo Cloning QuickJS from %QUICKJS_SOURCE% @ %QUICKJS_BRANCH%...
    git clone --branch "%QUICKJS_BRANCH%" "%QUICKJS_SOURCE%" "%DEPS_DESTINATION%\%QUICKJS%"
    if errorlevel 1 ( echo ERROR: Failed to clone QuickJS. & exit /b 1 )
) else ( echo   QuickJS: OK )
if not exist "%DEPS_DESTINATION%\%LIBYAML%" (
    echo Cloning libyaml from %LIBYAML_SOURCE% @ %LIBYAML_BRANCH%...
    git clone --branch "%LIBYAML_BRANCH%" "%LIBYAML_SOURCE%" "%DEPS_DESTINATION%\%LIBYAML%"
    if errorlevel 1 ( echo ERROR: Failed to clone libyaml. & exit /b 1 )
) else ( echo   libyaml: OK )
echo Dependency setup complete

:: ---------------------------------------------------------------------------
:: Output binary
:: ---------------------------------------------------------------------------
set "OUTPUT=%COMMON%\bin\configmgr.exe"
if not exist "%COMMON%\bin" mkdir "%COMMON%\bin"
if exist "%OUTPUT%" del /f /q "%OUTPUT%"

:: ---------------------------------------------------------------------------
:: Compiler auto-detection  (MSVC preferred, clang as fallback)
:: ---------------------------------------------------------------------------
set "COMPILER_TYPE=%FORCE_COMPILER%"
if "%COMPILER_TYPE%"=="" (
    where cl >nul 2>nul && set "COMPILER_TYPE=msvc"
)
if "%COMPILER_TYPE%"=="" (
    where clang >nul 2>nul && set "COMPILER_TYPE=clang"
)
if "%COMPILER_TYPE%"=="" (
    echo ERROR: No supported compiler found.
    echo.
    echo   For MSVC:  open a "Developer Command Prompt for VS" (or run vcvars64.bat^)
    echo              then re-run this script.
    echo   For clang: install LLVM from https://releases.llvm.org/ and add it to PATH.
    exit /b 1
)

:: ---------------------------------------------------------------------------
:: Compiler-specific flag setup
:: Use goto labels instead of if/else if chains - the chained form is
:: unreliable with enabledelayedexpansion and multi-line blocks in CMD.
:: ---------------------------------------------------------------------------
if /i "%COMPILER_TYPE%"=="msvc"  goto :setup_msvc
if /i "%COMPILER_TYPE%"=="clang" goto :setup_clang
echo ERROR: Unknown compiler '%COMPILER_TYPE%'. Use --compiler msvc or --compiler clang.
exit /b 1

:setup_msvc
if "%CC%"==""  set "CC=cl"
if "%CXX%"=="" set "CXX=cl"

:: MSVC C flags.  /std:c17 for C17, /W3 standard warnings, /O2 optimise,
:: /Zi debug info.  Math functions are part of the CRT; no explicit /link
:: needed.  strdup -> _strdup for MSVC CRT compatibility.
set "BASE_CFLAGS=/nologo /W3 /O2 /Zi /std:c17"
set "BASE_CFLAGS=%BASE_CFLAGS% /D_CRT_SECURE_NO_WARNINGS /Dstrdup=_strdup"
set "BASE_CFLAGS=%BASE_CFLAGS% /DCMGRTEST=1 /DCONFIG_BIGNUM=1"
set "BASE_CFLAGS=%BASE_CFLAGS% /DYAML_VERSION_MAJOR=%MAJOR%"
set "BASE_CFLAGS=%BASE_CFLAGS% /DYAML_VERSION_MINOR=%MINOR%"
set "BASE_CFLAGS=%BASE_CFLAGS% /DYAML_VERSION_PATCH=%PATCH%"
set "BASE_CFLAGS=%BASE_CFLAGS% /DYAML_DECLARE_STATIC=1"
set "BASE_CFLAGS=%BASE_CFLAGS% /I%COMMON%\h"
set "BASE_CFLAGS=%BASE_CFLAGS% /I%COMMON%\platform\windows"
set "BASE_CFLAGS=%BASE_CFLAGS% /I%LIBYAML_INC%"
set "BASE_CFLAGS=%BASE_CFLAGS% /I%QJS%"
set "BASE_CFLAGS=%BASE_CFLAGS% /I%QJS%\porting"

:: C++ flags for the regex wrapper (/EHsc enables standard C++ exceptions).
set "CXXFLAGS=/nologo /W3 /O2 /Zi /EHsc /std:c++14"
set "CXXFLAGS=%CXXFLAGS% /D_CRT_SECURE_NO_WARNINGS"
set "CXXFLAGS=%CXXFLAGS% /I%COMMON%\platform\windows"

:: quickjs.c detects _MSC_VER and includes porting/winstdio.h which already
:: defines ssize_t via _SSIZE_T_DEFINED, so no extra compat header needed.
set "QJS_EXTRA_FLAGS="

:: Response file token prefix for string-literal /D defines.
set "RSP_D=/D"

echo Using compiler: MSVC (cl.exe)
goto :setup_done

:setup_clang
if "%CC%"==""  set "CC=clang"
if "%CXX%"=="" set "CXX=clang++"

set "BASE_CFLAGS=-std=gnu11 -Wall -Wno-unused-function -Wno-unused-variable -g -O2"
set "BASE_CFLAGS=%BASE_CFLAGS% -D_CRT_SECURE_NO_WARNINGS -Dstrdup=_strdup"
set "BASE_CFLAGS=%BASE_CFLAGS% -DCMGRTEST=1 -DCONFIG_BIGNUM=1"
set "BASE_CFLAGS=%BASE_CFLAGS% -DYAML_VERSION_MAJOR=%MAJOR%"
set "BASE_CFLAGS=%BASE_CFLAGS% -DYAML_VERSION_MINOR=%MINOR%"
set "BASE_CFLAGS=%BASE_CFLAGS% -DYAML_VERSION_PATCH=%PATCH%"
set "BASE_CFLAGS=%BASE_CFLAGS% -DYAML_DECLARE_STATIC=1"
set "BASE_CFLAGS=%BASE_CFLAGS% -I%COMMON%\h"
set "BASE_CFLAGS=%BASE_CFLAGS% -I%COMMON%\platform\windows"
set "BASE_CFLAGS=%BASE_CFLAGS% -I%LIBYAML_INC%"
set "BASE_CFLAGS=%BASE_CFLAGS% -I%QJS%"
set "BASE_CFLAGS=%BASE_CFLAGS% -I%QJS%\porting"

set "CXXFLAGS=-std=c++14 -Wall -D_CRT_SECURE_NO_WARNINGS"
set "CXXFLAGS=%CXXFLAGS% -I%COMMON%\platform\windows"

:: Pre-include the ssize_t compat header and suppress the clang diagnostic
:: for the compatible typedef redeclaration inside quickjs.c.
set "QJS_EXTRA_FLAGS=-Wno-typedef-redefinition -Wno-error=typedef-redefinition"
set "QJS_EXTRA_FLAGS=%QJS_EXTRA_FLAGS% -include %COMMON%\platform\windows\quickjs_windows_compat.h"

set "RSP_D=-D"

echo Using compiler: LLVM clang

:setup_done

echo Using CC=%CC%  CXX=%CXX%

:: ---------------------------------------------------------------------------
:: Temporary build directory
:: ---------------------------------------------------------------------------
set "TMP_DIR=%TEMP%\configmgr_build_%RANDOM%"
mkdir "%TMP_DIR%"
if errorlevel 1 ( echo ERROR: Cannot create temp dir %TMP_DIR% & exit /b 1 )

:: ---------------------------------------------------------------------------
:: Response file for string-literal defines (avoids CMD quote-escaping issues).
:: Both MSVC and clang support @<file> response files with the same syntax.
:: We use RSP_D (/D or -D) chosen above to make the flags correct for each.
:: ---------------------------------------------------------------------------
set "RSP=%TMP_DIR%\string_defs.rsp"
(
    echo %RSP_D%CONFIG_VERSION="2021-03-27"
    echo %RSP_D%YAML_VERSION_STRING="%MAJOR%.%MINOR%.%PATCH%"
) > "%RSP%"

echo Compiling configmgr...

:: ===========================================================================
:: Helper subroutines
::   compile_c   <src> <obj> <extra-flags>
::   compile_cxx <src> <obj>
:: These centralise the MSVC (/Fo:) vs clang (-c -o) output-flag difference.
:: ===========================================================================
goto :skip_helpers

:compile_c
if /i "%COMPILER_TYPE%"=="msvc" (
    %CC% /c %BASE_CFLAGS% %~3 @"%RSP%" /Fo:"%~2" "%~1"
) else (
    %CC% %BASE_CFLAGS% %~3 @"%RSP%" -c -o "%~2" "%~1"
)
exit /b %errorlevel%

:compile_cxx
if /i "%COMPILER_TYPE%"=="msvc" (
    %CXX% /c %CXXFLAGS% /Fo:"%~2" "%~1"
) else (
    %CXX% %CXXFLAGS% -c -o "%~2" "%~1"
)
exit /b %errorlevel%

:skip_helpers

:: ===========================================================================
:: Phase 1: C++ regex wrapper
::   Windows has no POSIX regex; cppregex.cpp + winregex.cpp wrap std::regex
::   to present the regcomp/regexec API that jsonschema.c expects.
:: ===========================================================================
echo   [C++] cppregex.cpp
call :compile_cxx "%COMMON%\platform\windows\cppregex.cpp" "%TMP_DIR%\cppregex.obj"
if errorlevel 1 ( echo FAILED: cppregex.cpp & goto :cleanup_fail )

echo   [C++] winregex.cpp
call :compile_cxx "%COMMON%\platform\windows\winregex.cpp" "%TMP_DIR%\winregex.obj"
if errorlevel 1 ( echo FAILED: winregex.cpp & goto :cleanup_fail )

:: ===========================================================================
:: Phase 2: QuickJS (portable fork with Windows porting layer)
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
    call :compile_c %%f "%TMP_DIR%\qjs_!_i!_%%~nf.obj" "%QJS_EXTRA_FLAGS%"
    if errorlevel 1 ( echo FAILED: %%f & goto :cleanup_fail )
)

:: ===========================================================================
:: Phase 3: libyaml
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
    call :compile_c %%f "%TMP_DIR%\yaml_!_i!_%%~nf.obj" ""
    if errorlevel 1 ( echo FAILED: %%f & goto :cleanup_fail )
)

:: ===========================================================================
:: Phase 4: zowe-common-c / configmgr sources
::
:: winfile.c           replaces posixfile.c  (Windows file I/O via Win32 API)
:: stub_zos_modules.c  no-op replacements for z/OS-only QuickJS modules
::
:: Intentionally omitted (z/OS-only or not needed for YAML/schema core):
::   pdsutil.c, qjszos.c, qjsnet.c, tls.c, http*.c, bpxskt.c,
::   socketmgmt.c, fdpoll.c, jcsi.c
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
    call :compile_c %%f "%TMP_DIR%\src_!_i!_%%~nf.obj" ""
    if errorlevel 1 ( echo FAILED: %%f & goto :cleanup_fail )
)

:: ===========================================================================
:: Phase 5: Link
:: ===========================================================================
echo Linking...
set "ALL_OBJS="
for %%f in ("%TMP_DIR%\*.obj") do set "ALL_OBJS=!ALL_OBJS! "%%f""

if /i "%COMPILER_TYPE%"=="msvc" (
    :: MSVC: drive the linker through cl.exe.  Math is part of the CRT.
    :: winfile.c has #pragma comment(lib, "Ws2_32.lib") but we pass it
    :: explicitly too for clarity.
    cl /nologo /Fe:"%OUTPUT%" %ALL_OBJS% /link Ws2_32.lib
) else (
    clang %BASE_CFLAGS% -o "%OUTPUT%" %ALL_OBJS% -lWs2_32 -lm
)
if errorlevel 1 ( echo Build FAILED. & goto :cleanup_fail )

echo.
echo Build successful: %OUTPUT%
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
