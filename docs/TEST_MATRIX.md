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

### How to simulate “now” for 6b

**Not available in Shipping.**

- Packaged **Development**: `CompanyLicensingPoc.exe -LicenseFakeNow=2026-09-10T00:00:00Z`
- Environment: `COMPANY_LICENSING_FAKE_NOW=2026-09-10T00:00:00Z`
- Host check: `python3 Tools/verify_license.py --now 2026-09-10T00:00:00Z --license Tools/licenses/grace_valid/license.json`

### Host-side smoke (no Unreal)

```bash
python3 Tools/verify_license.py --license Tools/licenses/valid/license.json ; echo exit:$?
python3 Tools/verify_license.py --license Tools/licenses/expired/license.json ; echo exit:$?
python3 Tools/verify_license.py --license Tools/licenses/tampered/license.json ; echo exit:$?
python3 Tools/verify_license.py --license Tools/licenses/invalid_signature/license.json ; echo exit:$?
python3 Tools/verify_license.py --now 2026-08-21T12:00:00Z --license Tools/licenses/grace_valid/license.json ; echo exit:$?
python3 Tools/verify_license.py --now 2026-09-10T00:00:00Z --license Tools/licenses/grace_valid/license.json ; echo exit:$?
python3 Tools/verify_license.py --license /tmp/does-not-exist.json ; echo exit:$?
```

Valid and grace-valid should exit `0`. All others should exit `1`.
