# Security Policy

## Supported Versions

Security fixes are currently provided on a best-effort basis for the latest version of Weather Station.

| Version | Supported |
|---|---|
| 1.0.x | Yes |
| Older versions | No |

## Reporting a Vulnerability

If you discover a security vulnerability, please report it privately rather than opening a public GitHub issue.

You can report security issues through GitHub's private vulnerability reporting feature, if enabled for this repository.

Please include:

- A description of the vulnerability
- Steps to reproduce the issue
- The affected version
- Potential security impact
- Any suggested fixes, if available

Reports will be reviewed as time permits. There is no guaranteed response or resolution timeframe.

Please avoid publicly disclosing vulnerabilities until they have been reviewed.

## Security Considerations

Weather Station is intended for personal use on trusted local networks.

When using this project:

- Never commit Wi-Fi passwords, API keys, or other credentials.
- Store local credentials in `secrets.h` and exclude the file using `.gitignore`.
- Do not expose the ESP32 web dashboard directly to the internet.
- Use a trusted, password-protected Wi-Fi network.
- Keep the ESP32 board package and project dependencies updated.
- Review code changes before uploading new firmware.

## Known Limitations

- The local web dashboard does not provide user authentication.
- HTTP traffic is not encrypted.
- Anyone with network access to the device may be able to view its dashboard and access exposed controls.
- The project has not undergone a formal security audit.

## Disclosure

Confirmed security issues may be addressed in future releases. Relevant fixes and security notes will be documented when appropriate.

Thank you for helping keep Weather Station secure.
