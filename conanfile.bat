@echo off
title test conan
echo Here, i only test the connection of all header files, and can't output them correctly¡­
if exist amalgamate\crow_all.h del amalgamate\crow_all.h;
for /f "tokens=2 skip=1 delims= " %%i in (.\include\crow.h) do (
	type .\include\crow\%%~nxi >> amalgamate\crow_all.h
)
pause