"""Offline preparation/verification only; never opens an installer session."""
import ctypes as C
from ctypes import wintypes as W
import hashlib, importlib.util, json, shutil, subprocess
from pathlib import Path
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
OUT = ROOT / 'build/lab-costing-probe/Release'
SOURCE = Path('C:/Users/PC/.t3/worktrees/bimeron-desktop/t3-dd53e01c/build/foreground-lab-component-final-x64/Release/lab-trace-Success.msi')
PIN = '0c73788944a2d520628e7dd619df2eb507821021fd76cef4dd879851586d0dbf'
spec = importlib.util.spec_from_file_location('checker', HERE.parent/'lab-trace-check.py')
m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
msi = m.msi
msi.MsiDatabaseCommit.argtypes=[W.UINT]; msi.MsiDatabaseCommit.restype=W.UINT
NAMES=['original','component-stream','feature-components','both']
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def encoded(name):
    # MSI table encoding, independently documented by Wine dlls/msi/table.c.
    alphabet='0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz._'
    result='\u4840'
    for i in range(0,len(name),2):
        a=alphabet.index(name[i])
        result+=chr(0x3800+a+(alphabet.index(name[i+1])<<6)) if i+1<len(name) else chr(0x4800+a)
    return result

def streams(p):
    text=subprocess.check_output([str(OUT/'lab-storage.exe'),'dump',str(p)],text=True)
    return {''.join(chr(int(k[i:i+4],16)) for i in range(0,len(k),4)): bytes.fromhex(v)
            for k,v in (line.split(':') for line in text.splitlines())}
def logical(p):
    db=W.UINT(); m.check(msi.MsiOpenDatabaseW(str(p),None,C.byref(db)))
    try:
        columns=m.rows(db,'_Columns',['Table','Number','Name','Type'])
        result={'_Tables':m.rows(db,'_Tables',['Name']), '_Columns':columns}
        for table, in result['_Tables']:
            names=[r[2] for r in sorted((r for r in columns if r[0]==table),key=lambda r:int(r[1]))]
            result[table]=m.rows(db,table,names)
        return result
    finally: msi.MsiCloseHandle(db)
def feature(p):
    db=W.UINT();m.check(msi.MsiOpenDatabaseW(str(p),C.cast(C.c_void_p(1),W.LPCWSTR),C.byref(db)))
    view=W.UINT()
    try:
        sql='CREATE TABLE `FeatureComponents` (`Feature_` CHAR(38) NOT NULL, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Feature_`, `Component_`)'
        m.check(msi.MsiDatabaseOpenViewW(db,sql,C.byref(view)));m.check(msi.MsiViewExecute(view,0));m.check(msi.MsiDatabaseCommit(db))
    finally:
        if view.value:msi.MsiCloseHandle(view)
        msi.MsiCloseHandle(db)

def main():
    assert sha(SOURCE)==PIN
    base=logical(SOURCE); physical=streams(SOURCE)
    assert base['Component']==[]
    assert [r for r in base['_Columns'] if r[0]=='Component']==m.COMPONENT_SCHEMA
    assert encoded('Component') not in physical
    # Known nonempty table stream mappings, not a guessed Component literal.
    for table in ['Feature','Directory','Property','_Tables','_Columns']:
        assert encoded(table) in physical and physical[encoded(table)]
    reports={}
    for i,name in enumerate(NAMES):
        p=OUT/(name+'.msi');assert not p.exists(), 'refuse overwriting diagnostic copies'
        shutil.copyfile(SOURCE,p)
        if i&2: feature(p)
        before=streams(p)
        if i&1:
            subprocess.run([str(OUT/'lab-storage.exe'),'zero',str(p),encoded('Component')],check=True)
        after=streams(p)
        if i&1: assert after=={**before,encoded('Component'):b''}
        else: assert after==before
        actual=logical(p); expected=json.loads(json.dumps(base))
        if i&2:
            expected['_Tables']=sorted(expected['_Tables']+[['FeatureComponents']])
            expected['_Columns']=sorted(expected['_Columns']+[['FeatureComponents','1','Feature_',str(11558)],['FeatureComponents','2','Component_',str(11592)]])
            expected['FeatureComponents']=[]
        assert actual==expected, (name,'unexpected logical difference')
        if not i&2: assert after==({**physical,encoded('Component'):b''} if i&1 else physical)
        else:
            changed={k for k in physical.keys()|before.keys() if physical.get(k)!=before.get(k)}
            assert changed <= {encoded(t) for t in ['_Tables','_Columns','_StringPool','_StringData','FeatureComponents']}, changed
        assert (encoded('Component') in after)==bool(i&1)
        reports[name]={'sha256':sha(p),'tables':actual,'streams':{k.encode('unicode_escape').decode():{'size':len(v),'sha256':hashlib.sha256(v).hexdigest()} for k,v in after.items()}}
    assert len({r['sha256'] for r in reports.values()})==4
    assert sha(SOURCE)==PIN and sha(OUT/'original.msi')==PIN
    (OUT/'offline-report.json').write_text(json.dumps(reports,indent=2))
    pins='static const wchar_t* names[]={'+','.join('L"'+n+'.msi"' for n in NAMES)+'};\n'
    pins+='static const char* pins[]={'+','.join('"'+reports[n]['sha256']+'"' for n in NAMES)+'};\n'
    (HERE/'pins.h').write_text(pins)
    print(json.dumps({k:v['sha256'] for k,v in reports.items()},indent=2))
if __name__=='__main__':main()
