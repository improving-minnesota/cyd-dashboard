# Security Policy

## Supported versions

The latest release on `main` is supported. Older releases may not receive
security fixes; always update to the newest firmware.

## Reporting a vulnerability

**Do not open a public issue** for a security vulnerability. Please report it
privately via one of:

- **GitHub private vulnerability reporting** — use the "Report a vulnerability"
  button under the repo's **Security** tab (if enabled for this repository), or
- **Email** the maintainer, Paul Hassinger:
  `paul.hassinger (at) improving.com`.

Please include:

- The firmware version (**Settings → About**) and how to reproduce the issue.
- The impact, if known (e.g. is it exploitable remotely via WiFi?).

You'll receive an acknowledgment within a few days and an update on the fix and
release timeline. Security issues are handled before general bug reports.

## Scope

This is an ESP32 hobby/display device. It connects to external APIs (OpenSky,
Govee, weather) over WiFi. Things that matter here: insecure storage of
credentials on the device, unsafe handling of network data, and anything that
could let an attacker execute arbitrary code on the device or exfiltrate its
stored credentials.

## Firmware update (OTA) security

Firmware is delivered over HTTPS from this repository's GitHub releases and is
authenticated at two independent layers:

- **Transport (TLS certificate verification).** Both the release-metadata
  request and the firmware download are verified against a pinned root-CA
  bundle embedded in the firmware (`kGithubRootCAs` in `ota.ino`), covering
  github.com/api.github.com and objects.githubusercontent.com. The device never
  retries an unverified connection after a certificate-validation failure, so a
  man-in-the-middle cannot force it to skip verification.
- **Content integrity (SHA-256).** The streamed firmware image is hashed and
  compared against the asset digest published by the GitHub API before it is
  flashed. A mismatch aborts the update without touching the running slot.

**The single exception — expired CA certificates.** A firmware build only falls
back to skipping certificate validation when its bundled root certificates have
passed their expiry (`OTA_CA_EXPIRY`). This exists so that a device running
stale firmware that predates a GitHub certificate rotation can still update. In
practice this means only firmware whose embedded CA bundle is already expired
runs without certificate verification; current firmware keeps a valid bundle
and always verifies.

> **Residual risk.** During that fallback window the download is not
> certificate-verified, and because the asset digest is fetched over the same
> unauthenticated channel it provides no protection there either. The safe
> mitigation is to keep the device on current firmware so its CA bundle stays
> valid. Code-signing the firmware image would close this gap entirely and is
> the recommended hardening path if that fallback window is unacceptable.
