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


def get_smbios_info() -> tuple[str, str]:
    """Extract BIOS UUID and Motherboard Serial in-process via GetSystemFirmwareTable ('RSMB')."""
    bios_uuid = "UNKNOWN"
    mb_serial = "UNKNOWN"
    try:
        import ctypes
        from ctypes import wintypes
        kernel32 = ctypes.windll.kernel32
        GetSystemFirmwareTable = kernel32.GetSystemFirmwareTable
        GetSystemFirmwareTable.restype = wintypes.UINT
        GetSystemFirmwareTable.argtypes = [wintypes.DWORD, wintypes.DWORD, wintypes.c_void_p, wintypes.DWORD]

        # 'RSMB' in DWORD little endian is 0x52534D42
        rsmb_sig = 0x52534D42
        size = GetSystemFirmwareTable(rsmb_sig, 0, None, 0)
        if size == 0:
            return bios_uuid, mb_serial

        buf = ctypes.create_string_buffer(size)
        read_bytes = GetSystemFirmwareTable(rsmb_sig, 0, buf, size)
        if read_bytes == 0:
            return bios_uuid, mb_serial

        data = buf.raw
        # Parse SMBIOS raw table data starting after 8-byte header
        table_len = int.from_bytes(data[4:8], "little")
        offset = 8
        limit = min(offset + table_len, len(data))

        while offset + 4 <= limit:
            hdr_type = data[offset]
            hdr_len = data[offset + 1]
            if hdr_len < 4 or offset + hdr_len > limit:
                break

            # Type 1: System Information (UUID at offset 8, length 16)
            if hdr_type == 1 and hdr_len >= 0x19 and bios_uuid == "UNKNOWN":
                uuid_bytes = data[offset + 8 : offset + 24]
                if uuid_bytes != b"\x00" * 16 and uuid_bytes != b"\xFF" * 16:
                    d1 = int.from_bytes(uuid_bytes[0:4], "little")
                    d2 = int.from_bytes(uuid_bytes[4:6], "little")
                    d3 = int.from_bytes(uuid_bytes[6:8], "little")
                    bios_uuid = f"{d1:08X}-{d2:04X}-{d3:04X}-" + "".join(f"{b:02X}" for b in uuid_bytes[8:10]) + "-" + "".join(f"{b:02X}" for b in uuid_bytes[10:16])

            # Type 2: BaseBoard Information (SerialNumber string index at offset 7)
            if hdr_type == 2 and hdr_len >= 8 and mb_serial == "UNKNOWN":
                serial_idx = data[offset + 7]
                if serial_idx > 0:
                    str_area = data[offset + hdr_len :]
                    strings = [s.decode("latin-1", errors="ignore").strip() for s in str_area.split(b"\x00")]
                    if 0 < serial_idx <= len(strings) and strings[serial_idx - 1]:
                        mb_serial = strings[serial_idx - 1].upper()

            # Advance past strings area (terminated by \x00\x00)
            end_strings = data.find(b"\x00\x00", offset + hdr_len)
            if end_strings == -1:
                break
            offset = end_strings + 2
            if hdr_type == 127:
                break
    except Exception as e:
        print(f"Warning: in-process SMBIOS query failed: {e}", file=sys.stderr)

    return bios_uuid, mb_serial


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
    bios_uuid, mb_serial = get_smbios_info()
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
