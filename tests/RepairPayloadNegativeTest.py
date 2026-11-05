"""Exercise the actual configure-time broker gate with inert bytes, never Core.
Run: python tests/RepairPayloadNegativeTest.py
Extraction deliberately fails if the production gate boundaries change.
"""
import hashlib
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'CMakeLists.txt').read_text()
start = source.index('if(BIMERON_REPAIR_BROKER)')
end = source.index('    set_source_files_properties', start)
gate = source[start:end] + '\nendif()\n'

def pe(machine):
    data = bytearray(128)
    data[:2] = b'MZ'
    data[60:64] = (64).to_bytes(4, 'little')
    data[64:70] = b'PE\0\0' + machine.to_bytes(2, 'little')
    return data

def elf(machine):
    data = bytearray(64)
    data[:6] = b'\x7fELF\x02\x01'
    data[18:20] = machine.to_bytes(2, 'little')
    return data

with tempfile.TemporaryDirectory(prefix='bimeron-negative-') as directory:
    base = Path(directory)
    payload = base / 'inert-payload'
    script = base / 'gate.cmake'
    count = 0
    for windows, factory, machines in [(True, pe, [0x8664, 0xaa64]), (False, elf, [62, 183])]:
        for index, arch in enumerate(['amd64', 'arm64']):
            good = bytes(factory(machines[index]))
            cases = [
                ('absent', None, '0' * 64, 'existing payload'),
                ('digest', good, '0' * 64, 'SHA-256 mismatch'),
                ('architecture', bytes(factory(machines[1-index])), None, 'architecture mismatch'),
                ('truncated', b'MZ' if windows else b'\x7fELF', None, 'Truncated' if windows else 'ELF64'),
                ('changed-after-review', good + b'changed', hashlib.sha256(good).hexdigest(), 'SHA-256 mismatch'),
            ]
            for name, data, digest, expected in cases:
                if payload.exists():
                    payload.unlink()
                if data is not None:
                    payload.write_bytes(data)
                digest = digest or hashlib.sha256(data).hexdigest()
                script.write_text(f'set(WIN32 {"TRUE" if windows else "FALSE"})\n'
                    f'set(BIMERON_PACKAGE_ARCH {arch})\n'
                    f'set(BIMERON_REPAIR_BROKER "{payload.as_posix()}")\n'
                    f'set(BIMERON_REPAIR_BROKER_SHA256 {digest})\n' + gate)
                result = subprocess.run(['cmake', '-P', str(script)], capture_output=True, text=True)
                assert result.returncode != 0 and expected in result.stderr, (windows, arch, name, result.stdout, result.stderr)
                count += 1
    print(f'{count} production configure-gate negative cases passed; no payload executed')
