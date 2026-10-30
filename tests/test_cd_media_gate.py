"""Offline synthetic PE tests for the experimental CD media-flag bypass."""
import hashlib
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("cdtest",ROOT/"tools"/"experimental_cd_media_gate.py")
mod=importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


def make_fake():
    b=bytearray(0x50000)
    b[:2]=b"MZ"
    struct.pack_into("<I",b,0x3c,0x80)
    b[0x80:0x84]=b"PE\x00\x00"
    struct.pack_into("<H",b,0x80+6,1)
    struct.pack_into("<H",b,0x80+20,0xe0)
    struct.pack_into("<H",b,0x80+24,0x10b)
    struct.pack_into("<I",b,0x80+24+28,0x400000)
    s=0x80+24+0xe0
    b[s:s+8]=b".text\x00\x00\x00"
    struct.pack_into("<IIII",b,s+8,0x49000,0x1000,0x49000,0x400)
    b[0x34c1b:0x34c1d]=mod.ORIGINAL_BYTES
    return bytes(b)


class CDBranchOfflineTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original_known=mod.KNOWN
        cls.source=make_fake()
        cls.expect=cls.source[:0x34c1b]+mod.TEST_BYTES+cls.source[0x34c1d:]
        mod.KNOWN={mod.sha256(cls.source):("synthetic",mod.sha256(cls.expect))}

    @classmethod
    def tearDownClass(cls):
        mod.KNOWN=cls.original_known

    def test_pe_mapping(self):
        self.assertEqual(mod.gate_offset(self.source),0x34c1b)

    def test_exact_two_byte_delta(self):
        label,result,offset=mod.make_test_copy(self.source)
        self.assertEqual(label,"synthetic")
        self.assertEqual(offset,0x34c1b)
        self.assertEqual(len(result),len(self.source))
        self.assertEqual(result,self.expect)
        self.assertEqual([i for i,(a,b) in enumerate(zip(self.source,result)) if a!=b],
                         [0x34c1b,0x34c1c])

    def test_reject_unknown_binary(self):
        raw=bytearray(self.source)
        raw[0x1000]=0x55
        with self.assertRaisesRegex(ValueError,"Unknown input"):
            mod.make_test_copy(raw)

    def test_reject_modified_gate(self):
        raw=bytearray(self.source)
        raw[0x34c1b]=0x90
        mod.KNOWN[mod.sha256(raw)]=("synthetic bad",mod.sha256(self.expect))
        try:
            with self.assertRaisesRegex(ValueError,"signature missing"):
                mod.make_test_copy(raw)
        finally:
            mod.KNOWN.pop(mod.sha256(raw),None)

    def test_reject_invalid_header(self):
        b=bytearray(self.source)
        b[0x80:0x84]=b"FAIL"
        with self.assertRaisesRegex(ValueError,"PE signature"):
            mod.gate_offset(b)

    def test_reject_bad_expected_sha(self):
        k=mod.sha256(self.source)
        mod.KNOWN[k]=("synthetic","0"*64)
        try:
            with self.assertRaisesRegex(ValueError,"checksum"):
                mod.make_test_copy(self.source)
        finally:
            mod.KNOWN[k]=("synthetic",mod.sha256(self.expect))


if __name__=="__main__":
    unittest.main()
