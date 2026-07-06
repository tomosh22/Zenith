@echo off
REM Thin forwarder to Build/deploy_android.ps1 (descriptor-driven staging of
REM native .so libs into each android:true game's Gradle jniLibs dir).
REM Usage: deploy_android.bat [debug|release] [GameName]
powershell -ExecutionPolicy Bypass -File "%~dp0deploy_android.ps1" %*
exit /b %ERRORLEVEL%
