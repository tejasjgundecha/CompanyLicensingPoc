#!/usr/bin/env python3
"""Collect the machine fingerprint for license binding.

Run this on the target machine to obtain the SHA-256 fingerprint that must
be passed to generate_license.py --machine-fingerprint.

The fingerprint is the SHA-256 hex digest of:
    "{BIOS_UUID}|{MOTHERBOARD_SERIAL}|{MACHINE_GUID}"

Windows only.
"""

from __future__ import annotations

import hashlib
import platform
import subprocess
import sys
import winreg  # Windows only


def wmic_value(wmic_class: str, field: str) -> str:
    """Run a wmic query and return the trimmed value."""
    try:
        result = subprocess.run(
            ["wmic", "path", wmic_class, "get", field, "/value"],
            capture_output=True, text=True, timeout=10,
        )
        for line in result.stdout.splitlines():
            line = line.strip()
            if line.startswith(f"{field}="):
                return line.split("=", 1)[1].strip().upper()
    except Exception as e:
        print(f"Warning: wmic query failed for {wmic_class}.{field}: {e}", file=sys.stderr)
    return "UNKNOWN"


def machine_guid() -> str:
    """Read MachineGuid from the Windows Registry."""
    try:
        key = winreg.OpenKey(
            winreg.HKEY_LOCAL_MACHINE,
            r"SOFTWARE\Microsoft\Cryptography",
            0,
            winreg.KEY_READ | winreg.KEY_WOW64_64KEY,
        )
        value, _ = winreg.QueryValueEx(key, "MachineGuid")
        winreg.CloseKey(key)
        return value.strip()
    except Exception as e:
        print(f"Warning: failed to read MachineGuid: {e}", file=sys.stderr)
        return "UNKNOWN"


def compute_fingerprint() -> str:
    bios_uuid = wmic_value("Win32_ComputerSystemProduct", "UUID")
    mb_serial = wmic_value("Win32_BaseBoard", "SerialNumber")
    guid = machine_guid()
    raw = f"{bios_uuid}|{mb_serial}|{guid}"
    return hashlib.sha256(raw.encode("utf-8")).hexdigest()


def main() -> int:
    if platform.system() != "Windows":
        print("Error: this script is Windows-only.", file=sys.stderr)
        return 1

    fingerprint = compute_fingerprint()
    print(f"Machine fingerprint: {fingerprint}")
    print(f"\nUse with generate_license.py:")
    print(f"  --machine-fingerprint {fingerprint}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
