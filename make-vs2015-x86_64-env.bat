@ECHO OFF

set SNAPPY_DIR=C:\Users\lev\projects\UnityHapPlugin\hapunityplugin\3rdparty\snappy-windows-1.1.1.8

set OLDPATH=%PATH%

call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" amd64 > NUL:

echo export INCLUDE='%INCLUDE%:%SNAPPY_DIR%\include'
echo export LIB='%LIB%:%SNAPPY_DIR%\native'
echo export LIBPATH='%LIBPATH%'

call set NEWPATH=%%PATH:%OLDPATH%=%%
set NEWPATH=%NEWPATH:C:=/c%
set NEWPATH=%NEWPATH:\=/%
set NEWPATH=%NEWPATH:;=:%
echo export PATH="%NEWPATH%:$PATH"
