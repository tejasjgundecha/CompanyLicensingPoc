# Generating test keys and licenses

All signing happens **outside** the Unreal project with OpenSSL. The game only contains the public key.

Requires: `openssl` 1.1.1+ or 3.x, and Python 3.9+.

## 1. Generate the key pair

From the repo root:

```bash
python3 Tools/generate_keys.py
```

Creates:

| File | Purpose |
|------|---------|
| `Tools/keys/poc_ed25519_private.pem` | Signs licenses. **Do not copy into the packaged build.** |
| `Tools/keys/poc_ed25519_public.pem` | OpenSSL verification for the Python tools |
| `Tools/keys/poc_ed25519_public.der` | Intermediate SPKI encoding |
| `Plugins/CompanyLicensing/Source/CompanyLicensing/Private/LicensePublicKey.h` | 32-byte public key compiled into the plugin |

If you rotate keys, regenerate licenses and **rebuild** the Unreal project so the new public key is linked.

## 2. Collect Machine Fingerprint (Hardware Binding)

To lock a license to a specific Windows machine, obtain that machine's SHA-256 fingerprint:

### Step 2.1: Run the collector on the target Windows machine

From the target Windows computer:

```cmd
python Tools\collect_fingerprint.py
```

Output:
```text
Machine fingerprint: 4a2f89c1b7e0d3...e8a15c
Use with generate_license.py:
  --machine-fingerprint 4a2f89c1b7e0d3...e8a15c
```

### How the fingerprint is computed:
On Windows, the fingerprint is a SHA-256 hex digest combining:
1. **BIOS / System UUID** (`wmic path Win32_ComputerSystemProduct get UUID`)
2. **Motherboard Serial Number** (`wmic path Win32_BaseBoard get SerialNumber`)
3. **MachineGuid** from Windows Registry (`HKLM\SOFTWARE\Microsoft\Cryptography\MachineGuid`)

Raw format: `"{BIOS_UUID}|{MOTHERBOARD_SERIAL}|{MACHINE_GUID}"`

---

## 3. Sign a License

### Option A: Standard Floating License (No machine binding)
Leave `--machine-fingerprint` empty:

```bash
python3 Tools/generate_license.py \
  --license-id POC-001 \
  --project-id demo-project \
  --issued-at 2026-08-26T00:00:00Z \
  --subscription-valid-until 2026-12-31T23:59:59Z \
  --offline-grace-until 2026-09-15T23:59:59Z \
  --last-validation 2026-08-26T00:00:00Z \
  --output Tools/licenses/custom/license.json
```

### Option B: Machine-Bound License
Include the collected `--machine-fingerprint`:

```bash
python3 Tools/generate_license.py \
  --license-id POC-BOUND-001 \
  --project-id demo-project \
  --issued-at 2026-08-26T00:00:00Z \
  --subscription-valid-until 2026-12-31T23:59:59Z \
  --offline-grace-until 2026-09-15T23:59:59Z \
  --last-validation 2026-08-26T00:00:00Z \
  --machine-fingerprint 4a2f89c1b7e0d3...e8a15c \
  --output Tools/licenses/custom/license.json
```

The 15-day offline window is encoded as `offlineGraceUntil` (here 26 Aug → 15 Sep 2026). All timestamps and the machine fingerprint are **signed** into the payload.

---

## 4. Generate the sample test suite

```bash
python3 Tools/generate_sample_licenses.py
```

Creates:

| Path | Intent |
|------|--------|
| `Tools/licenses/valid/license.json` | Correct signature, unexpired, floating |
| `Tools/licenses/expired/license.json` | Signed, subscription in the past |
| `Tools/licenses/grace_valid/license.json` | Signed; grace still in the future |
| `Tools/licenses/grace_expired/license.json` | Signed; subscription future, grace already past |
| `Tools/licenses/machine_bound/license.json` | Signed with dummy fingerprint (fails on all real machines) |
| `Tools/licenses/tampered/license.json` | Valid signature for original payload; fields edited after signing |
| `Tools/licenses/invalid_signature/license.json` | Arbitrary Base64 signature |

Copy the chosen file to `license.json` **next to the packaged EXE**.

---

## 5. Verify a license without running Unreal

```bash
# Verify standard un-bound license:
python3 Tools/verify_license.py --license Tools/licenses/valid/license.json

# Verify with simulated machine fingerprint:
python3 Tools/verify_license.py \
  --license Tools/licenses/custom/license.json \
  --machine-fingerprint 4a2f89c1b7e0d3...e8a15c

# Verify with clock override:
python3 Tools/verify_license.py --now 2026-09-10T00:00:00Z --license Tools/licenses/grace_valid/license.json
```

Exit code `0` means the license is valid and allowed to run.

---

## 6. What is signed

The signature is Ed25519 over this canonical UTF-8 string (not the JSON document):

```text
licenseId=POC-001
projectId=demo-project
issuedAt=2026-08-26T00:00:00Z
subscriptionValidUntil=2026-12-31T23:59:59Z
offlineGraceUntil=2026-09-15T23:59:59Z
lastValidation=2026-08-26T00:00:00Z
machineFingerprint=4a2f89c1b7e0d3...e8a15c
```

`signature` is omitted from the canonical string. OpenSSL equivalent:

```bash
openssl pkeyutl -sign -inkey Tools/keys/poc_ed25519_private.pem -rawin -in payload.txt -out payload.sig
```
