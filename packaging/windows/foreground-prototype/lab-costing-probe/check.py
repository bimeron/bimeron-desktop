"""Host checks: hash-only mode and rejection; no MSI action execution."""
import hashlib, re, subprocess
from pathlib import Path
from prepare import OUT, HERE, NAMES, logical, streams, sha, SOURCE, PIN
import json
report=json.loads((OUT/'offline-report.json').read_text())
assert sha(SOURCE)==PIN
for name in NAMES:
    path=OUT/(name+'.msi')
    assert sha(path)==report[name]['sha256']
    assert logical(path)==report[name]['tables']
    assert {k.encode('unicode_escape').decode(): {'size':len(v),'sha256':hashlib.sha256(v).hexdigest()} for k,v in streams(path).items()}==report[name]['streams']
exe=OUT/'lab-costing-probe.exe'
def run(arg,code):
    p=subprocess.run([str(exe),arg],capture_output=True,text=True)
    assert p.returncode==code,(arg,p.returncode)
    return p.stdout
assert run('--verify-only',0)=='{"validation":0}\n'
run('CostInitialize',2);run('original',2);run('--unknown',2)
for i,name in enumerate(NAMES):
    p=OUT/(name+'.msi'); original=p.read_bytes()
    try:
        p.write_bytes(original+b'bad')
        assert run('--verify-only',4)==f'{{"case":{i},"validation":1}}\n'
    finally:p.write_bytes(original)
a,b=[OUT/(n+'.msi') for n in NAMES[:2]]
x,y=a.read_bytes(),b.read_bytes()
try:
    a.write_bytes(y);b.write_bytes(x);run('--verify-only',4)
finally:a.write_bytes(x);b.write_bytes(y)
dump=Path('C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/VC/Tools/MSVC/14.51.36231/bin/Hostx64/x64/dumpbin.exe')
imports=subprocess.check_output([str(dump),'/imports',str(exe)],text=True)
exports=subprocess.check_output([str(dump),'/exports','C:/Windows/System32/msi.dll'],text=True)
export_names={int(a):b for a,b in re.findall(r'^\s*(\d+)\s+[0-9A-F]+\s+[0-9A-F]+\s+(Msi\w+)\s*$',exports,re.M)}
msi_block=imports.split('    msi.dll\n',1)[1].split('    ole32.dll',1)[0]
assert {export_names[int(n)] for n in re.findall(r'Ordinal\s+(\d+)',msi_block)}=={'MsiOpenPackageExW','MsiDoActionW','MsiCloseHandle'}
assert 'delay load' not in imports.lower()
src=(HERE/'probe.cpp').read_text()
assert len(re.findall(r'MsiDoActionW\(',src))==1
assert 'MsiDoActionW(h,L"CostInitialize")' in src
assert 'MsiOpenPackageExW(p.c_str(),MSIOPENPACKAGEFLAGS_IGNOREMACHINESTATE,&h)' in src
(OUT/'probe.imports.txt').write_text(imports)
print('PASS: four corrupt hashes, swapped cases, three invalid arguments, fixed MSI imports/action; host verify-only')
for name in ['lab-costing-probe.exe','lab-storage.exe']+[n+'.msi' for n in NAMES]:
    print(name,hashlib.sha256((OUT/name).read_bytes()).hexdigest())
