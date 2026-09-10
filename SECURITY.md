# Security Policy

## Supported Versions

| Version | Supported |
|---------|-----------|
| latest  | Yes       |

## Reporting a Vulnerability

This plugin runs inside the OBS process, so a bug here is a bug in OBS.

For non-sensitive issues, open a regular GitHub issue. If you believe you have found a security vulnerability that should be disclosed privately, please [contact](https://www.nathanv.com/contact) the maintainer directly.

Please include:

- A description of the issue
- Steps to reproduce
- Potential impact
- A suggested fix (if you have one)

You should receive a response within 72 hours.

## Known Limitations

- **Runs with OBS's privileges.** Every OBS plugin is native code loaded into the OBS process; there is no sandbox. Only install builds from the GitHub releases page (each release lists SHA-256 checksums) or builds you compiled yourself.
- **macOS builds are ad-hoc signed and not notarized.** Release builds come from GitHub Actions without a Developer ID, so Gatekeeper warns on the `.pkg` and you have to open it deliberately. The signature says nothing about who built it; the SHA-256 checksums on the release do. Windows and Linux builds carry no signature at all.
- **The Windows installer needs admin.** It writes to `C:\ProgramData\obs-studio\plugins\`, the machine-wide OBS plugin folder, and registers an uninstaller. It offers a per-user prompt if you decline elevation.
- **Reads the profile's recording path.** The dock reads the configured recording folder from the OBS profile to report free disk space. It reads nothing else, writes nothing to disk, and changes no OBS or Aitum Vertical settings.
- **No phoning home.** This project does not collect analytics or metrics and does not call home in any way. It opens no network connections.
