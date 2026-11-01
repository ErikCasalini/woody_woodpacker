import argparse
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
PACKER = ROOT / "woody_woodpacker"


def run(args, cwd, env=None):
    return subprocess.run(
        [str(arg) for arg in args], cwd=cwd, env=env,
        capture_output=True, timeout=10,
    )


def phdrs(data):
    off = struct.unpack_from("<Q", data, 32)[0]
    entsz, num = struct.unpack_from("<HH", data, 54)
    return [
        (off + i * entsz, struct.unpack_from("<IIQQQQQQ", data, off + i * entsz))
        for i in range(num)
    ]


class PackerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.fixtures_dir = tempfile.TemporaryDirectory(prefix="woody-fixtures-")
        cls.addClassCleanup(cls.fixtures_dir.cleanup)
        work = Path(cls.fixtures_dir.name)
        src = work / "fixture.c"
        src.write_text('''#include <stdio.h>
#include <stdlib.h>

static volatile unsigned char bss[8193];
static int ready;

__attribute__((constructor)) static void init(void)
{
    ready = 11;
}

int main(int argc, char **argv)
{
    if (ready != 11 || bss[0] || bss[4096] || bss[8192])
        return 99;
    bss[4096] = 42;
    printf("payload:%d:%s:%s:%u\\n", argc, argv[1],
        getenv("WOODY_TEST_VALUE"), bss[4096]);
    fprintf(stderr, "stderr-ok\\n");
    return 37;
}
''')
        cls.fixtures = []
        for name, flags in (("pie", ["-fPIE", "-pie"]), ("exec", ["-no-pie"])):
            fixture = work / name
            result = run(["clang", *flags, src, "-o", fixture], work)
            if result.returncode:
                raise RuntimeError(result.stderr.decode())
            cls.fixtures.append(fixture)
        asm = work / "static.s"
        asm.write_text('''.intel_syntax noprefix
.global _start
.text
_start:
    mov eax, 1
    mov edi, 1
    lea rsi, [rip + msg]
    mov edx, 10
    syscall
    mov eax, 60
    mov edi, 23
    syscall
.section .rodata
msg: .ascii "static-ok\\n"
.section .note.GNU-stack,"",@progbits
''')
        fixture = work / "static"
        result = run(["clang", "-nostdlib", "-static", asm, "-o", fixture], work)
        if result.returncode:
            raise RuntimeError(result.stderr.decode())
        cls.fixtures.append(fixture)

    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory(prefix="woody-test-")
        self.addCleanup(self.temp_dir.cleanup)
        self.work = Path(self.temp_dir.name)
        self.output = self.work / "woody"

    def pack(self, target, packer=None):
        return run([packer or PACKER, target], self.work)

    def test_runs_pie_exec_and_static_with_args_env_and_bss(self):
        env = dict(os.environ, WOODY_TEST_VALUE="env-ok")
        for fixture in self.fixtures:
            with self.subTest(fixture=fixture.name):
                original = run([fixture, "arg-é", "second"], self.work, env)
                self.assertIn(original.returncode, (23, 37))
                packed = self.pack(fixture)
                self.assertEqual(packed.returncode, 0, packed.stderr)
                executed = run([self.output, "arg-é", "second"], self.work, env)
                self.assertEqual(executed.returncode, original.returncode, executed.stderr)
                self.assertTrue(executed.stdout.endswith(original.stdout), executed.stdout)
                self.assertEqual(executed.stderr, original.stderr)
                data = self.output.read_bytes()
                self.assertEqual(data[16:18], fixture.read_bytes()[16:18])
                headers = [ph for _, ph in phdrs(data)]
                self.assertEqual(len(headers), 4)
                self.assertFalse(any(ph[0] in (2, 3) for ph in headers))
                payload = [ph for ph in headers if ph[0] == 1][-1]
                magic, compressed_sz, decompressed_sz, off, key = struct.unpack_from(
                    "<8sQQQ32s", data, payload[2],
                )
                self.assertEqual((magic, off), (b"WOODY\0\0\0", 64))
                self.assertEqual(compressed_sz, fixture.stat().st_size)
                self.assertEqual(decompressed_sz, fixture.stat().st_size)
                # The payload is encrypted at rest: a random key is stored and
                # the bytes on disk must differ from the original ELF and must
                # not expose the ELF magic in clear.
                self.assertNotEqual(key, b"\x00" * 32)
                stored_payload = data[payload[2] + off:]
                self.assertEqual(len(stored_payload), fixture.stat().st_size)
                self.assertNotEqual(stored_payload, fixture.read_bytes())
                self.assertNotEqual(stored_payload[:4], b"\x7fELF")

    def test_invalid_inputs_preserve_existing_output_or_leave_it_absent(self):
        base = bytearray(self.fixtures[0].read_bytes())
        interp_off, interp = next((off, ph) for off, ph in phdrs(base) if ph[0] == 3)
        load_off, _ = next((off, ph) for off, ph in phdrs(base) if ph[0] == 1)
        invalid = {"empty": b"", "short": base[:32], "not-elf": b"X" * 64}
        data = bytearray(base)
        struct.pack_into("<Q", data, 32, (1 << 64) - 1)
        invalid["phoff-overflow"] = data
        data = bytearray(base)
        struct.pack_into("<Q", data, load_off + 48, 3)
        invalid["invalid-align"] = data
        data = bytearray(base)
        struct.pack_into("<Q", data, load_off + 16, (1 << 64) - 1)
        invalid["segment-overflow"] = data
        data = bytearray(base)
        struct.pack_into("<Q", data, interp_off + 32, 1)
        invalid["short-interp"] = data
        data = bytearray(base)
        data[interp[2] + interp[5] - 1] = ord("X")
        invalid["unterminated-interp"] = data
        for name, data in invalid.items():
            target = self.work / name
            target.write_bytes(data)
            for existing in (False, True):
                with self.subTest(input=name, existing=existing):
                    self.output.unlink(missing_ok=True)
                    sentinel = b"previous output\x00\xff"
                    if existing:
                        self.output.write_bytes(sentinel)
                    result = self.pack(target)
                    self.assertEqual(result.returncode, 1)
                    self.assertTrue(result.stderr.startswith(b"Error:"), result.stderr)
                    if existing:
                        self.assertEqual(self.output.read_bytes(), sentinel)
                    else:
                        self.assertFalse(self.output.exists())

    def test_rejects_target_output_alias_without_truncation(self):
        target = self.fixtures[0]
        original = target.read_bytes()
        self.output.hardlink_to(target)
        for input_path in (target, self.output):
            with self.subTest(input=input_path.name):
                self.assertEqual(self.pack(input_path).returncode, 1)
                self.assertEqual(self.output.read_bytes(), original)
                self.assertEqual(target.read_bytes(), original)

    def test_missing_stub_preserves_output(self):
        stripped = self.work / "packer-no-stub"
        result = run(["objcopy", "--remove-section=.stub", PACKER, stripped], self.work)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.output.write_bytes(b"previous output")
        result = self.pack(self.fixtures[0], stripped)
        self.assertEqual(result.returncode, 1)
        self.assertEqual(result.stderr, b"Error: .stub section not found\n")
        self.assertEqual(self.output.read_bytes(), b"previous output")

    def test_stub_rejects_each_corrupted_magic_byte(self):
        packed = self.pack(self.fixtures[0])
        self.assertEqual(packed.returncode, 0, packed.stderr)
        data = self.output.read_bytes()
        payload = [ph for _, ph in phdrs(data) if ph[0] == 1][-1]
        for i in range(6):
            with self.subTest(byte=i):
                corrupted = bytearray(data)
                corrupted[payload[2] + i] ^= 1
                self.output.write_bytes(corrupted)
                result = run([self.output], self.work)
                self.assertEqual(result.returncode, 1)
                self.assertIn(b"find_elf_payload_header", result.stderr)

    def test_key_is_random_printed_and_matches_header(self):
        keys = set()
        for _ in range(2):
            packed = self.pack(self.fixtures[0])
            self.assertEqual(packed.returncode, 0, packed.stderr)
            lines = [l for l in packed.stdout.splitlines()
                     if l.startswith(b"key_value:")]
            self.assertEqual(len(lines), 1, packed.stdout)
            printed = lines[0].split(b"0x", 1)[1].strip()
            self.assertEqual(len(printed), 64)   # 32 key bytes -> 64 hex chars
            int(printed, 16)                      # must be valid hexadecimal
            data = self.output.read_bytes()
            payload = [ph for _, ph in phdrs(data) if ph[0] == 1][-1]
            key_bytes = data[payload[2] + 32:payload[2] + 64]
            self.assertEqual(printed.decode(), key_bytes.hex())
            keys.add(bytes(key_bytes))
        self.assertEqual(len(keys), 2)            # two packings -> two keys

    def test_stub_reports_corrupted_payload_without_crashing(self):
        packed = self.pack(self.fixtures[0])
        self.assertEqual(packed.returncode, 0, packed.stderr)
        data = bytearray(self.output.read_bytes())
        payload = [ph for _, ph in phdrs(data) if ph[0] == 1][-1]
        # Flip a byte of the encrypted ELF header: once decrypted, the ELF magic
        # no longer matches, so the stub must refuse it cleanly (not segfault).
        data[payload[2] + 64] ^= 0xff
        self.output.write_bytes(data)
        result = run([self.output], self.work)
        self.assertEqual(result.returncode, 1)    # clean exit, not killed by a signal
        self.assertIn(b"corrupted", result.stderr)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("packer", nargs="?", type=Path, default=PACKER)
    PACKER = parser.parse_args().packer.resolve()
    unittest.main(argv=[__file__], verbosity=2)
