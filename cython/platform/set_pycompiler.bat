echo off

REM Usage: set_pycompiler C:\Python37 mingw32
REM Where %PYTHON_DIR% is the directory of your Python installation.
REM Compiler option can be "mingw32" or "msvc".
REM In Pyslvs project.
set HERE=%~dp0
set PYTHON_DIR=%1
set COMPILER=%2

REM Create "distutils.cfg"
set DISTUTILS=%PYTHON_DIR%\Lib\distutils\distutils.cfg
if exist "%DISTUTILS%" del "%DISTUTILS%" /Q /S
echo [build]>> "%DISTUTILS%"
echo compiler=%COMPILER%>> "%DISTUTILS%"
echo patched file "%DISTUTILS%"
REM Apply the patch of "cygwinccompiler.py".
REM Unix "patch" command of Msys.
set patch="C:\Program Files\Git\usr\bin\patch.exe"
%patch% -N "%PYTHON_DIR%\lib\distutils\cygwinccompiler.py" "%HERE%\cygwinccompiler.diff"
%patch% -N "%PYTHON_DIR%\include\pyconfig.h" "%HERE%\pyconfig.diff"

REM Copy "vcruntime140.dll" to "libs".
copy "%PYTHON_DIR%\vcruntime140.dll" "%PYTHON_DIR%\libs"
echo copied "vcruntime140.dll".
