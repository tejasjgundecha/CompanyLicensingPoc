# Company Licensing POC

Proof of concept: **license verification compiled into a packaged Unreal Engine Windows executable**. There is no launcher, backend, API, S3, or separate licensing process.

The only question this project answers:

> Can we embed license-verification code into a packaged Unreal EXE such that the EXE refuses to run when its embedded/local license is invalid or expired?

**Yes — that is what `CompanyLicensing` does.** The plugin is a runtime module loaded at `PostConfigInit`, which is before gameplay maps and the default game mode run.

## Layout

```text
Unreal Application (packaged EXE)
├── Unreal Engine
├── CompanyLicensingPoc (game module: HUD text only)
└── CompanyLicensing (plugin, compiled in)
      ├── LicenseManager        startup load + enforce
      ├── LicenseValidator      state machine
      ├── SignatureVerifier     Ed25519 via OpenSSL
      ├── MachineFingerprint   SHA-256 hardware binding (Windows)
      ├── ClockGuard           clock rollback detection (Windows)
      └── LicenseState         enum, not booleans
```

| Path | Role |
|------|------|
| `Plugins/CompanyLicensing/` | Reusable runtime plugin |
| `Source/CompanyLicensingPoc/` | Tiny blank C++ game |
| `Tools/` | Keygen + license signing (OpenSSL). **Not shipped inside the game.** |
| `Tools/keys/poc_ed25519_private.pem` | Test **private** key. Never copied into the EXE. |
| `Tools/collect_fingerprint.py` | Collects machine fingerprint on target Windows machine |
| `Plugins/.../Private/LicensePublicKey.h` | Test **public** key compiled into the plugin |

## Validation sequence

On packaged-game startup:

1. Plugin `StartupModule` runs (`LoadingPhase: PostConfigInit`).
2. Load `license.json` from the executable directory (also checks the project dir for editor convenience).
3. **(Shipping only)** Check for **clock rollback** — compare current UTC against a persisted registry timestamp and `GetTickCount64` boot-time estimate. Fail if rollback > 5 minutes.
4. Rebuild a **canonical payload** (not the raw JSON) and verify the Ed25519 signature with the **embedded public key**.
5. Check `projectId == demo-project`.
6. **(Windows only)** If `machineFingerprint` is non-empty, compute the current machine's SHA-256 fingerprint and compare. Fail if mismatch.
7. Check `subscriptionValidUntil` against current UTC time.
8. Check `offlineGraceUntil` against current UTC time.
9. Valid → record timestamp in registry; HUD shows `License Valid - Application Running`.
10. Invalid → modal error message and `RequestExitWithStatus`.

The JSON `signature` field is **not** part of the signed data. Changing any signed field without re-signing yields `LICENSE_TAMPERED`.

### Canonical payload

UTF-8, one field per line, trailing newline:

```text
licenseId=...
projectId=...
issuedAt=...
subscriptionValidUntil=...
offlineGraceUntil=...
lastValidation=...
machineFingerprint=...
```

### License states

| State | Allows run | Typical cause |
|-------|------------|----------------|
| `LICENSE_VALID` | Yes | Signature, project, subscription, and grace all OK |
| `OFFLINE_GRACE_VALID` | Yes | Still inside signed offline window after last validation |
| `LICENSE_MISSING` | No | No `license.json` |
| `LICENSE_TAMPERED` | No | Signature does not match payload |
| `LICENSE_INVALID` | No | Bad Base64, bad dates, or wrong `projectId` |
| `LICENSE_EXPIRED` | No | `subscriptionValidUntil` in the past |
| `OFFLINE_GRACE_EXPIRED` | No | Subscription still valid but grace window ended |
| `LICENSE_MACHINE_MISMATCH` | No | Machine fingerprint does not match the signed value |
| `LICENSE_CLOCK_ROLLBACK` | No | System clock rolled back (Shipping only) |

User-facing text:

- `OFFLINE_GRACE_EXPIRED` → `License validation period has expired.`
- `LICENSE_MACHINE_MISMATCH` → `This license is not authorized for this machine.`
- `LICENSE_CLOCK_ROLLBACK` → `System clock tampering detected.`
- Every other failure → `License validation failed.`

## Cryptography

- Algorithm: **Ed25519** (OpenSSL `EVP_PKEY_ED25519` / `pkeyutl -rawin`).
- Private key exists only under `Tools/keys/` and is used by the Python utilities.
- The Unreal target links **engine OpenSSL** for verify-only.
- These keys are **POC test keys**. Rotate them before any real deployment.

## Editor vs packaged EXE

| Context | Behavior |
|---------|----------|
| Unreal Editor | Validation runs and logs; **does not quit** so you can open the project |
| Cook / commandlet | Enforcement skipped so packaging is not blocked |
| Packaged game (`Development` / `Shipping`) | Failure **exits** after a message box |

Editor-only testing is **not** the success criterion. See [docs/PACKAGING_AND_TEST.md](docs/PACKAGING_AND_TEST.md).

## Clock override (non-Shipping only)

Compiled out of Shipping (`#if !UE_BUILD_SHIPPING`):

```text
-LicenseFakeNow=2026-09-10T00:00:00Z
```

or environment variable `COMPANY_LICENSING_FAKE_NOW`. Use this to exercise the 15-day grace window without waiting. Shipping builds must use real UTC and appropriately dated signed licenses.

## Docs

1. [docs/LICENSE_GENERATION.md](docs/LICENSE_GENERATION.md) — keys and licenses
2. [docs/PACKAGING_AND_TEST.md](docs/PACKAGING_AND_TEST.md) — Windows Shipping package
3. [docs/TEST_MATRIX.md](docs/TEST_MATRIX.md) — expected results

## What this POC does not include

Backend, API, database, login, launcher, S3, networking, user management, admin UI, Electron, subscription service.
