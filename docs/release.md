# Release and flashing

## Release (OTA)

1. `pio test -e native` and `pio run -e note4c` pass locally.
2. Tag and push: `git tag v1.2.3 && git push origin v1.2.3`.
3. `.github/workflows/release.yml` sets `FIRMWARE_VERSION` in `platformio.ini` to the tag, runs the native tests, builds `note4c` and publishes `firmware.bin` (exact name) on the GitHub release.
4. On the panel: press Confirm, wait for the maintenance screen to show "<tag> verfuegbar - Taste Hoch", press Up to install. The panel restarts; after the first successful `/kitchen` fetch the new image is marked valid, otherwise the bootloader rolls back.

## Flash via USB

`pio run -e note4c -t upload` (first install, or recovery when OTA is not possible).

## Restore the factory firmware

`esptool.py --chip esp32s3 write_flash 0x0 backup/note4c-factory-84c7bb4dbeac.bin`

The backup directory is gitignored and local to the original machine.
