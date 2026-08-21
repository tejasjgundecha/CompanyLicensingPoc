#!/usr/bin/env python3
"""Sign a license JSON file with the POC Ed25519 private key (OpenSSL).

The signature covers the canonical payload, not the raw JSON text, and excludes
the signature field.
"""

from __future__ import annotations

import argparse
import base64
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_PRIVATE = ROOT / "Tools" / "keys" / "poc_ed25519_private.pem"

CANONICAL_FIELDS = (
    "licenseId",
    "projectId",
    "issuedAt",
    "subscriptionValidUntil",
    "offlineGraceUntil",
    "lastValidation",
)


def canonical_payload(data: dict) -> bytes:
    lines = [f"{key}={data.get(key, '')}" for key in CANONICAL_FIELDS]
    return ("\n".join(lines) + "\n").encode("utf-8")


def sign(payload: bytes, private_pem: Path) -> bytes:
    with tempfile.NamedTemporaryFile(delete=False) as payload_file:
        payload_path = Path(payload_file.name)
        payload_file.write(payload)
    with tempfile.NamedTemporaryFile(delete=False) as sig_file:
        sig_path = Path(sig_file.name)
    try:
        subprocess.check_call(
            [
                "openssl",
                "pkeyutl",
                "-sign",
                "-inkey",
                str(private_pem),
                "-rawin",
                "-in",
                str(payload_path),
                "-out",
                str(sig_path),
            ]
        )
        signature = sig_path.read_bytes()
    finally:
        payload_path.unlink(missing_ok=True)
        sig_path.unlink(missing_ok=True)
    if len(signature) != 64:
        raise RuntimeError(f"Expected 64-byte Ed25519 signature, got {len(signature)}")
    return signature


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate a signed license.json")
    parser.add_argument("--private-key", type=Path, default=DEFAULT_PRIVATE)
    parser.add_argument("--license-id", required=True)
    parser.add_argument("--project-id", default="demo-project")
    parser.add_argument("--issued-at", required=True, help="ISO-8601 UTC, e.g. 2026-08-21T00:00:00Z")
    parser.add_argument("--subscription-valid-until", required=True)
    parser.add_argument("--offline-grace-until", required=True)
    parser.add_argument("--last-validation", default="")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    data = {
        "licenseId": args.license_id,
        "projectId": args.project_id,
        "issuedAt": args.issued_at,
        "subscriptionValidUntil": args.subscription_valid_until,
        "offlineGraceUntil": args.offline_grace_until,
        "lastValidation": args.last_validation,
    }
    signature = sign(canonical_payload(data), args.private_key)
    data["signature"] = base64.b64encode(signature).decode("ascii")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
