param([string]$Port = 'COM8', [string]$Esptool = 'esptool')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$bin = "$root\firmware\esp32-c5-n16r8\app-v1.20.0-ble.bin"
& $Esptool --chip esp32c5 --port $Port write-flash 0x10000 $bin
