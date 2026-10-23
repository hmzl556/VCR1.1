param([string]$Keil = 'D:\MDK5.36\UV4\UV4.exe')
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$project = Join-Path $root 'bleuart\prj\ble_dtss.uvprojx'
$log = Join-Path $root 'bleuart\prj\build_final.log'
if (!(Test-Path -LiteralPath $Keil)) { throw "Keil executable missing: $Keil" }
$started = Get-Date
$process = Start-Process -FilePath $Keil -ArgumentList @('-r',('"'+$project+'"'),'-j0','-o',('"'+$log+'"')) -WindowStyle Hidden -PassThru -Wait
$buildText = [IO.File]::ReadAllText($log, [Text.Encoding]::GetEncoding(936))
if ($buildText -notmatch '0 Error\(s\),') { throw "Build failed; inspect $log (exit $($process.ExitCode))" }
$map = [IO.File]::ReadAllText((Join-Path $root 'bleuart\prj\Listings\ble.map'))
foreach ($region in @(@('ER_IROM1',0x01036000), @('RW_IRAM0',0x20004000), @('RW_IRAM1',0x2000BBE0), @('MCU_STACK',0x2000C000))) {
    $pattern = 'Execution Region '+$region[0]+' \(Exec base: 0x([0-9a-fA-F]+), Load base: 0x[0-9a-fA-F]+, Size: 0x([0-9a-fA-F]+),'
    if ($map -notmatch $pattern) { throw "Missing region in linker map: $($region[0])" }
    $end = [Convert]::ToUInt32($Matches[1],16) + [Convert]::ToUInt32($Matches[2],16)
    if ($end -gt $region[1]) { throw "Region overlaps reserved boundary: $($region[0])" }
    '{0} end: 0x{1:X8}' -f $region[0], $end
}
if ($map -notmatch 'Execution Region MCU_STACK \(Exec base: 0x2000bc00, Load base: 0x[0-9a-fA-F]+, Size: 0x00000400,') { throw 'Unexpected main/ISR stack placement' }
$binary = Get-Item -LiteralPath (Join-Path $root 'ble.bin')
if ($binary.LastWriteTime -lt $started.AddSeconds(-2) -or $binary.Length -gt 0x32000) { throw 'Missing/stale/oversized APP1 binary' }
$hash = Get-FileHash -Algorithm SHA256 -LiteralPath $binary.FullName
[IO.File]::WriteAllText((Join-Path $root 'ble.bin.sha256'), ($hash.Hash.ToLower()+'  ble.bin'+[Environment]::NewLine))
($buildText -split "`n" | Where-Object { $_ -match 'Program Size:|Error\(s\)|Build Time' })
"ble.bin: $($binary.Length) bytes; SHA256 $($hash.Hash)"
