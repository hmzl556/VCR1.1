@echo off
cd %~dp0

copy    .\bin\APP.bin   ..\tools\dfu\Image\Image\APP.bin
copy    .\bin\APP.bin   ..\tools\release\APP.bin
start /b ..\tools\release\BinFileSynthesis_BR1601A01_00_V1.3.1.exe -bootloader ..\tools\dfu\Image\Image\masterboot.bin -appbin ..\tools\release\APP.bin -download ..\tools\dfu\Image\Image\BR1601A01_Download_Tool  -vision 0x1000

