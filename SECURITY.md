# Security Policy

## Supported versions

| Version | Supported |
| ------- | --------- |
| latest `main` | yes |
| 1.x releases | yes |
| anything older | no |

## Reporting a vulnerability

Report vulnerabilities privately through GitHub's security advisories. Open the Security tab of
this repository and choose "Report a vulnerability". Please do not open a public issue for
something exploitable.

This is a spare time project with one maintainer. The honest promise is best effort. Expect an
acknowledgment within a week and a fix as fast as severity warrants.

## Scope

RawInputViewer is a local Windows desktop utility. It does not listen on a network, execute
downloaded scripts, or elevate privileges. The practical attack surface is narrow.

**Local input and device access.** The app registers for raw keyboard and mouse input and opens
HID device interface paths through `CreateFileW` to read product strings for display. It
processes only input the OS delivers to its own window. Misbehavior here is a bug, not remote
code execution. Crashes or memory corruption from malformed device notifications are still in
scope.

**Registry and settings.** Window placement and list view column layout are stored under
`HKEY_CURRENT_USER`. Paths come from this executable's version resource (`CompanyName`,
`ProductName`, and version). Unexpected registry content should fail safely rather than corrupt
memory or write outside the intended key.

**Embedded resources.** Scan code and virtual key mapping tables ship inside the executable. They
are not loaded from arbitrary user supplied paths at runtime.

**Release binaries.** Official builds come from GitHub Actions on tagged releases. If you obtain
`RawInputViewer.exe` elsewhere, verify the source. Unsigned Windows binaries may trigger
SmartScreen. That is expected for this project.

If you find input handling, registry access, or device enumeration that leads to memory
corruption, unexpected privilege elevation, or execution of attacker controlled code, that is
the report we want.

Vulnerabilities in the Windows SDK, MSVC runtime, or other components outside this repository
belong upstream. A note here is still welcome if this tool is affected.
