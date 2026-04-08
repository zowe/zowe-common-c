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
:: Prerequisites:
::   - clang on PATH  (https://releases.llvm.org/)
::   - "Desktop development with C++" workload from the VS Installer
::     (clang on Windows links against the MSVC CRT, so the SDK headers
::     and import libs must be present)
::   - git on PATH (for cloning dependencies when not yet present)
::
:: Output: bin\configmgr.exe relative to the zowe-common-c root.

setlocal enabledelayedexpansion

:: ---------------------------------------------------------------------------
:: Parse arguments
:: ---------------------------------------------------------------------------
:parse_args
if "%~1"=="" goto :args_done
if /i "%~1"=="-h"     goto :show_help
if /i "%~1"=="--help" goto :show_help
echo Unknown argument: %~1
echo Usage: build_cmgr_windows.bat
exit /b 1
:show_help
echo Usage: build_cmgr_windows.bat
exit /b 0
:args_done

echo ********************************************************************************
echo Building configmgr for Windows (clang)...
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
:: Check for clang
:: ---------------------------------------------------------------------------
if "%CC%"==""  set "CC=clang"
if "%CXX%"=="" set "CXX=clang++"
where "%CC%" >nul 2>nul
if errorlevel 1 (
    echo ERROR: %CC% not found on PATH.
    echo Install LLVM from https://releases.llvm.org/ and ensure it is on PATH.
    exit /b 1
)

:: ---------------------------------------------------------------------------
:: Compiler flags
:: ---------------------------------------------------------------------------
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

:: Suppress the compatible typedef-redefinition diagnostic that clang emits
:: when quickjs.c's own ssize_t typedef meets the one from winstdio.h.
set "QJS_EXTRA_FLAGS=-Wno-typedef-redefinition"

echo Using compiler: %CC%

:: ---------------------------------------------------------------------------
:: Temporary build directory
:: ---------------------------------------------------------------------------
set "TMP_DIR=%TEMP%\configmgr_build_%RANDOM%"
mkdir "%TMP_DIR%"
if errorlevel 1 ( echo ERROR: Cannot create temp dir %TMP_DIR% & exit /b 1 )

:: ---------------------------------------------------------------------------
:: Response file for string-literal defines (avoids CMD shell stripping quotes
:: from -D"..." arguments when passed on the command line).
:: ---------------------------------------------------------------------------
set "RSP=%TMP_DIR%\string_defs.rsp"
(
    echo -DCONFIG_VERSION="2021-03-27"
    echo -DYAML_VERSION_STRING="%MAJOR%.%MINOR%.%PATCH%"
) > "%RSP%"

echo Compiling configmgr...

:: ===========================================================================
:: Helper subroutines
::   compile_c   <src> <obj> <extra-flags>
::   compile_cxx <src> <obj>
:: ===========================================================================
goto :skip_helpers

:compile_c
%CC% %BASE_CFLAGS% %~3 @"%RSP%" -c -o "%~2" "%~1"
exit /b %errorlevel%

:compile_cxx
%CXX% %CXXFLAGS% -c -o "%~2" "%~1"
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
:: winfile.c replaces posixfile.c  (Windows file I/O via Win32 API).
:: z/OS-specific modules (qjszos, qjsnet) are ifdef'd out in embeddedjs.c
:: via __ZOWE_OS_ZOS, so no stubs are needed here.
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

%CC% -o "%OUTPUT%" %ALL_OBJS% -lWs2_32 -lm
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
