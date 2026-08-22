#!/usr/bin/env python3
"""Reference validator matching the Unreal CompanyLicensing plugin (OpenSSL Ed25519).

Used to sanity-check sample licenses without launching Unreal Engine.
"""

from __future__ import annotations

import argparse
import base64
import json
import subprocess
import tempfile
from datetime import datetime, timezone
from enum import Enum
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_PUBLIC = ROOT / "Tools" / "keys" / "poc_ed25519_public.pem"
EXPECTED_PROJECT_ID = "demo-project"

CANONICAL_FIELDS = (
    "licenseId",
    "projectId",
    "issuedAt",
    "subscriptionValidUntil",
    "offlineGraceUntil",
    "lastValidation",
    "machineFingerprint",
)


class LicenseState(Enum):
    LICENSE_VALID = "LICENSE_VALID"
    LICENSE_EXPIRED = "LICENSE_EXPIRED"
    LICENSE_MISSING = "LICENSE_MISSING"
    LICENSE_TAMPERED = "LICENSE_TAMPERED"
    LICENSE_INVALID = "LICENSE_INVALID"
    OFFLINE_GRACE_VALID = "OFFLINE_GRACE_VALID"
    OFFLINE_GRACE_EXPIRED = "OFFLINE_GRACE_EXPIRED"
    LICENSE_MACHINE_MISMATCH = "LICENSE_MACHINE_MISMATCH"
    LICENSE_CLOCK_ROLLBACK = "LICENSE_CLOCK_ROLLBACK"


def canonical_payload(data: dict) -> bytes:
    lines = [f"{key}={data.get(key, '')}" for key in CANONICAL_FIELDS]
    return ("\n".join(lines) + "\n").encode("utf-8")


def parse_iso(value: str) -> datetime | None:
    if not value:
        return None
    text = value.replace("Z", "+00:00")
    try:
        parsed = datetime.fromisoformat(text)
    except ValueError:
        return None
    if parsed.tzinfo is None:
        parsed = parsed.replace(tzinfo=timezone.utc)
    return parsed.astimezone(timezone.utc)


def verify_signature(payload: bytes, signature: bytes, public_pem: Path) -> bool:
    if len(signature) != 64:
        return False
    with tempfile.NamedTemporaryFile(delete=False) as payload_file:
        payload_path = Path(payload_file.name)
        payload_file.write(payload)
    with tempfile.NamedTemporaryFile(delete=False) as sig_file:
        sig_path = Path(sig_file.name)
        sig_file.write(signature)
    try:
        completed = subprocess.run(
            [
                "openssl",
                "pkeyutl",
                "-verify",
                "-pubin",
                "-inkey",
                str(public_pem),
                "-rawin",
                "-in",
                str(payload_path),
                "-sigfile",
                str(sig_path),
            ],
            capture_output=True,
            text=True,
        )
        return completed.returncode == 0
    finally:
        payload_path.unlink(missing_ok=True)
        sig_path.unlink(missing_ok=True)


def validate(path: Path | None, now: datetime, public_pem: Path, machine_fingerprint: str | None = None) -> LicenseState:
    if path is None or not path.exists():
        return LicenseState.LICENSE_MISSING

    data = json.loads(path.read_text(encoding="utf-8"))
    try:
        signature = base64.b64decode(data.get("signature", ""), validate=True)
    except Exception:
        return LicenseState.LICENSE_INVALID

    if not verify_signature(canonical_payload(data), signature, public_pem):
        return LicenseState.LICENSE_TAMPERED

    if data.get("projectId") != EXPECTED_PROJECT_ID:
        return LicenseState.LICENSE_INVALID

    lic_fingerprint = data.get("machineFingerprint", "")
    if machine_fingerprint is not None and lic_fingerprint:
        if machine_fingerprint != lic_fingerprint:
            return LicenseState.LICENSE_MACHINE_MISMATCH

    subscription_end = parse_iso(data.get("subscriptionValidUntil", ""))
    grace_end = parse_iso(data.get("offlineGraceUntil", ""))
    if subscription_end is None or grace_end is None:
        return LicenseState.LICENSE_INVALID

    if now > subscription_end:
        return LicenseState.LICENSE_EXPIRED
    if now > grace_end:
        return LicenseState.OFFLINE_GRACE_EXPIRED

    last_validation = parse_iso(data.get("lastValidation", ""))
    if last_validation is not None and last_validation < now <= grace_end:
        return LicenseState.OFFLINE_GRACE_VALID
    return LicenseState.LICENSE_VALID


def user_message(state: LicenseState) -> str:
    if state == LicenseState.OFFLINE_GRACE_EXPIRED:
        return "License validation period has expired."
    if state in (LicenseState.LICENSE_VALID, LicenseState.OFFLINE_GRACE_VALID):
        return "License Valid - Application Running"
    return "License validation failed."


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--license", type=Path)
    parser.add_argument("--public-key", type=Path, default=DEFAULT_PUBLIC)
    parser.add_argument("--now", default=None, help="ISO-8601 UTC override, e.g. 2026-09-10T00:00:00Z")
    parser.add_argument("--machine-fingerprint", default=None, help="Expected machine fingerprint for validation")
    args = parser.parse_args()

    if args.now:
        now = parse_iso(args.now)
        if now is None:
            raise SystemExit("Invalid --now")
    else:
        now = datetime.now(timezone.utc)

    state = validate(args.license, now, args.public_key, args.machine_fingerprint)
    allows = state in (LicenseState.LICENSE_VALID, LicenseState.OFFLINE_GRACE_VALID)
    print(f"state={state.value}")
    print(f"allowsRun={str(allows).lower()}")
    print(f"message={user_message(state)}")
    return 0 if allows else 1


if __name__ == "__main__":
    raise SystemExit(main())
