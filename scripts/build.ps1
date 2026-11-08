param(
  [ValidateSet('c5')][string]$Target = 'c5',
  [string]$ArduinoCli = 'arduino-cli'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if ($Target -eq 'c5') {
  & $ArduinoCli compile --fqbn 'esp32:esp32:esp32c5:FlashSize=16M,PartitionScheme=custom,PSRAM=enabled,CDCOnBoot=default' --build-property 'compiler.cpp.extra_flags=-DMARAUDER_C5' "$root\esp32_marauder"
}
