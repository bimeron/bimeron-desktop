"""Offline actual MSI verification and canonical manifest generation; no installer session."""
import sys
sys.dont_write_bytecode = True
import importlib.util
from pathlib import Path
import ctypes as C
from ctypes import wintypes as W
import copy
import hashlib
import json
import struct
import uuid
spec = importlib.util.spec_from_file_location('trace', Path(__file__).with_name('lab-trace-check.py'))
t = importlib.util.module_from_spec(spec)
spec.loader.exec_module(t)
COLUMNS = {**t.COLUMNS, '_Streams':['Name'], '_Storages':['Name']}
NAMES = dict(zip(t.ACTIONS, ['AckPreflight','AckRollback','AckDeferred','AckCommit','AckPostFinalize']))
ACTIONS = {NAMES[k]:v for k,v in t.ACTIONS.items()}
IMPORTS = {'KERNEL32.DLL','ADVAPI32.DLL','MSI.DLL','BCRYPT.DLL','OLE32.DLL','SHELL32.DLL'}

# Reviewed direct API allowlist; MSI ordinals are the SDK import-library entries.
API_IMPORTS = {'ADVAPI32.DLL': ['ConvertStringSecurityDescriptorToSecurityDescriptorW',
                  'ConvertStringSidToSidW',
                  'EqualSid',
                  'GetAce',
                  'GetSecurityDescriptorControl',
                  'GetSecurityDescriptorDacl',
                  'GetSecurityDescriptorOwner',
                  'GetSecurityInfo',
                  'GetTokenInformation',
                  'IsValidAcl',
                  'IsValidSecurityDescriptor',
                  'IsValidSid',
                  'IsWellKnownSid',
                  'MapGenericMask',
                  'OpenProcessToken',
                  'OpenThreadToken'],
 'MSI.DLL': ['#103',
             '#113',  # MsiQueryProductStateW (system msi.dll export)
             '#118',
             '#120',
             '#125',
             '#150',
             '#159',
             '#160',
             '#17',
             '#32',
             '#49',
             '#64',
             '#74',
             '#78',
             '#8'],
 'BCRYPT.DLL': ['BCryptHash'],
 'OLE32.DLL': ['CoTaskMemFree', 'StringFromGUID2'],
 'SHELL32.DLL': ['SHGetKnownFolderPath'],
 'KERNEL32.DLL': ['CloseHandle',
                  'CompareStringOrdinal',
                  'CreateFileW',
                  'DeleteCriticalSection',
                  'EncodePointer',
                  'EnterCriticalSection',
                  'ExitProcess',
                  'FindClose',
                  'FindFirstFileExW',
                  'FindNextFileW',
                  'FlsAlloc',
                  'FlsFree',
                  'FlsGetValue',
                  'FlsSetValue',
                  'FlushFileBuffers',
                  'FreeEnvironmentStringsW',
                  'FreeLibrary',
                  'GetACP',
                  'GetCPInfo',
                  'GetCommandLineA',
                  'GetCommandLineW',
                  'GetConsoleMode',
                  'GetConsoleOutputCP',
                  'GetCurrentProcess',
                  'GetCurrentProcessId',
                  'GetCurrentThread',
                  'GetCurrentThreadId',
                  'GetDriveTypeW',
                  'GetEnvironmentStringsW',
                  'GetFileInformationByHandle',
                  'GetFileInformationByHandleEx',
                  'GetFileType',
                  'GetLastError',
                  'GetModuleFileNameW',
                  'GetModuleHandleExW',
                  'GetModuleHandleW',
                  'GetOEMCP',
                  'GetProcAddress',
                  'GetProcessHeap',
                  'GetStartupInfoW',
                  'GetStdHandle',
                  'GetStringTypeW',
                  'GetSystemTimeAsFileTime',
                  'GetVolumeNameForVolumeMountPointW',
                  'HeapAlloc',
                  'HeapFree',
                  'HeapReAlloc',
                  'HeapSize',
                  'InitializeCriticalSectionEx',
                  'InitializeSListHead',
                  'InterlockedFlushSList',
                  'IsDebuggerPresent',
                  'IsProcessorFeaturePresent',
                  'IsValidCodePage',
                  'LCMapStringW',
                  'LeaveCriticalSection',
                  'LoadLibraryExW',
                  'LocalFree',
                  'MultiByteToWideChar',
                  'QueryPerformanceCounter',
                  'RaiseException',
                  'ReadFile',
                  'RtlCaptureContext',
                  'RtlLookupFunctionEntry',
                  'RtlPcToFileHeader',
                  'RtlUnwindEx',
                  'RtlVirtualUnwind',
                  'SetFilePointerEx',
                  'SetLastError',
                  'SetStdHandle',
                  'SetUnhandledExceptionFilter',
                  'TerminateProcess',
                  'UnhandledExceptionFilter',
                  'VerSetConditionMask',
                  'VerifyVersionInfoW',
                  'VirtualProtect',
                  'WideCharToMultiByte',
                  'WriteConsoleW',
                  'WriteFile']}

def pe_check(path, arch, actions, allowed_imports):
    b = path.read_bytes(); pe = struct.unpack_from('<I', b, 60)[0]
    assert b[pe:pe+4] == b'PE\0\0'
    assert struct.unpack_from('<H', b, pe+4)[0] == {'x64':0x8664, 'arm64':0xaa64}[arch]
    optional = pe+24; assert struct.unpack_from('<H', b, optional)[0] == 0x20b
    sections = []
    for n in range(struct.unpack_from('<H', b, pe+6)[0]):
        o = optional + struct.unpack_from('<H', b, pe+20)[0] + n*40
        size, va, rawsize, raw = struct.unpack_from('<IIII', b, o+8)
        sections.append((va, max(size, rawsize), raw))
    def offset(rva):
        for va, size, raw in sections:
            if va <= rva < va+size: return raw+rva-va
        raise AssertionError('bad RVA')
    config_rva = struct.unpack_from('<I', b, optional+112+10*8)[0]
    assert struct.unpack_from('<H', b, offset(config_rva)+78)[0] == 0x800
    export_rva = struct.unpack_from('<I', b, optional+112)[0]
    export = offset(export_rva)
    count = struct.unpack_from('<I', b, export+24)[0]
    names = struct.unpack_from('<I', b, export+32)[0]
    exports = []
    for n in range(count):
        name = offset(struct.unpack_from('<I', b, offset(names)+n*4)[0])
        exports.append(b[name:b.index(0, name)].decode())
    assert set(exports) == set(actions)
    # Reject all delay imports, not merely unfamiliar ones.
    assert struct.unpack_from('<II', b, optional+112+13*8) == (0, 0)
    imports = []; api_imports = {}; rva, size = struct.unpack_from('<II', b, optional+112+8)
    o = offset(rva)
    while any(b[o:o+20]):
        name = offset(struct.unpack_from('<I', b, o+12)[0])
        dll = b[name:b.index(0, name)].decode().upper(); imports.append(dll)
        thunk = struct.unpack_from('<I', b, o)[0] or struct.unpack_from('<I', b, o+16)[0]
        entries = []; j = offset(thunk)
        while True:
            entry = struct.unpack_from('<Q', b, j)[0]; j += 8
            if not entry: break
            if entry & (1 << 63): entries.append('#'+str(entry & 0xffff))
            else:
                symbol = offset(entry)+2
                entries.append(b[symbol:b.index(0,symbol)].decode())
            assert len(entries) <= 256
        assert dll not in api_imports
        api_imports[dll] = sorted(entries); o += 20
    assert set(imports) <= allowed_imports
    assert set(api_imports) == set(API_IMPORTS)
    for dll, entries in api_imports.items(): assert set(entries) <= set(API_IMPORTS[dll]), (dll,entries)
    path.with_suffix('.imports.json').write_text(json.dumps(api_imports,indent=2)+'\n')
    print('fixed system DLL/API imports; no delay imports:', imports)
    return hashlib.sha256(b).hexdigest()


def validate(tables, digest, variant, arch, ids):
    assert tables['_Streams']==[['\x05SummaryInformation'],['Binary.LabAckBinary']]
    assert tables['_Storages']==[]
    normalized = copy.deepcopy(tables)
    for name in ['CustomAction','InstallExecuteSequence']:
        for row in normalized[name]:
            row[0] = next((old for old,new in NAMES.items() if new==row[0]),row[0])
            if name=='CustomAction':
                assert row[2]=='LabAckBinary'
                row[2]='LabTraceBinary'
                row[3]=next((old for old,new in NAMES.items() if new==row[3]),row[3])
        normalized[name].sort()
    assert normalized['Binary']==[['LabAckBinary',digest]]
    normalized['Binary'][0][0]='LabTraceBinary'
    assert normalized['Feature'][0][2]=='LABPROTECTEDACK'
    normalized['Feature'][0][2]='LABTRACEONLY'
    props=dict(normalized['Property'])
    assert props['ProductName']=='Bimeron LABPROTECTEDACK '+variant
    next(row for row in normalized['Property'] if row[0]=='ProductName')[1]='Bimeron LABTRACEONLY '+variant
    t.validate(normalized,digest,variant,arch,ids)

def canonical(tables):
    out=bytearray()
    def number(n): out.extend(struct.pack('<I',n))
    def text(s):
        b=s.encode('utf-16le'); number(len(b)); out.extend(b)
    for table in COLUMNS:
        text(table); number(len(tables[table]))
        for row in sorted(tables[table]):
            number(len(row))
            for field in row: text(field)
    for name in ['summaryTemplate','summaryPackage','summaryFlags']: text(str(tables[name]))
    return bytes(out)

def main():
    folder,arch=Path(sys.argv[1]).resolve(),sys.argv[2]
    cache=dict(line.split('=',1) for line in (folder.parent/'CMakeCache.txt').read_text().splitlines() if ':INTERNAL=' in line)
    evidence={}; seen=set()
    for point,variant in enumerate(['Success','CommitFail','PostFinalizeFail']):
        path=folder/f'lab-protected-ack-{variant}.msi'; dll=path.with_suffix('.dll')
        digest=pe_check(dll,arch,actions=ACTIONS,allowed_imports=IMPORTS)
        db=W.UINT(); t.check(t.msi.MsiOpenDatabaseW(str(path),None,C.byref(db)))
        try:
            tables={name:t.rows(db,name,columns) for name,columns in COLUMNS.items()}
            tables.update(summaryTemplate=t.summary(db,7),summaryPackage=t.summary(db,9),summaryFlags=t.summary(db,15))
        finally: t.msi.MsiCloseHandle(db)
        ids={key:'{'+cache[f'ACK_{variant}_{kind}:INTERNAL']+'}' for key,kind in [('productCode','PRODUCT'),('upgradeCode','UPGRADE')]}
        ids['packageCode']=tables['summaryPackage']
        assert not seen.intersection(ids.values()); seen.update(ids.values())
        validate(tables,digest,variant,arch,ids)
        negatives=0
        for name in ['Component','FeatureComponents','File','Binary','CustomAction','InstallExecuteSequence','Property','Directory','Feature']:
            bad=copy.deepcopy(tables); bad[name].append(['unexpected']*len(t.COLUMNS[name]))
            try: validate(bad,digest,variant,arch,ids)
            except (AssertionError,ValueError,IndexError): negatives+=1
            else: raise AssertionError('accepted mutation '+name)
        for action in ACTIONS:
            for column,value in [(1,'1'),(2,'6700')]:
                bad=copy.deepcopy(tables); next(r for r in bad['InstallExecuteSequence'] if r[0]==action)[column]=value
                try: validate(bad,digest,variant,arch,ids)
                except AssertionError: negatives+=1
                else: raise AssertionError('accepted callback mutation')
        logical=canonical(tables)
        installation=uuid.uuid5(uuid.UUID(ids['packageCode'].strip('{}')),'installation').bytes
        generation=uuid.uuid5(uuid.UUID(ids['packageCode'].strip('{}')),'generation').bytes
        msi_hash=hashlib.sha256(path.read_bytes()).digest()
        manifest=(b'BLACK001'+installation+generation+struct.pack('<II',1 if arch=='x64' else 2,point)+
                  uuid.UUID(ids['productCode'].strip('{}')).bytes_le+uuid.UUID(ids['packageCode'].strip('{}')).bytes_le+
                  msi_hash+bytes.fromhex(digest)+hashlib.sha256(logical).digest()+logical)
        assert 176<len(manifest)<=65536
        destination=folder/f'lab-protected-ack-{variant}.run.manifest'
        destination.write_bytes(manifest)
        path.with_suffix('.tables.json').write_text(json.dumps(tables,indent=2)+'\n')
        evidence[variant]=dict(msi=str(path),dll=str(dll),msiSHA256=msi_hash.hex(),dllSHA256=digest,
            manifest=str(destination),manifestSHA256=hashlib.sha256(manifest).hexdigest(),manifestBytes=len(manifest),
            installationId=installation.hex(),generationId=generation.hex(),negativeCases=negatives,**ids)
    (folder/'lab-protected-ack-evidence.json').write_text(json.dumps(evidence,indent=2)+'\n')
    print(json.dumps(evidence,indent=2)); print('PASS: actual read-only MSI tables/Binary/PE and 57 negative models; manifests generated after MSI linking')
if __name__=='__main__': main()
