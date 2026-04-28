# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.3.0] - 2026-02-27

### Added

- Compress PBM with ZLIB prior to upload
- Decompress on microcontroller using miniz

### Fixed

- FPS fetching using ffprobe handles variations in output
- Only try to remove the `frame` package. Resolves issue when trying to
  remove the currently deployed firmware package caused frame update to
  fail.

## [0.2.0]

### Added

- Based on ESP-IDF v5.5.2
- Golioth v0.22.0
- Runtime WiFi Credentials
- x509 PKI Authentication
- Runtime PKI Credentials
- Frame delivery via Golioth settings service
- OTA Firmware updates via Golioth OTA service
