# Changelog

## [1.3.2](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.3.1...v1.3.2) (2026-09-04)


### Bug Fixes

* Prevent TFT/SPI race that froze the device when an OTA started ([#30](https://github.com/improving-minnesota/cyd-dashboard/issues/30)) ([613d846](https://github.com/improving-minnesota/cyd-dashboard/commit/613d846c3b0b54e1358c30e513513fc4b6d71592))

## [1.3.1](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.3.0...v1.3.1) (2026-09-04)


### Bug Fixes

* pause net fetches during OTA and don't persist Auto-Update off for dev builds ([#28](https://github.com/improving-minnesota/cyd-dashboard/issues/28)) ([45f44e5](https://github.com/improving-minnesota/cyd-dashboard/commit/45f44e5bf49fc6966956cb2e6753d2a5428b1126))

## [1.3.0](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.2.7...v1.3.0) (2026-09-04)


### Features

* **ota:** add firmware OTA updates from GitHub releases ([#7](https://github.com/improving-minnesota/cyd-dashboard/issues/7)) ([4ef9083](https://github.com/improving-minnesota/cyd-dashboard/commit/4ef908308c1506e51030f171530e77210ecf3db2))
* **ota:** use branch version with -dev for local builds; polish About page ([#9](https://github.com/improving-minnesota/cyd-dashboard/issues/9)) ([dad95ad](https://github.com/improving-minnesota/cyd-dashboard/commit/dad95ad74d9ee45fc3382b53b49cb91280fbb089))


### Bug Fixes

* harden OTA security and fix release-image rollback crash ([#11](https://github.com/improving-minnesota/cyd-dashboard/issues/11)) ([082e856](https://github.com/improving-minnesota/cyd-dashboard/commit/082e856bad24a63bd692f3d5c6674bc192f6c3f0))
* **ota:** allow a -dev build to upgrade to the release of the same version ([#22](https://github.com/improving-minnesota/cyd-dashboard/issues/22)) ([14c3c6d](https://github.com/improving-minnesota/cyd-dashboard/commit/14c3c6d397cb090086de7d1f674628a45e214ac3))
* **ota:** draw the progress bar incrementally to stop flicker ([#24](https://github.com/improving-minnesota/cyd-dashboard/issues/24)) ([1765617](https://github.com/improving-minnesota/cyd-dashboard/commit/1765617861ce66ebfc155404d2d0c70a00aad18b))
* **ota:** run OTA on a dedicated large-stack task to fix TLS stack overflow ([#13](https://github.com/improving-minnesota/cyd-dashboard/issues/13)) ([9128263](https://github.com/improving-minnesota/cyd-dashboard/commit/9128263828c7a7b3014c55d5882ee176a824df77))
* **ota:** stop esp_task_wdt_reset spam from the OTA task ([#15](https://github.com/improving-minnesota/cyd-dashboard/issues/15)) ([3c10b4d](https://github.com/improving-minnesota/cyd-dashboard/commit/3c10b4d9ff65ff4717bbbc35bb44006937453010))
* **pool:** clear stale temp when switching Govee devices ([#1](https://github.com/improving-minnesota/cyd-dashboard/issues/1)) ([5cec4e6](https://github.com/improving-minnesota/cyd-dashboard/commit/5cec4e6e97466a7236b061d79147d2978900147c))
* **sleep:** don't trust a stale wakeup-cause register on non-sleep boots ([#18](https://github.com/improving-minnesota/cyd-dashboard/issues/18)) ([9fbe4ea](https://github.com/improving-minnesota/cyd-dashboard/commit/9fbe4eaf1552a52b635da5b189015476e60db575))
* **sleep:** only trust the clock once NTP has synced ([#26](https://github.com/improving-minnesota/cyd-dashboard/issues/26)) ([916464b](https://github.com/improving-minnesota/cyd-dashboard/commit/916464bee30868bb3aa9e14792ac3d2620170f5a))
* **version:** stop the dev-build version string from going stale ([#20](https://github.com/improving-minnesota/cyd-dashboard/issues/20)) ([2c75903](https://github.com/improving-minnesota/cyd-dashboard/commit/2c75903030cd9cedbe1202f5825ae8e172ceef67))

## [1.2.7](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.2.6...v1.2.7) (2026-09-04)


### Bug Fixes

* **ota:** draw the progress bar incrementally to stop flicker ([#24](https://github.com/improving-minnesota/cyd-dashboard/issues/24)) ([1765617](https://github.com/improving-minnesota/cyd-dashboard/commit/1765617861ce66ebfc155404d2d0c70a00aad18b))

## [1.2.6](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.2.5...v1.2.6) (2026-09-04)


### Bug Fixes

* **ota:** allow a -dev build to upgrade to the release of the same version ([#22](https://github.com/improving-minnesota/cyd-dashboard/issues/22)) ([14c3c6d](https://github.com/improving-minnesota/cyd-dashboard/commit/14c3c6d397cb090086de7d1f674628a45e214ac3))

## [1.2.5](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.2.4...v1.2.5) (2026-09-04)


### Bug Fixes

* **version:** stop the dev-build version string from going stale ([#20](https://github.com/improving-minnesota/cyd-dashboard/issues/20)) ([2c75903](https://github.com/improving-minnesota/cyd-dashboard/commit/2c75903030cd9cedbe1202f5825ae8e172ceef67))

## [1.2.4](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.2.3...v1.2.4) (2026-09-04)


### Bug Fixes

* **sleep:** don't trust a stale wakeup-cause register on non-sleep boots ([#18](https://github.com/improving-minnesota/cyd-dashboard/issues/18)) ([9fbe4ea](https://github.com/improving-minnesota/cyd-dashboard/commit/9fbe4eaf1552a52b635da5b189015476e60db575))

## [1.2.3](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.2.2...v1.2.3) (2026-09-04)


### Bug Fixes

* **ota:** stop esp_task_wdt_reset spam from the OTA task ([#15](https://github.com/improving-minnesota/cyd-dashboard/issues/15)) ([3c10b4d](https://github.com/improving-minnesota/cyd-dashboard/commit/3c10b4d9ff65ff4717bbbc35bb44006937453010))

## [1.2.2](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.2.1...v1.2.2) (2026-09-04)


### Bug Fixes

* **ota:** run OTA on a dedicated large-stack task to fix TLS stack overflow ([#13](https://github.com/improving-minnesota/cyd-dashboard/issues/13)) ([9128263](https://github.com/improving-minnesota/cyd-dashboard/commit/9128263828c7a7b3014c55d5882ee176a824df77))

## [1.2.1](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.2.0...v1.2.1) (2026-09-04)


### Bug Fixes

* harden OTA security and fix release-image rollback crash ([#11](https://github.com/improving-minnesota/cyd-dashboard/issues/11)) ([082e856](https://github.com/improving-minnesota/cyd-dashboard/commit/082e856bad24a63bd692f3d5c6674bc192f6c3f0))

## [1.2.0](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.1.0...v1.2.0) (2026-09-03)


### Features

* **ota:** use branch version with -dev for local builds; polish About page ([#9](https://github.com/improving-minnesota/cyd-dashboard/issues/9)) ([dad95ad](https://github.com/improving-minnesota/cyd-dashboard/commit/dad95ad74d9ee45fc3382b53b49cb91280fbb089))

## [1.1.0](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.0.1...v1.1.0) (2026-09-03)


### Features

* **ota:** add firmware OTA updates from GitHub releases ([#7](https://github.com/improving-minnesota/cyd-dashboard/issues/7)) ([4ef9083](https://github.com/improving-minnesota/cyd-dashboard/commit/4ef908308c1506e51030f171530e77210ecf3db2))

## [1.0.1](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.0.0...v1.0.1) (2026-09-03)


### Bug Fixes

* **pool:** clear stale temp when switching Govee devices ([#1](https://github.com/improving-minnesota/cyd-dashboard/issues/1)) ([5cec4e6](https://github.com/improving-minnesota/cyd-dashboard/commit/5cec4e6e97466a7236b061d79147d2978900147c))

## 1.0.0 (2026-09-03)


### Bug Fixes

* **pool:** clear stale temp when switching Govee devices ([#1](https://github.com/improving-minnesota/cyd-dashboard/issues/1)) ([5cec4e6](https://github.com/improving-minnesota/cyd-dashboard/commit/5cec4e6e97466a7236b061d79147d2978900147c))
