# Test matrix

Dates below assume “today” is **2026-08-21** UTC. If you run this later, regenerate samples with `python3 Tools/generate_sample_licenses.py` or use `--now` / `-LicenseFakeNow` on non-Shipping builds.

Place each `license.json` next to the packaged EXE. Expected UI is a Windows message box unless the app is allowed to run.

| # | Setup | Internal state | User message | EXE |
|---|--------|----------------|--------------|-----|
| 1 | `Tools/licenses/valid/license.json` | `LICENSE_VALID` | `License Valid - Application Running` (on-screen HUD) | Starts |
| 2 | Delete `license.json` | `LICENSE_MISSING` | `License validation failed.` | Refuses |
| 3 | `Tools/licenses/expired/license.json` | `LICENSE_EXPIRED` | `License validation failed.` | Refuses |
| 4 | `Tools/licenses/tampered/license.json` (`subscriptionValidUntil` set to `2035-12-31T23:59:59Z`, signature unchanged) | `LICENSE_TAMPERED` | `License validation failed.` | Refuses |
| 5 | `Tools/licenses/invalid_signature/license.json` | `LICENSE_TAMPERED` | `License validation failed.` | Refuses |
| 6a | `Tools/licenses/grace_valid/license.json` with now = `2026-08-21` | `OFFLINE_GRACE_VALID` | HUD success text | Starts |
| 6b | Same file, now = `2026-09-10T00:00:00Z` (after `offlineGraceUntil` `2026-09-05`) | `OFFLINE_GRACE_EXPIRED` | `License validation period has expired.` | Refuses |
| 6c | Shipping: `Tools/licenses/grace_expired/license.json` (real clock on 2026-08-21) | `OFFLINE_GRACE_EXPIRED` | `License validation period has expired.` | Refuses |
| 7 | `Tools/licenses/machine_bound/license.json` on a different machine (fingerprint won't match) | `LICENSE_MACHINE_MISMATCH` | `This license is not authorized for this machine.` | Refuses |
| 8 | Roll system clock back by 1+ hours on a Shipping build (after at least one successful validation has recorded a timestamp) | `LICENSE_CLOCK_ROLLBACK` | `System clock tampering detected.` | Refuses |
| 9 | Roll system clock back by < 5 minutes (NTP drift range) | `LICENSE_VALID` | HUD success text | Starts |

### How to simulate "now" for 6b

**Not available in Shipping.**

- Packaged **Development**: `CompanyLicensingPoc.exe -LicenseFakeNow=2026-09-10T00:00:00Z`
- Environment: `COMPANY_LICENSING_FAKE_NOW=2026-09-10T00:00:00Z`
- Host check: `python3 Tools/verify_license.py --now 2026-09-10T00:00:00Z --license Tools/licenses/grace_valid/license.json`

### Testing machine fingerprint mismatch (test 7)

1. Collect fingerprint on machine A: `python3 Tools/collect_fingerprint.py`
2. Generate a license bound to machine A: `python3 Tools/generate_license.py --machine-fingerprint <hash-from-step-1> ...`
3. Copy the license to machine B and run — should get `LICENSE_MACHINE_MISMATCH`
4. The pre-built `Tools/licenses/machine_bound/license.json` uses a dummy fingerprint (`a1b2c3...`) so it will fail on any real machine

### Testing clock rollback (tests 8–9)

Clock rollback detection is **Shipping-only** (non-Shipping builds support fake clocks).

1. Package a Shipping build and run once with a valid license → the plugin records the current time in `HKCU\Software\CompanyLicensing\LastSeenUtc`
2. Roll the system clock back by 1 hour → re-launch → should get `LICENSE_CLOCK_ROLLBACK`
3. Roll the clock back by only 2 minutes → re-launch → should still start (within 5-minute tolerance)
4. To reset the clock guard state, delete the registry key: `reg delete HKCU\Software\CompanyLicensing /f`

### Host-side smoke (no Unreal)

```bash
python3 Tools/verify_license.py --license Tools/licenses/valid/license.json ; echo exit:$?
python3 Tools/verify_license.py --license Tools/licenses/expired/license.json ; echo exit:$?
python3 Tools/verify_license.py --license Tools/licenses/tampered/license.json ; echo exit:$?
python3 Tools/verify_license.py --license Tools/licenses/invalid_signature/license.json ; echo exit:$?
python3 Tools/verify_license.py --now 2026-08-21T12:00:00Z --license Tools/licenses/grace_valid/license.json ; echo exit:$?
python3 Tools/verify_license.py --now 2026-09-10T00:00:00Z --license Tools/licenses/grace_valid/license.json ; echo exit:$?
python3 Tools/verify_license.py --license /tmp/does-not-exist.json ; echo exit:$?
python3 Tools/verify_license.py --machine-fingerprint wrong --license Tools/licenses/machine_bound/license.json ; echo exit:$?
```

Valid and grace-valid should exit `0`. All others should exit `1`.
