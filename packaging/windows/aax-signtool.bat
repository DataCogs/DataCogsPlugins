@echo off
setlocal
rem Signtool wrapper for wraptool + Azure Artifact Signing, from PACE's
rem "Azure Digital Signing" tutorial (adapted from the KoalaDSP guide).
rem wraptool constructs its signtool call around the legacy certificate-
rem thumbprint model; this wrapper keeps only the target binary path and
rem re-invokes the real signtool with the Artifact Signing arguments.
rem Requires env: SIGNTOOL_PATH, ACS_DLIB, ACS_JSON (set by
rem build-installer.ps1 / CI).

set "here=%~dp0"
echo %*>"%here%wraptool-args.tmp"
python "%here%aax-signtool.py" || exit /b 1
set /p target=<"%here%wraptool-args.tmp"

"%SIGNTOOL_PATH%" sign /v /fd SHA256 /tr "http://timestamp.acs.microsoft.com" /td SHA256 /dlib "%ACS_DLIB%" /dmdf "%ACS_JSON%" "%target%"
exit /b %errorlevel%
