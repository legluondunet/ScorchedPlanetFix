#!/usr/bin/env python3
"""Version-locked Scorched Planet 3dfx fade timing patch + rollback.

Original retail SPVOODOO.EXE only, validated by SHA-256. Python stdlib.
No commercial bytes are included. Supports copy, apply with a permanent backup,
restore, and read-only status; does not launch the game or change its Wine prefix.
"""

import argparse
import hashlib
import os
from pathlib import Path
import stat
import struct
import sys
import tempfile

ORIGINAL_SHA256 = "fc1374b82b423a9836a9dda496da9960967fd75c3d98c5a0ef39d9fa8be0f80a"
FADELOW_SHA256 = "0df67600d095e18d8b45b2714236991695011158f3bf7d424fb5185e5eccae45"
GATE_VA = 0x004495CD
GATE_BEFORE = bytes.fromhex("17 b7 d1 38")  # little-endian float 0.0001
GATE_AFTER = bytes.fromhex("ac c5 27 37")  # little-endian float ~0.00001
BACKUP_SUFFIX = ".ScorchedPlanetFix-original.bak"


class PatchError(Exception):
    pass


def digest(data):
    return hashlib.sha256(data).hexdigest()


def read_regular(path):
    if path.is_symlink() or not path.is_file():
        raise PatchError(f"Missing/unsafe regular file: {path}")
    return path.read_bytes()


def pe_offset(data, address=GATE_VA):
    if len(data) < 0x100 or data[:2] != b"MZ":
        raise PatchError("Not a PE32 executable (MZ)")
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if not 0 <= pe <= len(data) - 24 or data[pe:pe + 4] != b"PE\0\0":
        raise PatchError("Invalid PE signature")
    n = struct.unpack_from("<H", data, pe + 6)[0]
    optlen = struct.unpack_from("<H", data, pe + 20)[0]
    opt = pe + 24
    if opt + optlen + n * 40 > len(data) or optlen < 32:
        raise PatchError("Truncated PE sections")
    if struct.unpack_from("<H", data, opt)[0] != 0x10B:
        raise PatchError("Expected PE32, not PE32+")
    base = struct.unpack_from("<I", data, opt + 28)[0]
    if base != 0x400000:
        raise PatchError(f"Unexpected image base {base:#x}")
    rva = address - base
    for i in range(n):
        s = opt + optlen + 40 * i
        name = data[s:s + 8].split(b"\0", 1)[0]
        vsize, srva, rawsize, rawptr = struct.unpack_from("<IIII", data, s + 8)
        if name == b".text" and srva <= rva and rva + 4 <= srva + min(vsize, rawsize):
            offset = rawptr + rva - srva
            if offset + 4 <= len(data):
                return offset
    raise PatchError(f"Target {address:#x} not backed by PE .text bytes")


def make_fadelow(data):
    if digest(data) != ORIGINAL_SHA256:
        raise PatchError("Unsupported original executable SHA-256; refusing to patch")
    offset = pe_offset(data)
    if data[offset:offset + 4] != GATE_BEFORE:
        raise PatchError("Original fade threshold signature does not match")
    changed = data[:offset] + GATE_AFTER + data[offset + 4:]
    if digest(changed) != FADELOW_SHA256:
        raise PatchError("Generated patch SHA-256 mismatch; refusing to write")
    assert len(data) == len(changed)
    assert all(data[i] == changed[i] for i in range(len(data)) if not offset <= i < offset + 4)
    return changed


def ensure_backup(backup, data, mode):
    if backup.is_symlink():
        raise PatchError(f"Backup is a symlink, refusing: {backup}")
    if backup.exists():
        if digest(read_regular(backup)) != ORIGINAL_SHA256:
            raise PatchError(f"Existing backup is not the verified original: {backup}")
        return "existing"
    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    try:
        fd = os.open(backup, flags, mode)
    except FileExistsError:
        raise PatchError(f"Backup appeared during operation: {backup}") from None
    try:
        with os.fdopen(fd, "wb") as f:
            f.write(data)
            f.flush()
            os.fsync(f.fileno())
        if digest(read_regular(backup)) != ORIGINAL_SHA256:
            raise PatchError("Backup write verification failed")
    except BaseException:
        backup.unlink(missing_ok=True)
        raise
    return "created"


def atomic_replace(path, data, mode):
    if path.is_symlink():
        raise PatchError("Refusing to replace a symlink")
    fd, name = tempfile.mkstemp(prefix=f".{path.name}.scorchedplanetfix-", dir=path.parent)
    temp = Path(name)
    try:
        with os.fdopen(fd, "wb") as f:
            os.fchmod(f.fileno(), mode)
            f.write(data)
            f.flush()
            os.fsync(f.fileno())
        if digest(read_regular(temp)) != digest(data):
            raise PatchError("Temporary output hash mismatch")
        os.replace(temp, path)
    finally:
        temp.unlink(missing_ok=True)


def make_new_copy(path, data, mode):
    if path.is_symlink():
        raise PatchError(f"Destination symlink refused: {path}")
    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    try:
        fd = os.open(path, flags, mode)
    except FileExistsError:
        raise PatchError(f"Destination exists; no overwrite allowed: {path}") from None
    try:
        with os.fdopen(fd, "wb") as f:
            f.write(data)
            f.flush()
            os.fsync(f.fileno())
        if digest(read_regular(path)) != FADELOW_SHA256:
            raise PatchError("Copy verification failed")
    except BaseException:
        path.unlink(missing_ok=True)
        raise


def perform(action, exe, output=None, confirmed=False):
    if exe.is_symlink():
        raise PatchError("Executable symlink refused")
    data = read_regular(exe)
    current = digest(data)
    backup = exe.with_name(exe.name + BACKUP_SUFFIX)
    if backup.is_symlink():
        raise PatchError("Backup symlink refused")
    backup_hash = digest(read_regular(backup)) if backup.exists() else None
    if backup_hash is not None and backup_hash != ORIGINAL_SHA256:
        raise PatchError("Backup exists but its SHA-256 does not match original")

    if action == "status":
        if current == ORIGINAL_SHA256:
            state = "ORIGINAL"
        elif current == FADELOW_SHA256:
            state = "FADELOW"
        else:
            state = "UNKNOWN/UNSUPPORTED"
        return f"Executable: {state} ({current})\nBackup: {('verified' if backup_hash else 'absent')} ({backup})"

    if action == "copy":
        if output is None:
            raise PatchError("copy requires --output")
        if output.is_symlink():
            raise PatchError("Destination symlink refused")
        if exe.resolve() == output.resolve():
            raise PatchError("Source and output must differ")
        patch = make_fadelow(data)
        make_new_copy(output, patch, stat.S_IMODE(exe.stat().st_mode))
        return f"FadeLow copy created: {output}\nSHA-256: {FADELOW_SHA256}; original unchanged"

    if action in ("apply", "restore") and not confirmed:
        raise PatchError("Writing the game executable requires explicit --yes")
    if action == "apply":
        if current == FADELOW_SHA256:
            if backup_hash != ORIGINAL_SHA256:
                raise PatchError("Already patched but verified original backup missing")
            return "Already patched with FadeLow; verified backup exists (no change)"
        patched = make_fadelow(data)
        mode = stat.S_IMODE(exe.stat().st_mode)
        result = ensure_backup(backup, data, mode)
        # Refuse an intervening write before swapping the installed file.
        if digest(read_regular(exe)) != ORIGINAL_SHA256:
            raise PatchError("Executable changed after backup; refusing replacement")
        atomic_replace(exe, patched, mode)
        if digest(read_regular(exe)) != FADELOW_SHA256:
            raise PatchError("Post-install SHA-256 mismatch; restore from backup")
        return f"FadeLow applied; backup {result}: {backup}\nRollback: restore --exe '{exe}' --yes"

    if action == "restore":
        if backup_hash != ORIGINAL_SHA256:
            raise PatchError("Cannot restore: exact verified original backup missing")
        if current == ORIGINAL_SHA256:
            return "Original already restored; backup preserved (no change)"
        if current != FADELOW_SHA256:
            raise PatchError("Unsupported current executable; refusing to overwrite")
        mode = stat.S_IMODE(exe.stat().st_mode)
        raw = read_regular(backup)
        if digest(read_regular(exe)) != FADELOW_SHA256:
            raise PatchError("Executable changed while preparing restore")
        atomic_replace(exe, raw, mode)
        if digest(read_regular(exe)) != ORIGINAL_SHA256:
            raise PatchError("Post-restore SHA-256 mismatch")
        return f"Original restored from {backup}; backup preserved for safety"
    raise PatchError(f"Unsupported action: {action}")


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("action", choices=("status", "copy", "apply", "restore"))
    p.add_argument("--exe", type=Path, default=Path("SPVOODOO.EXE"))
    p.add_argument("--output", type=Path, help="new copy location (copy only; refuses overwrite)")
    p.add_argument("--yes", action="store_true", help="required for modifying or restoring the installed executable")
    args = p.parse_args(argv)
    try:
        print(perform(args.action, args.exe, args.output, args.yes))
    except (PatchError, OSError, struct.error) as exc:
        p.exit(1, f"ERROR: {exc}\n")


if __name__ == "__main__":
    main()
