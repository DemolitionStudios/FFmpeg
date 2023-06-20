@ECHO OFF

set OLDPATH=%PATH%

call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64 > NUL:

echo export INCLUDE='%INCLUDE%'
echo.
echo export LIB='%LIB%'
echo. 
echo export LIBPATH='%LIBPATH%'
echo. 

call set NEWPATH=%%PATH:%OLDPATH%=%%
set NEWPATH=%NEWPATH:C:=/c%
set NEWPATH=%NEWPATH:\=/%
set NEWPATH=%NEWPATH:;=:%
echo export PATH="%NEWPATH%:$PATH"
