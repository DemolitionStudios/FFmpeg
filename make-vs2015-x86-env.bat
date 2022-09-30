@ECHO OFF

set SNAPPY_DIR=D:\projects\DemolitionStudios\hapunityplugin\3rdparty\snappy-windows-1.1.9

set OLDPATH=%PATH%

call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x86 > NUL:

echo export INCLUDE='%INCLUDE%:%SNAPPY_DIR%\include'
echo.
echo export LIB='%LIB%;%SNAPPY_DIR%\native'
echo.
echo export LIBPATH='%LIBPATH%'
echo.

call set NEWPATH=%%PATH:%OLDPATH%=%%
set NEWPATH=%NEWPATH:C:=/c%
set NEWPATH=%NEWPATH:\=/%
set NEWPATH=%NEWPATH:;=:%
echo export PATH="%NEWPATH%:$PATH"
