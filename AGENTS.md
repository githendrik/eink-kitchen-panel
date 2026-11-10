# AGENTS.md

Firmware for a ZECTRIX NOTE4C (ESP32-S3) e-paper kitchen panel. See `docs/architecture.md`.

- Build: `pio run -e note4c`. Tests: `pio test -e native`. Run both before committing.
- `lib/core` is plain C++17 with no Arduino headers; hardware code lives in `src/`.
- Update `docs/architecture.md` in every change that alters behaviour.
- Repo is public: never commit WiFi passwords or tokens.
- Flash: `pio run -e note4c -t upload` (factory flash backup: `backup/`, gitignored).
