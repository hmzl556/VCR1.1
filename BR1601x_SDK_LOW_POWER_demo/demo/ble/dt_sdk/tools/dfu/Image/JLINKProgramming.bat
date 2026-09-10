echo off


set JLink_path=..\..\JLink\JLink_V632\JLink.exe
set JLink_script_path=..\..\JLink\JLink_Script\download.jlink
set utility_path=..\..\Util\Utility.exe

::Creating bootsetting file
::bootsetting.bin path
set output_bootsetting=.\Image\bootsetting.bin
::bank1 parameters£¬nonoptional
set bank1_start_address=0x1004000
set bank1_version=0x00000001
set bank1_bin=.\Image\APP.bin
set bank1_activation=yes
::Public key£¬optional

if not defined bank2_bin if not defined image_update_bin (
	%utility_path% bootsetting generate %output_bootsetting%                                         	^
											--bank1_start_address %bank1_start_address%                   ^
											--bank1_version %bank1_version%                               ^
											--bank1_bin %bank1_bin%                                       ^
											--bank1_activation %bank1_activation%
)

::Execute Jlink script file to program bin files
%JLink_path% -Device BR1601A01 -If SWD -Speed 4000 -commanderscript %JLink_script_path%

PAUSE
EXIT