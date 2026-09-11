# Changelog

## [1.14.0](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.13.1...v1.14.0) (2026-09-11)


### Features

* add ADSB.lol route source, same-airport yellow LED, and root-CA naming ([#99](https://github.com/improving-minnesota/cyd-dashboard/issues/99)) ([68c4c13](https://github.com/improving-minnesota/cyd-dashboard/commit/68c4c13bf75186cecdef3bd8ee3dc9c7cf253c07))
* add Data reset option for pool and weather temperature history ([d10451b](https://github.com/improving-minnesota/cyd-dashboard/commit/d10451b22b7011534cf2ce0ff4f57dc2b3cf21f1))
* add Data reset option for pool and weather temperature history ([1aa30c8](https://github.com/improving-minnesota/cyd-dashboard/commit/1aa30c871b035c9549148da9eee4a49ee817322f))
* add OpenSky credits screen and ground-track display ([#61](https://github.com/improving-minnesota/cyd-dashboard/issues/61)) ([c54f763](https://github.com/improving-minnesota/cyd-dashboard/commit/c54f763e689cf326e3caba5c89d73ac2cc864f8e))
* add static IP / hostname config under Settings -&gt; Network ([338e9c1](https://github.com/improving-minnesota/cyd-dashboard/commit/338e9c194ed824df15793769b8faf41429a730aa))
* add track projection, header credit tiers, and harden OpenSky TLS ([#63](https://github.com/improving-minnesota/cyd-dashboard/issues/63)) ([d1bde87](https://github.com/improving-minnesota/cyd-dashboard/commit/d1bde87af2b60d0bb89358b5d8e8eb11d3aab480))
* add weather temperature history storage and graph ([35a4951](https://github.com/improving-minnesota/cyd-dashboard/commit/35a49516dd0dbf2cf6aad79197c8e1c865d86551))
* add weather temperature history storage and graph ([28b8544](https://github.com/improving-minnesota/cyd-dashboard/commit/28b8544e73a8eeb4445141f8787f82874df351e0))
* live radar flight tracking with heading plane icons ([a168309](https://github.com/improving-minnesota/cyd-dashboard/commit/a1683097e62d4a78f775601dd937f9c2304f4836))
* live radar flight tracking with heading plane icons ([718baf1](https://github.com/improving-minnesota/cyd-dashboard/commit/718baf16ffb85bcf5cbae96ffdfbe0486eff9dff))
* move Enable timer to Flight Tracker and add Home airport setting ([3e538a8](https://github.com/improving-minnesota/cyd-dashboard/commit/3e538a8728a18ca8b29cba22d713097135126121))
* move Enable timer to Flight Tracker and add Home airport setting ([8d925ea](https://github.com/improving-minnesota/cyd-dashboard/commit/8d925eaeab29de34413b0f22b480dd085ef4cc84))
* **ota:** add firmware OTA updates from GitHub releases ([#7](https://github.com/improving-minnesota/cyd-dashboard/issues/7)) ([4ef9083](https://github.com/improving-minnesota/cyd-dashboard/commit/4ef908308c1506e51030f171530e77210ecf3db2))
* **ota:** use branch version with -dev for local builds; polish About page ([#9](https://github.com/improving-minnesota/cyd-dashboard/issues/9)) ([dad95ad](https://github.com/improving-minnesota/cyd-dashboard/commit/dad95ad74d9ee45fc3382b53b49cb91280fbb089))
* rename reset options, add Graph Data, wipe logos, sync docs ([6d15be5](https://github.com/improving-minnesota/cyd-dashboard/commit/6d15be565f3e726a5286832395113a045872862c))
* Route/track retry guards and updated LED blink logic ([#94](https://github.com/improving-minnesota/cyd-dashboard/issues/94)) ([0f247ef](https://github.com/improving-minnesota/cyd-dashboard/commit/0f247efa1f5ea13c50425bdd589a39365ef02164))
* verify TLS cert for Nominatim geocoding ([35a01b7](https://github.com/improving-minnesota/cyd-dashboard/commit/35a01b705c4ac1a16989683e729c83981dcaab75))
* verify TLS certificates for all third-party API calls ([56f840e](https://github.com/improving-minnesota/cyd-dashboard/commit/56f840ef2c3c4a35b0c20dcc4eb3c151bf0e2062))
* verify TLS certs for all third-party API calls ([14f14c3](https://github.com/improving-minnesota/cyd-dashboard/commit/14f14c3e05355e75dd2c43d27a187dabaaeae96c))


### Bug Fixes

* bound OpenSky query to radar range to avoid JSON parse errors ([e97d71d](https://github.com/improving-minnesota/cyd-dashboard/commit/e97d71db76ee059be0ad277c2701c8aff283f729))
* build and attach release firmware in one job, drop artifacts ([339e8ea](https://github.com/improving-minnesota/cyd-dashboard/commit/339e8ea8e72ac9ef41baa737e011cdf63b0f0310))
* build and attach release firmware in one job, drop artifacts ([dc7ba2a](https://github.com/improving-minnesota/cyd-dashboard/commit/dc7ba2adc1d3af079b3867b3602dd42fbe5d9650))
* **build:** make ENABLE_SERIAL_PROVISION overridable via -D flag ([487b8b8](https://github.com/improving-minnesota/cyd-dashboard/commit/487b8b8d56133e3d44eae02b780820d873539470))
* **build:** make ENABLE_SERIAL_PROVISION overridable via -D flag ([650845a](https://github.com/improving-minnesota/cyd-dashboard/commit/650845adf5586be596bd303baa55164add0a40ed))
* clarify OpenSky radar-polling credit exhaustion fallback ([38dc84c](https://github.com/improving-minnesota/cyd-dashboard/commit/38dc84c2ce0321f1e44fe6328743533860dcb94e))
* clarify OpenSky radar-polling credit exhaustion fallback ([820f641](https://github.com/improving-minnesota/cyd-dashboard/commit/820f64186ae39cd639a9cf441450455eb2bc0627))
* consolidate TLS into a single global trust store ([#67](https://github.com/improving-minnesota/cyd-dashboard/issues/67)) ([16c0b35](https://github.com/improving-minnesota/cyd-dashboard/commit/16c0b35942f9486a2be868ec31daeca65551a8c1))
* detect version.txt bumps via release existence, not head_commit.modified ([16662ff](https://github.com/improving-minnesota/cyd-dashboard/commit/16662ff58aef33c7ab306072e679a118e10fd838))
* detect version.txt bumps via release existence, not head_commit.modified ([c963201](https://github.com/improving-minnesota/cyd-dashboard/commit/c9632018990ef2355a48786e6a6130d65d236464))
* Don't leave "Update: Scanning..." stuck on screen ([#32](https://github.com/improving-minnesota/cyd-dashboard/issues/32)) ([d524547](https://github.com/improving-minnesota/cyd-dashboard/commit/d52454724415d1bbfd1c9fa2f379a4343b87430b))
* give release jobs repo context for gh release commands ([cd4bd1f](https://github.com/improving-minnesota/cyd-dashboard/commit/cd4bd1f3f20b424a5726545a4d0d1050003c8085))
* give release jobs repo context for gh release commands ([7d9247a](https://github.com/improving-minnesota/cyd-dashboard/commit/7d9247ad87b3015d803ff021bb7c87264ba6f8b3))
* harden OTA security and fix release-image rollback crash ([#11](https://github.com/improving-minnesota/cyd-dashboard/issues/11)) ([082e856](https://github.com/improving-minnesota/cyd-dashboard/commit/082e856bad24a63bd692f3d5c6674bc192f6c3f0))
* harden TLS and reduce long-run heap pressure ([15119fb](https://github.com/improving-minnesota/cyd-dashboard/commit/15119fb83ecc44817a85f402d498769d1993c4d0))
* harden TLS handling and radar off-screen tracking ([#57](https://github.com/improving-minnesota/cyd-dashboard/issues/57)) ([7727678](https://github.com/improving-minnesota/cyd-dashboard/commit/772767806b58f83ae198f13d2ded785752fcf923))
* hold LED 5 s and auto-return non-dashboard screens after 2 minutes ([#102](https://github.com/improving-minnesota/cyd-dashboard/issues/102)) ([dc88bed](https://github.com/improving-minnesota/cyd-dashboard/commit/dc88bed25a0c5405fa3d7ac4dad7bc2439616fe7))
* keep HTTP headers across TLS retries and report invalid creds per provider ([#59](https://github.com/improving-minnesota/cyd-dashboard/issues/59)) ([bfd9e8c](https://github.com/improving-minnesota/cyd-dashboard/commit/bfd9e8c5376e008b4cc8ab6ac9a2098bc8d99454))
* mark release-please PR as published after release ([3e995eb](https://github.com/improving-minnesota/cyd-dashboard/commit/3e995eb0490cbf87685e702029e487ccb8bef2a1))
* only show build number when BUILD_NUM is non-zero ([9b3b402](https://github.com/improving-minnesota/cyd-dashboard/commit/9b3b402a9cfe7c19de58a0336a0efd7f3f329e92))
* **ota:** allow a -dev build to upgrade to the release of the same version ([#22](https://github.com/improving-minnesota/cyd-dashboard/issues/22)) ([14c3c6d](https://github.com/improving-minnesota/cyd-dashboard/commit/14c3c6d397cb090086de7d1f674628a45e214ac3))
* **ota:** draw the progress bar incrementally to stop flicker ([#24](https://github.com/improving-minnesota/cyd-dashboard/issues/24)) ([1765617](https://github.com/improving-minnesota/cyd-dashboard/commit/1765617861ce66ebfc155404d2d0c70a00aad18b))
* **ota:** run OTA on a dedicated large-stack task to fix TLS stack overflow ([#13](https://github.com/improving-minnesota/cyd-dashboard/issues/13)) ([9128263](https://github.com/improving-minnesota/cyd-dashboard/commit/9128263828c7a7b3014c55d5882ee176a824df77))
* **ota:** stop esp_task_wdt_reset spam from the OTA task ([#15](https://github.com/improving-minnesota/cyd-dashboard/issues/15)) ([3c10b4d](https://github.com/improving-minnesota/cyd-dashboard/commit/3c10b4d9ff65ff4717bbbc35bb44006937453010))
* pause net fetches during OTA and don't persist Auto-Update off for dev builds ([#28](https://github.com/improving-minnesota/cyd-dashboard/issues/28)) ([45f44e5](https://github.com/improving-minnesota/cyd-dashboard/commit/45f44e5bf49fc6966956cb2e6753d2a5428b1126))
* **pool:** clear stale temp when switching Govee devices ([#1](https://github.com/improving-minnesota/cyd-dashboard/issues/1)) ([5cec4e6](https://github.com/improving-minnesota/cyd-dashboard/commit/5cec4e6e97466a7236b061d79147d2978900147c))
* Prevent TFT/SPI race that froze the device when an OTA started ([#30](https://github.com/improving-minnesota/cyd-dashboard/issues/30)) ([613d846](https://github.com/improving-minnesota/cyd-dashboard/commit/613d846c3b0b54e1358c30e513513fc4b6d71592))
* restore heap headroom so OTA and TLS validation work reliably ([#65](https://github.com/improving-minnesota/cyd-dashboard/issues/65)) ([f7417b9](https://github.com/improving-minnesota/cyd-dashboard/commit/f7417b9aa659cbbc5831c1dff1e050e93968b441))
* Set route/track credits to 0 on 429 and document ([#96](https://github.com/improving-minnesota/cyd-dashboard/issues/96)) ([66e3ee3](https://github.com/improving-minnesota/cyd-dashboard/commit/66e3ee310a22446d9226574bb27a92ea6c39a458))
* **settings:** sync Sleep Mode display strings from NVS at boot ([c260794](https://github.com/improving-minnesota/cyd-dashboard/commit/c260794f91f6197e424500efa85a4e2be29ed670))
* **settings:** sync Sleep Mode display strings from NVS at boot ([cacfa85](https://github.com/improving-minnesota/cyd-dashboard/commit/cacfa851b177ecb0e4f96ac387a0395272e94b8f))
* show 0 aircraft status after successful OpenSky states:null response ([8ea7665](https://github.com/improving-minnesota/cyd-dashboard/commit/8ea7665b29aee11b891dc61c64fef0e7f69d8ab1))
* show 0 aircraft status after successful OpenSky states:null response ([725fdb4](https://github.com/improving-minnesota/cyd-dashboard/commit/725fdb419896b8ca6bc797d17099eeca6ad094f1))
* **sleep:** don't rely on sntp_get_sync_status(), which never completes ([08217d1](https://github.com/improving-minnesota/cyd-dashboard/commit/08217d1d6e8d1f549f02e30627ba1c7f0ebbcca9))
* **sleep:** don't sleep while an OTA is pending or running ([7774fa4](https://github.com/improving-minnesota/cyd-dashboard/commit/7774fa49c1fda4f068a8d7126830d742330d5cab))
* **sleep:** don't sleep while an OTA is pending or running ([e13ad15](https://github.com/improving-minnesota/cyd-dashboard/commit/e13ad157248513851f86500253f27f6fb29f99d4))
* **sleep:** don't trust a stale wakeup-cause register on non-sleep boots ([#18](https://github.com/improving-minnesota/cyd-dashboard/issues/18)) ([9fbe4ea](https://github.com/improving-minnesota/cyd-dashboard/commit/9fbe4eaf1552a52b635da5b189015476e60db575))
* **sleep:** gate sleep window on timezone applied this boot ([#55](https://github.com/improving-minnesota/cyd-dashboard/issues/55)) ([f43ae1d](https://github.com/improving-minnesota/cyd-dashboard/commit/f43ae1d7f63e09409601543bce79a2e1a08e1ecd))
* **sleep:** keep device awake for the full wake duration on touch-wake ([2231065](https://github.com/improving-minnesota/cyd-dashboard/commit/223106505b94a3dd86d72f6dcba81e873f996fb1))
* **sleep:** keep device awake for the full wake duration on touch-wake ([87e2e39](https://github.com/improving-minnesota/cyd-dashboard/commit/87e2e3947d6dd8fa21996bbd35d67c2c9f285865))
* **sleep:** only trust the clock once NTP has synced ([#26](https://github.com/improving-minnesota/cyd-dashboard/issues/26)) ([916464b](https://github.com/improving-minnesota/cyd-dashboard/commit/916464bee30868bb3aa9e14792ac3d2620170f5a))
* **sleep:** release the touch CS GPIO hold on wake so deep sleep actually sticks ([bea120b](https://github.com/improving-minnesota/cyd-dashboard/commit/bea120bf72487a2dfa533c7bd7b640b817d830e2))
* **sleep:** release the touch CS GPIO hold on wake so deep sleep actually sticks ([88ad6bf](https://github.com/improving-minnesota/cyd-dashboard/commit/88ad6bfcd773e9566bccc860797f5d7a75828a4a))
* **sleep:** stop gating sleep on sntp_get_sync_status(), which never completes ([6dbad89](https://github.com/improving-minnesota/cyd-dashboard/commit/6dbad89d62a1087df80b1823b82f4539d6d4a266))
* stream JSON responses to reduce TLS heap fragmentation ([1201c53](https://github.com/improving-minnesota/cyd-dashboard/commit/1201c53822b0560db55b03e2152643aada74e0b6))
* stream JSON responses to reduce TLS heap fragmentation ([4ffb7e7](https://github.com/improving-minnesota/cyd-dashboard/commit/4ffb7e728573e2d13ebf23348ec89462d5d09ec1))
* **ui:** use black text on yellow anonymous-warning border ([e515223](https://github.com/improving-minnesota/cyd-dashboard/commit/e515223890a533e7c0eb9bb048eaa831f7ed472e))
* **ui:** use black text on yellow anonymous-warning border ([1be7a47](https://github.com/improving-minnesota/cyd-dashboard/commit/1be7a474f93f61a3db212cdc7b5f7acacc2bc745))
* **version:** stop the dev-build version string from going stale ([#20](https://github.com/improving-minnesota/cyd-dashboard/issues/20)) ([2c75903](https://github.com/improving-minnesota/cyd-dashboard/commit/2c75903030cd9cedbe1202f5825ae8e172ceef67))

## [1.13.1](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.13.0...v1.13.1) (2026-09-11)


### Bug Fixes

* hold LED 5 s and auto-return non-dashboard screens after 2 minutes ([#102](https://github.com/improving-minnesota/cyd-dashboard/issues/102)) ([dc88bed](https://github.com/improving-minnesota/cyd-dashboard/commit/dc88bed25a0c5405fa3d7ac4dad7bc2439616fe7))

## [1.13.0](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.12.1...v1.13.0) (2026-09-11)


### Features

* add ADSB.lol route source, same-airport yellow LED, and root-CA naming ([#99](https://github.com/improving-minnesota/cyd-dashboard/issues/99)) ([68c4c13](https://github.com/improving-minnesota/cyd-dashboard/commit/68c4c13bf75186cecdef3bd8ee3dc9c7cf253c07))

## [1.12.1](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.12.0...v1.12.1) (2026-09-10)


### Bug Fixes

* Set route/track credits to 0 on 429 and document ([#96](https://github.com/improving-minnesota/cyd-dashboard/issues/96)) ([66e3ee3](https://github.com/improving-minnesota/cyd-dashboard/commit/66e3ee310a22446d9226574bb27a92ea6c39a458))

## [1.12.0](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.11.1...v1.12.0) (2026-09-10)


### Features

* Route/track retry guards and updated LED blink logic ([#94](https://github.com/improving-minnesota/cyd-dashboard/issues/94)) ([0f247ef](https://github.com/improving-minnesota/cyd-dashboard/commit/0f247efa1f5ea13c50425bdd589a39365ef02164))

## [1.11.1](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.11.0...v1.11.1) (2026-09-10)


### Bug Fixes

* mark release-please PR as published after release ([3e995eb](https://github.com/improving-minnesota/cyd-dashboard/commit/3e995eb0490cbf87685e702029e487ccb8bef2a1))

## [1.10.0](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.9.7...v1.10.0) (2026-09-10)


### Features

* add Data reset option for pool and weather temperature history ([d10451b](https://github.com/improving-minnesota/cyd-dashboard/commit/d10451b22b7011534cf2ce0ff4f57dc2b3cf21f1))
* add Data reset option for pool and weather temperature history ([1aa30c8](https://github.com/improving-minnesota/cyd-dashboard/commit/1aa30c871b035c9549148da9eee4a49ee817322f))
* rename reset options, add Graph Data, wipe logos, sync docs ([6d15be5](https://github.com/improving-minnesota/cyd-dashboard/commit/6d15be565f3e726a5286832395113a045872862c))


### Bug Fixes

* detect version.txt bumps via release existence, not head_commit.modified ([16662ff](https://github.com/improving-minnesota/cyd-dashboard/commit/16662ff58aef33c7ab306072e679a118e10fd838))
* detect version.txt bumps via release existence, not head_commit.modified ([c963201](https://github.com/improving-minnesota/cyd-dashboard/commit/c9632018990ef2355a48786e6a6130d65d236464))

## [1.9.7](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.9.6...v1.9.7) (2026-09-10)


### Bug Fixes

* show 0 aircraft status after successful OpenSky states:null response ([8ea7665](https://github.com/improving-minnesota/cyd-dashboard/commit/8ea7665b29aee11b891dc61c64fef0e7f69d8ab1))
* show 0 aircraft status after successful OpenSky states:null response ([725fdb4](https://github.com/improving-minnesota/cyd-dashboard/commit/725fdb419896b8ca6bc797d17099eeca6ad094f1))

## [1.9.6](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.9.5...v1.9.6) (2026-09-10)


### Bug Fixes

* only show build number when BUILD_NUM is non-zero ([9b3b402](https://github.com/improving-minnesota/cyd-dashboard/commit/9b3b402a9cfe7c19de58a0336a0efd7f3f329e92))

## [1.9.5](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.9.4...v1.9.5) (2026-09-10)


### Bug Fixes

* harden TLS and reduce long-run heap pressure ([15119fb](https://github.com/improving-minnesota/cyd-dashboard/commit/15119fb83ecc44817a85f402d498769d1993c4d0))

## [1.9.4](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.9.3...v1.9.4) (2026-09-06)


### Bug Fixes

* stream JSON responses to reduce TLS heap fragmentation ([1201c53](https://github.com/improving-minnesota/cyd-dashboard/commit/1201c53822b0560db55b03e2152643aada74e0b6))
* stream JSON responses to reduce TLS heap fragmentation ([4ffb7e7](https://github.com/improving-minnesota/cyd-dashboard/commit/4ffb7e728573e2d13ebf23348ec89462d5d09ec1))

## [1.9.3](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.9.2...v1.9.3) (2026-09-06)


### Bug Fixes

* clarify OpenSky radar-polling credit exhaustion fallback ([38dc84c](https://github.com/improving-minnesota/cyd-dashboard/commit/38dc84c2ce0321f1e44fe6328743533860dcb94e))
* clarify OpenSky radar-polling credit exhaustion fallback ([820f641](https://github.com/improving-minnesota/cyd-dashboard/commit/820f64186ae39cd639a9cf441450455eb2bc0627))

## [1.9.2](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.9.1...v1.9.2) (2026-09-06)


### Bug Fixes

* consolidate TLS into a single global trust store ([#67](https://github.com/improving-minnesota/cyd-dashboard/issues/67)) ([16c0b35](https://github.com/improving-minnesota/cyd-dashboard/commit/16c0b35942f9486a2be868ec31daeca65551a8c1))

## [1.9.1](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.9.0...v1.9.1) (2026-09-06)


### Bug Fixes

* restore heap headroom so OTA and TLS validation work reliably ([#65](https://github.com/improving-minnesota/cyd-dashboard/issues/65)) ([f7417b9](https://github.com/improving-minnesota/cyd-dashboard/commit/f7417b9aa659cbbc5831c1dff1e050e93968b441))

## [1.9.0](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.8.0...v1.9.0) (2026-09-06)


### Features

* add track projection, header credit tiers, and harden OpenSky TLS ([#63](https://github.com/improving-minnesota/cyd-dashboard/issues/63)) ([d1bde87](https://github.com/improving-minnesota/cyd-dashboard/commit/d1bde87af2b60d0bb89358b5d8e8eb11d3aab480))

## [1.8.0](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.7.3...v1.8.0) (2026-09-05)


### Features

* add OpenSky credits screen and ground-track display ([#61](https://github.com/improving-minnesota/cyd-dashboard/issues/61)) ([c54f763](https://github.com/improving-minnesota/cyd-dashboard/commit/c54f763e689cf326e3caba5c89d73ac2cc864f8e))

## [1.7.3](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.7.2...v1.7.3) (2026-09-05)


### Bug Fixes

* keep HTTP headers across TLS retries and report invalid creds per provider ([#59](https://github.com/improving-minnesota/cyd-dashboard/issues/59)) ([bfd9e8c](https://github.com/improving-minnesota/cyd-dashboard/commit/bfd9e8c5376e008b4cc8ab6ac9a2098bc8d99454))

## [1.7.2](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.7.1...v1.7.2) (2026-09-05)


### Bug Fixes

* harden TLS handling and radar off-screen tracking ([#57](https://github.com/improving-minnesota/cyd-dashboard/issues/57)) ([7727678](https://github.com/improving-minnesota/cyd-dashboard/commit/772767806b58f83ae198f13d2ded785752fcf923))

## [1.7.1](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.7.0...v1.7.1) (2026-09-04)


### Bug Fixes

* **sleep:** gate sleep window on timezone applied this boot ([#55](https://github.com/improving-minnesota/cyd-dashboard/issues/55)) ([f43ae1d](https://github.com/improving-minnesota/cyd-dashboard/commit/f43ae1d7f63e09409601543bce79a2e1a08e1ecd))

## [1.7.0](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.6.0...v1.7.0) (2026-09-04)


### Features

* live radar flight tracking with heading plane icons ([a168309](https://github.com/improving-minnesota/cyd-dashboard/commit/a1683097e62d4a78f775601dd937f9c2304f4836))
* live radar flight tracking with heading plane icons ([718baf1](https://github.com/improving-minnesota/cyd-dashboard/commit/718baf16ffb85bcf5cbae96ffdfbe0486eff9dff))


### Bug Fixes

* bound OpenSky query to radar range to avoid JSON parse errors ([e97d71d](https://github.com/improving-minnesota/cyd-dashboard/commit/e97d71db76ee059be0ad277c2701c8aff283f729))

## [1.6.0](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.5.0...v1.6.0) (2026-09-04)


### Features

* add weather temperature history storage and graph ([35a4951](https://github.com/improving-minnesota/cyd-dashboard/commit/35a49516dd0dbf2cf6aad79197c8e1c865d86551))
* add weather temperature history storage and graph ([28b8544](https://github.com/improving-minnesota/cyd-dashboard/commit/28b8544e73a8eeb4445141f8787f82874df351e0))

## [1.5.0](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.4.0...v1.5.0) (2026-09-04)


### Features

* verify TLS cert for Nominatim geocoding ([35a01b7](https://github.com/improving-minnesota/cyd-dashboard/commit/35a01b705c4ac1a16989683e729c83981dcaab75))
* verify TLS certificates for all third-party API calls ([56f840e](https://github.com/improving-minnesota/cyd-dashboard/commit/56f840ef2c3c4a35b0c20dcc4eb3c151bf0e2062))
* verify TLS certs for all third-party API calls ([14f14c3](https://github.com/improving-minnesota/cyd-dashboard/commit/14f14c3e05355e75dd2c43d27a187dabaaeae96c))

## [1.4.0](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.3.9...v1.4.0) (2026-09-04)


### Features

* move Enable timer to Flight Tracker and add Home airport setting ([3e538a8](https://github.com/improving-minnesota/cyd-dashboard/commit/3e538a8728a18ca8b29cba22d713097135126121))
* move Enable timer to Flight Tracker and add Home airport setting ([8d925ea](https://github.com/improving-minnesota/cyd-dashboard/commit/8d925eaeab29de34413b0f22b480dd085ef4cc84))

## [1.3.9](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.3.8...v1.3.9) (2026-09-04)


### Bug Fixes

* **ui:** use black text on yellow anonymous-warning border ([e515223](https://github.com/improving-minnesota/cyd-dashboard/commit/e515223890a533e7c0eb9bb048eaa831f7ed472e))
* **ui:** use black text on yellow anonymous-warning border ([1be7a47](https://github.com/improving-minnesota/cyd-dashboard/commit/1be7a474f93f61a3db212cdc7b5f7acacc2bc745))

## [1.3.8](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.3.7...v1.3.8) (2026-09-04)


### Bug Fixes

* **settings:** sync Sleep Mode display strings from NVS at boot ([c260794](https://github.com/improving-minnesota/cyd-dashboard/commit/c260794f91f6197e424500efa85a4e2be29ed670))
* **settings:** sync Sleep Mode display strings from NVS at boot ([cacfa85](https://github.com/improving-minnesota/cyd-dashboard/commit/cacfa851b177ecb0e4f96ac387a0395272e94b8f))

## [1.3.7](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.3.6...v1.3.7) (2026-09-04)


### Bug Fixes

* **sleep:** don't rely on sntp_get_sync_status(), which never completes ([08217d1](https://github.com/improving-minnesota/cyd-dashboard/commit/08217d1d6e8d1f549f02e30627ba1c7f0ebbcca9))
* **sleep:** stop gating sleep on sntp_get_sync_status(), which never completes ([6dbad89](https://github.com/improving-minnesota/cyd-dashboard/commit/6dbad89d62a1087df80b1823b82f4539d6d4a266))

## [1.3.6](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.3.5...v1.3.6) (2026-09-04)


### Bug Fixes

* **sleep:** release the touch CS GPIO hold on wake so deep sleep actually sticks ([bea120b](https://github.com/improving-minnesota/cyd-dashboard/commit/bea120bf72487a2dfa533c7bd7b640b817d830e2))
* **sleep:** release the touch CS GPIO hold on wake so deep sleep actually sticks ([88ad6bf](https://github.com/improving-minnesota/cyd-dashboard/commit/88ad6bfcd773e9566bccc860797f5d7a75828a4a))

## [1.3.5](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.3.4...v1.3.5) (2026-09-04)


### Bug Fixes

* **sleep:** don't sleep while an OTA is pending or running ([7774fa4](https://github.com/improving-minnesota/cyd-dashboard/commit/7774fa49c1fda4f068a8d7126830d742330d5cab))
* **sleep:** don't sleep while an OTA is pending or running ([e13ad15](https://github.com/improving-minnesota/cyd-dashboard/commit/e13ad157248513851f86500253f27f6fb29f99d4))

## [1.3.4](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.3.3...v1.3.4) (2026-09-04)


### Bug Fixes

* **build:** make ENABLE_SERIAL_PROVISION overridable via -D flag ([487b8b8](https://github.com/improving-minnesota/cyd-dashboard/commit/487b8b8d56133e3d44eae02b780820d873539470))
* **build:** make ENABLE_SERIAL_PROVISION overridable via -D flag ([650845a](https://github.com/improving-minnesota/cyd-dashboard/commit/650845adf5586be596bd303baa55164add0a40ed))
* **sleep:** keep device awake for the full wake duration on touch-wake ([2231065](https://github.com/improving-minnesota/cyd-dashboard/commit/223106505b94a3dd86d72f6dcba81e873f996fb1))
* **sleep:** keep device awake for the full wake duration on touch-wake ([87e2e39](https://github.com/improving-minnesota/cyd-dashboard/commit/87e2e3947d6dd8fa21996bbd35d67c2c9f285865))

## [1.3.3](https://github.com/improving-minnesota/cyd-dashboard/compare/v1.3.2...v1.3.3) (2026-09-04)


### Bug Fixes

* Don't leave "Update: Scanning..." stuck on screen ([#32](https://github.com/improving-minnesota/cyd-dashboard/issues/32)) ([d524547](https://github.com/improving-minnesota/cyd-dashboard/commit/d52454724415d1bbfd1c9fa2f379a4343b87430b))

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
