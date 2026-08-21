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

## 2. Sign one license

```bash
python3 Tools/generate_license.py \
  --license-id POC-001 \
  --project-id demo-project \
  --issued-at 2026-08-21T00:00:00Z \
  --subscription-valid-until 2026-09-30T23:59:59Z \
  --offline-grace-until 2026-09-05T23:59:59Z \
  --last-validation 2026-08-21T00:00:00Z \
  --output Tools/licenses/custom/license.json
```

The 15-day offline window is encoded as `offlineGraceUntil` (here 21 Aug → 5 Sep 2026). That timestamp is **signed**. The EXE does not trust an unsigned “licenseValid” flag.

## 3. Generate the sample set

```bash
python3 Tools/generate_sample_licenses.py
```

| Path | Intent |
|------|--------|
| `Tools/licenses/valid/license.json` | Correct signature, unexpired |
| `Tools/licenses/expired/license.json` | Signed, subscription in the past |
| `Tools/licenses/grace_valid/license.json` | Signed; grace still in the future (as of 2026-08-21) |
| `Tools/licenses/grace_expired/license.json` | Signed; subscription future, grace already past |
| `Tools/licenses/tampered/license.json` | Valid signature for the original payload; `subscriptionValidUntil` edited to 2035 |
| `Tools/licenses/invalid_signature/license.json` | Arbitrary Base64 signature |

Copy the chosen file to `license.json` **next to the packaged EXE**.

## 4. Check a file without Unreal

```bash
python3 Tools/verify_license.py --license Tools/licenses/valid/license.json
python3 Tools/verify_license.py --license Tools/licenses/tampered/license.json
python3 Tools/verify_license.py --now 2026-09-10T00:00:00Z --license Tools/licenses/grace_valid/license.json
```

Exit code `0` means the application would be allowed to run.

## 5. What is signed

The signature is Ed25519 over this canonical UTF-8 string (not the JSON document):

```text
licenseId=POC-001
projectId=demo-project
issuedAt=2026-08-21T00:00:00Z
subscriptionValidUntil=2026-09-30T23:59:59Z
offlineGraceUntil=2026-09-05T23:59:59Z
lastValidation=2026-08-21T00:00:00Z
```

`signature` is omitted from that string. OpenSSL equivalent:

```bash
openssl pkeyutl -sign -inkey Tools/keys/poc_ed25519_private.pem -rawin -in payload.txt -out payload.sig
```
