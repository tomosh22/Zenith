@echo off
REM ============================================================================
REM deploy_android.bat
REM
REM Stages native libraries from AGDE build output into each game's Gradle
REM jniLibs directory so that Gradle can package them into the APK.
REM
REM Usage:
REM   deploy_android.bat [debug|release] [GameName]
REM
REM Examples:
REM   deploy_android.bat debug              - Stage all games (debug)
REM   deploy_android.bat release            - Stage all games (release)
REM   deploy_android.bat debug Sokoban      - Stage Sokoban only (debug)
REM ============================================================================

setlocal enabledelayedexpansion

set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=debug

set SINGLE_GAME=%2

REM Validate build type
if /I not "%BUILD_TYPE%"=="debug" if /I not "%BUILD_TYPE%"=="release" (
	echo Error: Build type must be 'debug' or 'release'
	echo Usage: deploy_android.bat [debug^|release] [GameName]
	exit /b 1
)

REM Map build type to AGDE output directory name
if /I "%BUILD_TYPE%"=="debug" (
	set AGDE_CONFIG=arm64_v8a_vs2022_debug_agde_false
) else (
	set AGDE_CONFIG=arm64_v8a_vs2022_release_agde_false
)

set ZENITH_ROOT=%~dp0..
set GAMES_DIR=%ZENITH_ROOT%\Games
set ABI=arm64-v8a

REM Locate libc++_shared.so from the NDK
REM AGDE uses cpp_shared STL, so we need to bundle libc++_shared.so
set CPP_SHARED=
if defined ANDROID_NDK_ROOT (
	set CPP_SHARED=%ANDROID_NDK_ROOT%\toolchains\llvm\prebuilt\windows-x86_64\sysroot\usr\lib\aarch64-linux-android\libc++_shared.so
)
if defined ANDROID_NDK (
	if not defined CPP_SHARED (
		set CPP_SHARED=%ANDROID_NDK%\toolchains\llvm\prebuilt\windows-x86_64\sysroot\usr\lib\aarch64-linux-android\libc++_shared.so
	)
)

REM List of all game projects
set GAMES=Sokoban Marble Runner Combat Exploration Survival TilePuzzle AIShowcase

REM Lowercase library name lookup
set LIB_Sokoban=libsokoban.so
set LIB_Marble=libmarble.so
set LIB_Runner=librunner.so
set LIB_Combat=libcombat.so
set LIB_Exploration=libexploration.so
set LIB_Survival=libsurvival.so
set LIB_TilePuzzle=libtilepuzzle.so
set LIB_AIShowcase=libaishowcase.so

set STAGED=0
set FAILED=0

for %%G in (%GAMES%) do (
	REM Skip if single game specified and this isn't it
	if not "%SINGLE_GAME%"=="" (
		if /I not "%%G"=="%SINGLE_GAME%" (
			goto :skip_%%G
		)
	)

	set GAME_NAME=%%G
	set LIB_NAME=!LIB_%%G!
	set AGDE_OUTPUT=%GAMES_DIR%\%%G\Build\output\agde\%AGDE_CONFIG%\!LIB_NAME!
	set JNILIB_DIR=%GAMES_DIR%\%%G\Android\app\jniLibs\%ABI%

	echo.
	echo === %%G ===

	REM Check if the .so exists
	if not exist "!AGDE_OUTPUT!" (
		echo   WARNING: !AGDE_OUTPUT! not found
		echo   Build the AGDE solution first:
		echo     msbuild zenith_agde.sln /p:Configuration=%AGDE_CONFIG% /p:Platform=Android-arm64-v8a
		set /a FAILED+=1
		goto :skip_%%G
	)

	REM Create jniLibs directory
	if not exist "!JNILIB_DIR!" mkdir "!JNILIB_DIR!"

	REM Copy game .so
	echo   Copying !LIB_NAME! ...
	copy /Y "!AGDE_OUTPUT!" "!JNILIB_DIR!\!LIB_NAME!" >nul
	if errorlevel 1 (
		echo   ERROR: Failed to copy !LIB_NAME!
		set /a FAILED+=1
		goto :skip_%%G
	)

	REM Copy libc++_shared.so if found
	if defined CPP_SHARED (
		if exist "%CPP_SHARED%" (
			echo   Copying libc++_shared.so ...
			copy /Y "%CPP_SHARED%" "!JNILIB_DIR!\libc++_shared.so" >nul
		)
	)

	echo   Staged to !JNILIB_DIR!
	set /a STAGED+=1

	:skip_%%G
)

echo.
echo ============================================================================
echo Done. Staged: %STAGED%  Failed/Missing: %FAILED%
if %STAGED% gtr 0 (
	echo.
	echo Next steps:
	echo   cd Games\^<GameName^>\Android
	echo   gradlew assembleDebug
	echo   adb install -r app\build\outputs\apk\debug\app-debug.apk
)
echo ============================================================================

endlocal
