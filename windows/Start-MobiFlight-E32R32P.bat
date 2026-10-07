@echo off
setlocal
rem ===========================================================================
rem  Start MobiFlight Connector for the 3.2in Annunciator board (LCDWIKI E32R32P)
rem
rem  SET THIS ONCE: the COM port of each 3.2in board, separated by spaces.
rem  Find it in Device Manager under "Ports (COM & LPT)", listed as
rem  "USB-SERIAL CH340 (COMn)". Keep each board in the same USB socket: the
rem  CH340 has no serial number, so another socket means another COM number.
rem ===========================================================================
set PORTS=COM7

rem  Or pass them on the command line instead:  Start-MobiFlight-E32R32P.bat COM5 COM8
if not "%~1"=="" set PORTS=%*

rem  Why this exists. The board's USB chip shares its id with a dozen Arduino
rem  clones, and the Connector opens the port with the first matching definition
rem  it loads -- an Arduino's, which raises DTR. On this board DTR and RTS drive
rem  the ESP32's reset and boot pins, so that open puts it into its bootloader,
rem  and the Connector then lists it as a "compatible module". Setting both lines
rem  low first, here, stops that. Windows remembers it until the board is
rem  unplugged or the PC restarts, so run this every time instead of the
rem  Connector's own shortcut.
rem
rem  The 3.5in board (Caturda C3248W535) does not need this: it has native USB
rem  and no reset circuit. Do not list its port here.

set FAILED=
for %%P in (%PORTS%) do (
    mode %%P: dtr=off rts=off >nul 2>&1
    if errorlevel 1 (
        echo   %%P: could not open it. Is the board plugged in, and is %%P its port?
        set FAILED=1
    ) else (
        echo   %%P: DTR and RTS off.
    )
)

if defined FAILED (
    echo.
    echo  Fix PORTS at the top of this file, then run it again. Starting the
    echo  Connector anyway: any board that was reset into its bootloader needs
    echo  its RESET button pressed, or a replug, before it will connect.
    pause
)

start "" "%LOCALAPPDATA%\MobiFlight\MobiFlight Connector\MFConnector.exe"
