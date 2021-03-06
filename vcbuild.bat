@echo off

cd %~dp0

if /i "%1"=="help" goto help
if /i "%1"=="--help" goto help
if /i "%1"=="-help" goto help
if /i "%1"=="/help" goto help
if /i "%1"=="?" goto help
if /i "%1"=="-?" goto help
if /i "%1"=="--?" goto help
if /i "%1"=="/?" goto help

@rem Process arguments.
set config=Debug
set target=Build
set noprojgen=
set nobuild=
set test=
set test_args=
set msi=

:next-arg
if "%1"=="" goto args-done
if /i "%1"=="debug"        set config=Debug&goto arg-ok
if /i "%1"=="release"      set config=Release&goto arg-ok
if /i "%1"=="clean"        set target=Clean&goto arg-ok
if /i "%1"=="noprojgen"    set noprojgen=1&goto arg-ok
if /i "%1"=="nobuild"      set nobuild=1&goto arg-ok
if /i "%1"=="test-uv"      set test=test-uv&goto arg-ok
if /i "%1"=="test-internet"set test=test-internet&goto arg-ok
if /i "%1"=="test-pummel"  set test=test-pummel&goto arg-ok
if /i "%1"=="test-simple"  set test=test-simple&goto arg-ok
if /i "%1"=="test-message" set test=test-message&goto arg-ok
if /i "%1"=="test-all"     set test=test-all&goto arg-ok
if /i "%1"=="test"         set test=test&goto arg-ok
if /i "%1"=="msi"          set msi=1&goto arg-ok
:arg-ok
shift
goto next-arg
:args-done


:project-gen
@rem Skip project generation if requested.
if defined noprojgen goto msbuild

@rem Generate the VS project.
python tools\gyp_node -f msvs -G msvs_version=2010
if errorlevel 1 goto create-msvs-files-failed
if not exist node.sln goto create-msvs-files-failed
echo Project files generated.

:msbuild
@rem Skip project generation if requested.
if defined nobuild goto msi

@rem Bail out early if not running in VS build env.
if defined VCINSTALLDIR goto msbuild-found
if not defined VS100COMNTOOLS goto msbuild-not-found
if not exist "%VS100COMNTOOLS%\..\..\vc\vcvarsall.bat" goto msbuild-not-found
call "%VS100COMNTOOLS%\..\..\vc\vcvarsall.bat"
if not defined VCINSTALLDIR goto msbuild-not-found
goto msbuild-found

:msbuild-not-found
echo Build skipped. To build, this file needs to run from VS cmd prompt.
goto run

:msbuild-found
@rem Build the sln with msbuild.
msbuild node.sln /t:%target% /p:Configuration=%config% /clp:NoSummary;NoItemAndPropertyList;Verbosity=minimal /nologo
if errorlevel 1 goto exit

:msi
@rem Skip msi generation if not requested
if not defined msi goto run
python "%~dp0tools\msi\getnodeversion.py" < "%~dp0src\node_version.h" > "%temp%\node_version.txt"
if not errorlevel 0 echo Cannot determine current version of node.js & goto exit
for /F "tokens=*" %%i in (%temp%\node_version.txt) do set NODE_VERSION=%%i
msbuild "%~dp0tools\msi\nodemsi.sln" /t:Clean,Build /p:Configuration=%config% /p:NodeVersion=%NODE_VERSION% /clp:NoSummary;NoItemAndPropertyList;Verbosity=minimal /nologo
if errorlevel 1 goto exit

:run
@rem Run tests if requested.
if "%test%"=="" goto exit

if "%config%"=="Debug" set test_args=--mode=debug
if "%config%"=="Release" set test_args=--mode=release

if "%test%"=="test" set test_args=%test_args% simple message
if "%test%"=="test-internet" set test_args=%test_args% internet
if "%test%"=="test-pummel" set test_args=%test_args% pummel
if "%test%"=="test-simple" set test_args=%test_args% simple
if "%test%"=="test-message" set test_args=%test_args% message
if "%test%"=="test-all" set test_args=%test_args%

echo running 'python tools/test.py %test_args%'
python tools/test.py %test_args%
goto exit

:create-msvs-files-failed
echo Failed to create vc project files. 
goto exit

:help
echo vcbuild.bat [debug/release] [msi] [test-all/test-uv/test-internet/test-pummel/test-simple/test-message] [clean] [noprojgen] [nobuild]
echo Examples:
echo   vcbuild.bat                : builds debug build
echo   vcbuild.bat release msi    : builds release build and MSI installer package
echo   vcbuild.bat test           : builds debug build and runs tests
goto exit

:exit
