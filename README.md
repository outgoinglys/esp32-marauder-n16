# ESP32 Marauder N16 builds

This repository is a curated fork of [ESP32 Marauder](https://github.com/justcallmekoko/ESP32Marauder), based on v1.20.0 (commit `511bb36`). It contains a tested headless build for **ESP32-C5 DevKitC-1 N16R8** and the source changes used for its BLE console and 16 MB layout.

## Supported hardware

| Target | Status | Build settings |
| --- | --- | --- |
| ESP32-C5 DevKitC-1 N16R8 | Tested | `esp32:esp32:esp32c5:FlashSize=16M,PartitionScheme=custom,PSRAM=enabled,CDCOnBoot=default` |
| Generic ESP32-S3 N16 | Source profile only | Must be compiled and tested on the exact board before flashing |

The C5 and S3 are different chips. **Never flash the C5 binary onto an S3.** The S3 profile is intentionally not advertised as a ready-made universal binary: pinout, PSRAM mode, USB, and radio support vary between boards.

## What is included

- Full source tree, including the local BLE console changes.
- `partitions/partitions-16mb.csv`, with two OTA app slots and a large SPIFFS area.
- A verified C5 application binary in `firmware/esp32-c5-n16r8/`.
- License and attribution for the upstream MIT project.

## Flashing the verified C5 build

Use `esptool` or Arduino IDE with the matching board profile. The application binary alone is not a complete factory image; use the bootloader, partition table, and application addresses from the release manifest. Verify the SHA-256 sums before flashing.

This firmware is for authorized lab work and networks you own or have permission to test. MAC randomization and local network scans do not grant permission to bypass access controls. Deauthentication and similar radio tests can disrupt other users and should only be performed in an isolated test setup with explicit authorization.

## Building from source

The checked-in source is portable. Install Arduino-ESP32 3.3.2, select the C5 16 MB custom partition profile, and build the `esp32_marauder` sketch with `-DMARAUDER_C5`. The original upstream workflow is retained as reference in the source history; local machine paths and caches are not included.

## Publishing on GitHub

From this directory in PowerShell:

```powershell
git init
git add .
git commit -m "Add tested ESP32-C5 N16 Marauder build"
gh auth login
gh repo create esp32-marauder-n16 --public --source . --push
```

If GitHub CLI is unavailable, create an empty repository on github.com, then run:

```powershell
git remote add origin https://github.com/<your-user>/esp32-marauder-n16.git
git branch -M main
git push -u origin main
```

Keep source files in the repository. Put large firmware packages and future S3 binaries in GitHub **Releases**, with board name, flash size, commit, and SHA-256 in the release notes. Do not commit passwords, tokens, private keys, Arduino caches, or personal absolute paths.
