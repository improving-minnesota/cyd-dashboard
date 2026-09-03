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
