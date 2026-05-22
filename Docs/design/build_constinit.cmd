@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "C:\tmp\Zenith-engine-refactor\Docs\design"
cl /std:c++20 /c /nologo /EHsc constinit_prototype.cpp
exit /b %ERRORLEVEL%
