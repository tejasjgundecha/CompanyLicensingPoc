#!/usr/bin/env python3
"""Create the sample licenses used by the POC test matrix."""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GEN = ROOT / "Tools" / "generate_license.py"
OUT = ROOT / "Tools" / "licenses"


def generate(**kwargs: str) -> Path:
    output = Path(kwargs.pop("output"))
    cmd = [sys.executable, str(GEN)]
    for key, value in kwargs.items():
        cmd.extend([f"--{key.replace('_', '-')}", value])
    cmd.extend(["--output", str(output)])
    subprocess.check_call(cmd)
    return output


def main() -> int:
    valid = generate(
        license_id="POC-001",
        issued_at="2026-08-21T00:00:00Z",
        subscription_valid_until="2026-09-30T23:59:59Z",
        offline_grace_until="2026-09-05T23:59:59Z",
        last_validation="",
        output=str(OUT / "valid" / "license.json"),
    )

    generate(
        license_id="POC-EXPIRED",
        issued_at="2026-01-01T00:00:00Z",
        subscription_valid_until="2026-01-15T23:59:59Z",
        offline_grace_until="2026-01-20T23:59:59Z",
        last_validation="2026-01-01T00:00:00Z",
        output=str(OUT / "expired" / "license.json"),
    )

    generate(
        license_id="POC-GRACE-VALID",
        issued_at="2026-08-21T00:00:00Z",
        subscription_valid_until="2026-12-31T23:59:59Z",
        offline_grace_until="2026-09-05T23:59:59Z",
        last_validation="2026-08-21T00:00:00Z",
        output=str(OUT / "grace_valid" / "license.json"),
    )

    generate(
        license_id="POC-GRACE-EXPIRED",
        issued_at="2026-07-01T00:00:00Z",
        subscription_valid_until="2026-12-31T23:59:59Z",
        offline_grace_until="2026-08-01T23:59:59Z",
        last_validation="2026-07-01T00:00:00Z",
        output=str(OUT / "grace_expired" / "license.json"),
    )

    valid_data = json.loads(valid.read_text(encoding="utf-8"))

    tampered = dict(valid_data)
    tampered["subscriptionValidUntil"] = "2035-12-31T23:59:59Z"
    tampered_path = OUT / "tampered" / "license.json"
    tampered_path.parent.mkdir(parents=True, exist_ok=True)
    tampered_path.write_text(json.dumps(tampered, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote {tampered_path} (payload changed, signature unchanged)")

    invalid_sig = dict(valid_data)
    invalid_sig["signature"] = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=="
    invalid_path = OUT / "invalid_signature" / "license.json"
    invalid_path.parent.mkdir(parents=True, exist_ok=True)
    invalid_path.write_text(json.dumps(invalid_sig, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote {invalid_path}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
