@echo off

if /I "%~1"=="" echo Please *DRAG* your RAW files and/or folders into this BAT script!

set /a CLIPPING=1
set /a HISTOGRAM=0

PATH %~dp0;%PATH%

for %%a in (%*) do (
    if exist %%a (
        if exist %%a\ (
            cd %%a
            call :process *.ARW *.CR2 *.CR3 *.NEF *.PEF *.RAW *.DNG
            cd ..
        ) else (
            call :process "%%~a"
        )
    )
)
<nul set /p=Press a key or close this window...& pause > nul
exit 0

:process
for %%f in (%*) do (
    if %CLIPPING% equ 1 if not exist %%~nf_clip.tiff (set mkc=true) else (set mkc=)
    if %HISTOGRAM% equ 1 if not exist %%~nf_hist.csv (set mkh=true) else (set mkh=)
    set process=
    if defined mkc set process=true
    if defined mkh set process=true
    if defined process (
        <nul set /p=%%~nxf: 
        dcraw -D -4 -j -t 0 %%f || exit /B
        if defined mkh (hraw histogram -i %%~nf.pgm > %%~nf_hist.csv || del %%~nf_hist.csv)
        if defined mkc (hraw clipping -i %%~nf.pgm -o %%~nf_clip.tiff -v) else (echo done)
        del %%~nf.pgm
    )
)
exit /B
