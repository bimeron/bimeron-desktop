"""Check generated disabled WiX fixture; this is not MSI lifetime evidence."""
import copy
from pathlib import Path
import sys
import struct
import xml.etree.ElementTree as ET

NS = {"w": "http://schemas.microsoft.com/wix/2006/wi"}


def validate(root, architecture):
    product = root.find("w:Product", NS)
    assert product.find("w:Package", NS).get("Platform") == architecture
    binary = product.find("w:Binary", NS)
    assert Path(binary.get("SourceFile")).suffix == ".dll"
    image = Path(binary.get("SourceFile")).read_bytes()
    pe = struct.unpack_from("<I", image, 0x3C)[0]
    assert image[pe:pe + 4] == b"PE\0\0"
    machine = struct.unpack_from("<H", image, pe + 4)[0]
    assert machine == {"x64": 0x8664, "arm64": 0xAA64}[architecture]
    action = product.find("w:CustomAction", NS)
    assert action.get("BinaryKey") == binary.get("Id")
    assert action.get("DllEntry") == "ForegroundPrototype"
    assert action.get("Return") == "check"
    assert action.get("Execute") == "immediate"
    gate = product.find("w:InstallExecuteSequence/w:Custom", NS)
    assert gate.get("Action") == action.get("Id")
    assert gate.get("Before") == "InstallInitialize" and gate.text == "1"
    assert product.find("w:MajorUpgrade", NS).get("Schedule") == "afterInstallInitialize"
    assert not product.findall(".//w:File", NS)  # Binary is not installed helper.
    assert not product.findall(".//w:MoveFile", NS)


root = ET.parse(sys.argv[1]).getroot()
validate(root, sys.argv[2])
for xpath, key, bad in [
    ("w:Product/w:Package", "Platform", "x86"),
    ("w:Product/w:CustomAction", "Return", "ignore"),
    ("w:Product/w:CustomAction", "BinaryKey", "InstalledHelper"),
    ("w:Product/w:InstallExecuteSequence/w:Custom", "Before", "InstallFiles"),
    ("w:Product/w:MajorUpgrade", "Schedule", "afterInstallFinalize"),
]:
    changed = copy.deepcopy(root)
    changed.find(xpath, NS).set(key, bad)
    try:
        validate(changed, sys.argv[2])
    except AssertionError:
        continue
    raise AssertionError(f"accepted unsafe fixture mutation: {key}={bad}")
print("PASS: generated WiX Binary/architecture/hard-gate sequence and five negative mutations")
