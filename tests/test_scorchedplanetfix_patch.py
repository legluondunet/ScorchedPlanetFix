"""Unit tests with synthetic PE fixtures, no commercial game bytes."""
import hashlib
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

HERE = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location(
    "scorchedplanetfix_patch", HERE / "tools" / "scorchedplanetfix_patch.py"
)
patch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(patch)


def synthetic_pe():
    raw = bytearray(0x50000)
    raw[:2] = b"MZ"
    struct.pack_into("<I", raw, 0x3C, 0x80)
    raw[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<H", raw, 0x84, 0x14C)
    struct.pack_into("<H", raw, 0x86, 1)
    struct.pack_into("<H", raw, 0x94, 0xE0)
    struct.pack_into("<H", raw, 0x98, 0x10B)
    struct.pack_into("<I", raw, 0x80 + 24 + 28, 0x400000)
    sect = 0x80 + 24 + 0xE0
    raw[sect:sect + 8] = b".text\0\0\0"
    struct.pack_into("<IIII", raw, sect + 8, 0x49000, 0x1000, 0x49000, 0x400)
    raw[0x489CD:0x489D1] = patch.GATE_BEFORE
    return bytes(raw)


class TestProductionPatcher(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._real_original = patch.ORIGINAL_SHA256
        cls._real_low = patch.FADELOW_SHA256
        cls.orig = synthetic_pe()
        cls.low = cls.orig[:0x489CD] + patch.GATE_AFTER + cls.orig[0x489D1:]
        patch.ORIGINAL_SHA256 = hashlib.sha256(cls.orig).hexdigest()
        patch.FADELOW_SHA256 = hashlib.sha256(cls.low).hexdigest()

    @classmethod
    def tearDownClass(cls):
        patch.ORIGINAL_SHA256 = cls._real_original
        patch.FADELOW_SHA256 = cls._real_low

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.exe = self.root / "SPVOODOO.EXE"
        self.backup = self.root / ("SPVOODOO.EXE" + patch.BACKUP_SUFFIX)
        self.exe.write_bytes(self.orig)

    def test_mapping_and_patch(self):
        self.assertEqual(patch.pe_offset(self.orig), 0x489CD)
        self.assertEqual(patch.make_fadelow(self.orig), self.low)
        changed = [i for i, (a, b) in enumerate(zip(self.orig, self.low)) if a != b]
        self.assertEqual(changed, list(range(0x489CD, 0x489D1)))

    def test_original_status_is_read_only(self):
        self.assertIn("ORIGINAL", patch.perform("status", self.exe))
        self.assertFalse(self.backup.exists())

    def test_copy_produces_new_file_without_backup(self):
        output = self.root / "SPVOODOO-FadeLow.EXE"
        patch.perform("copy", self.exe, output)
        self.assertEqual(output.read_bytes(), self.low)
        self.assertEqual(self.exe.read_bytes(), self.orig)
        self.assertFalse(self.backup.exists())

    def test_apply_requires_consent(self):
        with self.assertRaisesRegex(patch.PatchError, "--yes"):
            patch.perform("apply", self.exe)
        self.assertEqual(self.exe.read_bytes(), self.orig)
        self.assertFalse(self.backup.exists())

    def test_apply_and_restore_roundtrip_with_backup(self):
        text = patch.perform("apply", self.exe, confirmed=True)
        self.assertIn("FadeLow applied", text)
        self.assertEqual(self.backup.read_bytes(), self.orig)
        self.assertEqual(self.exe.read_bytes(), self.low)
        self.assertIn("FADELOW", patch.perform("status", self.exe))
        self.assertIn("Already patched", patch.perform("apply", self.exe, confirmed=True))
        result = patch.perform("restore", self.exe, confirmed=True)
        self.assertIn("Original restored", result)
        self.assertEqual(self.exe.read_bytes(), self.orig)
        self.assertEqual(self.backup.read_bytes(), self.orig)
        self.assertIn("already restored", patch.perform("restore", self.exe, confirmed=True))

    def test_resume_from_existing_valid_backup(self):
        self.backup.write_bytes(self.orig)
        self.assertIn("existing", patch.perform("apply", self.exe, confirmed=True))
        self.assertEqual(self.exe.read_bytes(), self.low)

    def test_corrupt_backup_refused_without_modification(self):
        self.backup.write_bytes(b"garbled")
        for action in ("apply", "restore", "copy", "status"):
            with self.assertRaisesRegex(patch.PatchError, "Backup exists"):
                patch.perform(action, self.exe, self.root / "out.exe", confirmed=True)
        self.assertEqual(self.exe.read_bytes(), self.orig)

    def test_refuse_restore_without_backup(self):
        self.exe.write_bytes(self.low)
        with self.assertRaisesRegex(patch.PatchError, "backup missing"):
            patch.perform("restore", self.exe, confirmed=True)
        self.assertEqual(self.exe.read_bytes(), self.low)

    def test_refuse_unknown_exe(self):
        broken = bytearray(self.orig)
        broken[0x401] = 0x7F
        self.exe.write_bytes(broken)
        with self.assertRaisesRegex(patch.PatchError, "Unsupported original"):
            patch.perform("apply", self.exe, confirmed=True)
        self.assertFalse(self.backup.exists())

    def test_refuse_existing_output(self):
        output = self.root / "out.exe"
        output.write_bytes(b"KEEP")
        with self.assertRaisesRegex(patch.PatchError, "exists"):
            patch.perform("copy", self.exe, output)
        self.assertEqual(output.read_bytes(), b"KEEP")

    def test_refuse_source_symlink(self):
        link = self.root / "link.exe"
        link.symlink_to(self.exe)
        with self.assertRaisesRegex(patch.PatchError, "symlink"):
            patch.perform("apply", link, confirmed=True)
        self.assertEqual(self.exe.read_bytes(), self.orig)

    def test_refuse_backup_symlink(self):
        self.backup.symlink_to(self.exe)
        with self.assertRaisesRegex(patch.PatchError, "Backup symlink"):
            patch.perform("apply", self.exe, confirmed=True)
        self.assertEqual(self.exe.read_bytes(), self.orig)

    def test_refuse_output_symlink(self):
        dest = self.root / "alt.exe"
        dest.symlink_to(self.exe)
        with self.assertRaisesRegex(patch.PatchError, "Destination symlink"):
            patch.perform("copy", self.exe, dest)
        self.assertEqual(self.exe.read_bytes(), self.orig)

    def test_refuse_same_output(self):
        with self.assertRaisesRegex(patch.PatchError, "differ"):
            patch.perform("copy", self.exe, self.exe)
        self.assertEqual(self.exe.read_bytes(), self.orig)

    def test_bad_pe_signature(self):
        broken = bytearray(self.orig)
        broken[0x80:0x84] = b"FAIL"
        with self.assertRaisesRegex(patch.PatchError, "PE signature"):
            patch.pe_offset(broken)

    def test_restore_refuses_unknown_installed_bytes(self):
        self.backup.write_bytes(self.orig)
        self.exe.write_bytes(b"different")
        with self.assertRaisesRegex(patch.PatchError, "Unsupported current"):
            patch.perform("restore", self.exe, confirmed=True)
        self.assertEqual(self.exe.read_bytes(), b"different")

    def test_restore_requires_confirmation(self):
        self.backup.write_bytes(self.orig)
        self.exe.write_bytes(self.low)
        with self.assertRaisesRegex(patch.PatchError, "--yes"):
            patch.perform("restore", self.exe)
        self.assertEqual(self.exe.read_bytes(), self.low)


if __name__ == "__main__":
    unittest.main()
