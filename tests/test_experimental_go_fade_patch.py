"""Synthetic PE tests for the copy-only fade experiment; no game binaries."""
import hashlib
import importlib.util
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools" / "experimental_go_fade_patch.py"
spec = importlib.util.spec_from_file_location("experimental_go_fade_patch", SCRIPT)
patch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(patch)


def synthetic_pe():
    data = bytearray(0x50000)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 0x3C, 0x80)
    data[0x80:0x84] = b"PE\x00\x00"
    struct.pack_into("<H", data, 0x80 + 4, 0x14C)
    struct.pack_into("<H", data, 0x80 + 6, 1)
    struct.pack_into("<H", data, 0x80 + 20, 0xE0)
    struct.pack_into("<H", data, 0x80 + 24, 0x10B)
    struct.pack_into("<I", data, 0x80 + 24 + 28, 0x400000)
    section = 0x80 + 24 + 0xE0
    data[section:section + 8] = b".text\x00\x00\x00"
    struct.pack_into("<IIII", data, section + 8, 0x49000, 0x1000, 0x49000, 0x400)
    offset = 0x400 + (patch.PATCH_VA - 0x401000)
    assert offset == 0x489CD
    data[offset:offset + 4] = patch.EXPECTED_OLD
    return bytes(data)


class TestExperimentalGate(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = synthetic_pe()
        cls.original_sha = patch.EXPECTED_SHA256
        patch.EXPECTED_SHA256 = hashlib.sha256(cls.data).hexdigest()

    @classmethod
    def tearDownClass(cls):
        patch.EXPECTED_SHA256 = cls.original_sha

    def test_pe_address_mapping(self):
        self.assertEqual(patch.pe_file_offset(self.data, patch.PATCH_VA), 0x489CD)

    def test_modes_modify_exactly_four_bytes(self):
        for mode in ("low", "zero"):
            with self.subTest(mode=mode):
                updated, offset, changed = patch.prepare(self.data, mode)
                self.assertEqual(len(updated), len(self.data))
                self.assertEqual(offset, 0x489CD)
                self.assertEqual(changed, list(range(0x489CD, 0x489D1)))
                expected = struct.unpack("<f", struct.pack("<f", patch.MODES[mode]))[0]
                self.assertEqual(struct.unpack_from("<f", updated, offset)[0], expected)

    def test_unrecognized_hash_is_rejected(self):
        altered = bytearray(self.data)
        altered[0x300] = 0x7F
        with self.assertRaisesRegex(ValueError, "Refusing unsupported executable"):
            patch.prepare(altered, "low")

    def test_wrong_pe_header_is_rejected(self):
        altered = bytearray(self.data)
        altered[0x80:0x84] = b"FAIL"
        with self.assertRaisesRegex(ValueError, "Missing PE signature"):
            patch.pe_file_offset(altered, patch.PATCH_VA)

    def test_bad_bytes_even_with_known_hash_are_rejected(self):
        altered = bytearray(self.data)
        altered[0x489CD] = 0
        patch.EXPECTED_SHA256 = hashlib.sha256(altered).hexdigest()
        try:
            with self.assertRaisesRegex(ValueError, "Gate signature mismatch"):
                patch.prepare(altered, "low")
        finally:
            patch.EXPECTED_SHA256 = hashlib.sha256(self.data).hexdigest()

    def test_cli_requires_optin_and_does_not_overwrite(self):
        with tempfile.TemporaryDirectory() as path:
            root = Path(path)
            source, output = root / "input.exe", root / "output.exe"
            source.write_bytes(self.data)
            command = [sys.executable, str(SCRIPT), str(source), "--output", str(output)]
            # Both invocations must fail; synthetic files never match the
            # embedded real-game SHA-256 in the separate CLI process.
            self.assertNotEqual(subprocess.run(command, capture_output=True).returncode, 0)
            self.assertFalse(output.exists())
            explicit = command + ["--i-accept-the-risk"]
            self.assertNotEqual(subprocess.run(explicit, capture_output=True).returncode, 0)
            self.assertFalse(output.exists())
            output.write_bytes(b"existing file")
            self.assertNotEqual(subprocess.run(explicit, capture_output=True).returncode, 0)
            self.assertEqual(output.read_bytes(), b"existing file")


if __name__ == "__main__":
    unittest.main()
