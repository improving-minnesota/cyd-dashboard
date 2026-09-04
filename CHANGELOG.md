# Changelog

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
