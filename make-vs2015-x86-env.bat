@ECHO OFF

set OLDPATH=%PATH%

call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x86 > NUL:

echo export INCLUDE='%INCLUDE%'
echo export LIB='%LIB%'
echo export LIBPATH='%LIBPATH%'

call set NEWPATH=%%PATH:%OLDPATH%=%%
set NEWPATH=%NEWPATH:C:=/c%
set NEWPATH=%NEWPATH:\=/%
set NEWPATH=%NEWPATH:;=:%
echo export PATH="%NEWPATH%:$PATH"
