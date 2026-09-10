echo off

set BR_path=.\dist\uart_download.exe
       
set app_bin=..\dfu\Image\Image\APP.bin
::FLASH 256KB ADDR 0x1020000
::FLASH 512KB ADDR 0x1040000
set app_start_address=0x1040000
set app_version=0x01020304
set serial_port=COM160
set serial_baud=115200  
							 
%BR_path% %app_bin% %app_start_address% %app_version% %serial_port% %serial_baud%

							 
PAUSE
EXIT							 