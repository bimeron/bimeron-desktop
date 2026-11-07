"""Read-only MSI/PE fixture proof. Never opens an installer session or host registry."""
import ctypes as C
from ctypes import wintypes as W
import copy
import hashlib
import json
from pathlib import Path
import struct
import sys
import uuid

msi = C.WinDLL('msi.dll', winmode=0x800)
for name, args in {
    'MsiOpenDatabaseW': [W.LPCWSTR, W.LPCWSTR, C.POINTER(W.UINT)],
    'MsiDatabaseOpenViewW': [W.UINT, W.LPCWSTR, C.POINTER(W.UINT)],
    'MsiViewExecute': [W.UINT, W.UINT],
    'MsiViewFetch': [W.UINT, C.POINTER(W.UINT)],
    'MsiRecordGetStringW': [W.UINT, W.UINT, W.LPWSTR, C.POINTER(W.DWORD)],
    'MsiRecordReadStream': [W.UINT, W.UINT, C.c_void_p, C.POINTER(W.DWORD)],
    'MsiCloseHandle': [W.UINT],
}.items():
    getattr(msi, name).argtypes = args
    getattr(msi, name).restype = W.UINT

def check(code):
    if code: raise RuntimeError(f'MSI API error {code}')

def rows(db, table, columns):
    view = W.UINT()
    check(msi.MsiDatabaseOpenViewW(db, f'SELECT {",".join("`"+c+"`" for c in columns)} FROM `{table}`', C.byref(view)))
    result = []
    try:
        check(msi.MsiViewExecute(view, 0))
        while True:
            record = W.UINT()
            code = msi.MsiViewFetch(view, C.byref(record))
            if code == 259: break
            check(code)
            try:
                values = []
                for n, column in enumerate(columns, 1):
                    if column == 'Data':
                        data = bytearray()
                        while True:
                            buffer = C.create_string_buffer(65536); size = W.DWORD(len(buffer))
                            check(msi.MsiRecordReadStream(record, n, buffer, C.byref(size)))
                            if not size.value: break
                            data.extend(buffer.raw[:size.value])
                        values.append(hashlib.sha256(data).hexdigest())
                    else:
                        buffer = C.create_unicode_buffer(32768); size = W.DWORD(len(buffer))
                        check(msi.MsiRecordGetStringW(record, n, buffer, C.byref(size)))
                        values.append(buffer.value)
                result.append(values)
            finally: msi.MsiCloseHandle(record)
    finally: msi.MsiCloseHandle(view)
    return sorted(result)

ACTIONS = {'ObservationPreflight':1, 'ObservationRollback':3329,
           'ObservationDeferredBegin':3073, 'ObservationCommit':3585,
           'ObservationPostFinalize':1}
# Reviewed x64/ARM64 direct imports from MSVC 19.51 static CRT and this fixture.
# MSI SDK ordinal imports verified against native system msi.dll exports:
# 8 MsiCloseHandle; 17 MsiCreateRecord; 103 MsiProcessMessage; 125 MsiRecordSetStringW.
CONTEXT_API_IMPORTS = {
    'ADVAPI32.DLL': set("OpenProcessToken OpenThreadToken GetLengthSid GetSidSubAuthority GetSidSubAuthorityCount GetTokenInformation IsValidSid IsWellKnownSid GetSidLengthRequired IsTokenRestricted ConvertSidToStringSidW".split()),
    'MSI.DLL': {'#8', '#17', '#103', '#125'},
    'BCRYPT.DLL': {'BCryptHash'},
    'SECUR32.DLL': {'LsaGetLogonSessionData', 'LsaFreeReturnBuffer'},
    'KERNEL32.DLL': set("WriteConsoleW CreateFileW GetConsoleMode GetConsoleOutputCP WriteFile FlushFileBuffers HeapReAlloc CloseHandle GetLastError GetProcessTimes GetCurrentProcess GetCurrentProcessId GetCurrentThread GetCurrentThreadId ProcessIdToSessionId GetSystemTime LocalFree HeapSize QueryPerformanceCounter GetSystemTimeAsFileTime InitializeSListHead SetUnhandledExceptionFilter GetStartupInfoW GetModuleHandleW RtlPcToFileHeader RaiseException RtlLookupFunctionEntry RtlUnwindEx InterlockedFlushSList SetLastError FlsAlloc FlsGetValue FlsSetValue FlsFree EncodePointer EnterCriticalSection LeaveCriticalSection InitializeCriticalSectionEx DeleteCriticalSection ExitProcess TerminateProcess FreeLibrary GetModuleHandleExW GetProcAddress GetModuleFileNameW IsProcessorFeaturePresent RtlCaptureContext RtlVirtualUnwind IsDebuggerPresent UnhandledExceptionFilter HeapFree HeapAlloc GetStdHandle GetFileType FindClose FindFirstFileExW FindNextFileW IsValidCodePage GetACP GetOEMCP GetCPInfo GetCommandLineA GetCommandLineW MultiByteToWideChar WideCharToMultiByte GetEnvironmentStringsW FreeEnvironmentStringsW VirtualProtect LoadLibraryExW LCMapStringW GetProcessHeap SetFilePointerEx GetStringTypeW SetStdHandle VerSetConditionMask VerifyVersionInfoW".split()),
}

def validate_context_imports(imports):
    assert set(imports) == set(CONTEXT_API_IMPORTS), imports
    for dll, entries in imports.items():
        assert entries and len(entries) == len(set(entries)), (dll, entries)
        assert set(entries) <= CONTEXT_API_IMPORTS[dll], (dll, set(entries) - CONTEXT_API_IMPORTS[dll])
    assert set(imports['SECUR32.DLL']) == CONTEXT_API_IMPORTS['SECUR32.DLL']
    assert set(imports['MSI.DLL']) == CONTEXT_API_IMPORTS['MSI.DLL']

def pe_check(path, arch, profile='historical', image=None, report=True):
    assert profile in {'historical', 'context'}
    b = path.read_bytes() if image is None else image; pe = struct.unpack_from('<I', b, 60)[0]
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
    assert set(exports) == set(ACTIONS)
    if profile == 'context':
        assert len(exports) == len(ACTIONS) == struct.unpack_from('<I', b, export+20)[0]
        functions = offset(struct.unpack_from('<I', b, export+28)[0])
        export_size = struct.unpack_from('<I', b, optional+116)[0]
        for n in range(len(ACTIONS)):
            target = struct.unpack_from('<I', b, functions+n*4)[0]
            assert target and not export_rva <= target < export_rva+export_size, 'forwarded export'

    # Reject all delay imports, not merely unfamiliar ones.
    assert struct.unpack_from('<II', b, optional+112+13*8) == (0, 0)
    imports = []; apis = {}; rva, size = struct.unpack_from('<II', b, optional+112+8)
    o = offset(rva)
    while any(b[o:o+20]):
        name = offset(struct.unpack_from('<I', b, o+12)[0])
        dll = b[name:b.index(0, name)].decode().upper()
        imports.append(dll)
        thunk = struct.unpack_from('<I', b, o)[0] or struct.unpack_from('<I', b, o+16)[0]
        entries = []; at = offset(thunk)
        while True:
            value = struct.unpack_from('<Q', b, at)[0]
            if not value: break
            if value & (1 << 63):
                assert value & ~((1 << 63) | 0xffff) == 0
                entries.append('#'+str(value & 0xffff))
            else:
                entry = offset(value)+2
                entries.append(b[entry:b.index(0, entry)].decode())
            at += 8
        assert dll not in apis
        apis[dll] = sorted(entries); o += 20
    if profile == 'context':
        validate_context_imports(apis)
    else:
        # Preserve the historical default DLL profile; secur32 remains refused.
        assert set(imports) <= {'KERNEL32.DLL', 'ADVAPI32.DLL', 'MSI.DLL', 'BCRYPT.DLL'}
    if report:
        path.with_suffix('.imports.json').write_text(json.dumps(apis, indent=2)+'\n')
        print('fixed system imports; no delay imports; profile='+profile+':', json.dumps(apis, sort_keys=True))
    return hashlib.sha256(b).hexdigest()


def summary(db, field):
    msi.MsiGetSummaryInformationW.argtypes = [W.UINT, W.LPCWSTR, W.UINT, C.POINTER(W.UINT)]
    msi.MsiSummaryInfoGetPropertyW.argtypes = [W.UINT, W.UINT, C.POINTER(W.UINT), C.POINTER(C.c_int), C.c_void_p, W.LPWSTR, C.POINTER(W.DWORD)]
    h = W.UINT(); check(msi.MsiGetSummaryInformationW(db, None, 0, C.byref(h)))
    try:
        kind = W.UINT(); number = C.c_int(); size = W.DWORD(1024); value = C.create_unicode_buffer(1024)
        check(msi.MsiSummaryInfoGetPropertyW(h, field, C.byref(kind), C.byref(number), None, value, C.byref(size)))
        return value.value if kind.value == 30 else number.value
    finally: msi.MsiCloseHandle(h)

COLUMNS = {'_Tables':['Name'], '_Columns':['Table','Number','Name','Type'],
    'Component':['Component','ComponentId','Directory_','Attributes','Condition','KeyPath'], 'FeatureComponents':['Feature_','Component_'], 'File':['File'], 'Media':['DiskId','LastSequence','DiskPrompt','Cabinet','VolumeLabel','Source'],
    **{name:['Action','Condition','Sequence'] for name in ['AdminExecuteSequence','AdminUISequence','AdvtExecuteSequence','InstallUISequence','InstallExecuteSequence']},
    'CustomAction':['Action','Type','Source','Target'], 'Binary':['Name','Data'],
    'Directory':['Directory','Directory_Parent','DefaultDir'],
    'Feature':['Feature','Feature_Parent','Title','Description','Display','Level','Directory_','Attributes'], 'Property':['Property','Value']}
STANDARD = {'CostFinalize': ('',1000), 'CostInitialize': ('',800), 'FileCost': ('',900),
    'InstallFinalize': ('',6600), 'InstallInitialize': ('',1500),
    'InstallValidate': ('',1400), 'ProcessComponents': ('',1600), 'PublishFeatures': ('',6300),
    'PublishProduct': ('',6400), 'RegisterProduct': ('',6100), 'RegisterUser': ('',6000),
    'UnpublishFeatures': ('',1800), 'ValidateProductID': ('',700)}
ORDER = dict(zip(ACTIONS, [1499,1501,1502,1503,6601]))

COMPONENT_SCHEMA = [['Component', str(n), name, str(kind)] for n, name, kind in [
    (1, 'Component', 11592), (2, 'ComponentId', 7462), (3, 'Directory_', 3400),
    (4, 'Attributes', 1282), (5, 'Condition', 7679), (6, 'KeyPath', 7496)]]

FEATURE_COMPONENTS_SCHEMA = [['FeatureComponents', '1', 'Feature_', '11558'],
                             ['FeatureComponents', '2', 'Component_', '11592']]

def validate(t, digest, variant, arch, identities):
    assert ['Component'] in t['_Tables'], 'missing Component table'
    assert [r for r in t['_Columns'] if r[0] == 'Component'] == COMPONENT_SCHEMA, 'malformed Component schema'
    assert t['Component'] == [], 'nonempty Component table'
    assert ['FeatureComponents'] in t['_Tables'], 'missing FeatureComponents table'
    assert [r for r in t['_Columns'] if r[0] == 'FeatureComponents'] == FEATURE_COMPONENTS_SCHEMA, 'malformed FeatureComponents schema'
    assert t['FeatureComponents'] == [], 'nonempty FeatureComponents table'
    assert {r[0] for r in t['_Tables']} == (set(COLUMNS) - {'_Tables', '_Columns'}) | {'_Validation'}, t['_Tables']
    assert t['File'] == [] and t['Media'] == [['1','0','','','','']]
    assert t['Directory'] == [['TARGETDIR','','SourceDir']]
    assert t['Feature'] == [['EmptyLab','','LABTRACEONLY','','2','1','','0']]
    assert t['CustomAction'] == sorted([[a,str(k),'LabTraceBinary',a] for a,k in ACTIONS.items()])
    assert t['Binary'] == [['LabTraceBinary',digest]]
    assert {r[0]:(r[1],int(r[2])) for r in t['InstallExecuteSequence']} == {
        **STANDARD, **{a:('NOT Installed',n) for a,n in ORDER.items()}}
    expected_other = {
      'AdminExecuteSequence': [['CostFinalize','',1000],['CostInitialize','',800],['FileCost','',900],['InstallAdminPackage','',3900],['InstallFiles','',4000],['InstallFinalize','',6600],['InstallInitialize','',1500],['InstallValidate','',1400]],
      'AdminUISequence': [['CostFinalize','',1000],['CostInitialize','',800],['ExecuteAction','',1300],['FileCost','',900]],
      'AdvtExecuteSequence': [['InstallValidate','',1400],['CostFinalize','',1000],['CostInitialize','',800],['InstallFinalize','',6600],['InstallInitialize','',1500],['PublishFeatures','',6300],['PublishProduct','',6400]],
      'InstallUISequence': [['CostFinalize','',1000],['CostInitialize','',800],['ExecuteAction','',1300],['FileCost','',900],['ValidateProductID','',700]]}
    for name, expected in expected_other.items():
        assert t[name] == sorted([[a,b,str(c)] for a,b,c in expected]), (name,t[name])
    p = dict(t['Property'])
    assert set(p) == {'ALLUSERS','Manufacturer','ProductCode','ProductLanguage','ProductName','ProductVersion','UpgradeCode'}
    assert p['ALLUSERS'] == '1' and p['Manufacturer'] == 'Bimeron' and p['ProductLanguage'] == '1033'
    assert p['ProductName'] == 'Bimeron LABTRACEONLY '+variant and p['ProductVersion'] == '0.0.1'
    assert p['ProductCode'] == identities['productCode'] and p['UpgradeCode'] == identities['upgradeCode']
    assert t['summaryTemplate'] == ('x64;1033' if arch == 'x64' else 'Arm64;1033')
    assert t['summaryPackage'] == identities['packageCode']
    assert t['summaryFlags'] == 2  # compressed; per-machine ALLUSERS above
    for value in identities.values(): assert str(uuid.UUID(value.strip('{}'))).upper() == value.strip('{}')
    assert len(set(identities.values())) == 3
    forbidden = {'{E93DE42D-1F4F-4227-8056-1086581CCE51}', '{A8945D41-FEAD-441D-AC52-FB832E9E6BE9}'}
    assert not forbidden.intersection(identities.values())

if __name__ == '__main__':
    folder, arch = Path(sys.argv[1]).resolve(), sys.argv[2]
    profile = sys.argv[3] if len(sys.argv) == 4 else 'historical'
    assert len(sys.argv) in {3, 4} and profile in {'historical', 'context'}
    cache = (folder.parent/'CMakeCache.txt').read_text()
    cache_values = dict(line.split('=',1) for line in cache.splitlines() if ':INTERNAL=' in line)
    # API policy negatives do not alter historical or other fixture allowlists.
    if profile == 'context':
        allowed = {dll: sorted(entries) for dll, entries in CONTEXT_API_IMPORTS.items()}
        validate_context_imports(allowed)
        for dll in allowed:
            for entry in ['UnreviewedApi', '#65535']:
                bad = copy.deepcopy(allowed); bad[dll].append(entry)
                try: validate_context_imports(bad)
                except AssertionError: pass
                else: raise AssertionError('unknown API/ordinal accepted')
        bad = copy.deepcopy(allowed); bad['OTHER.DLL'] = ['Unknown']
        try: validate_context_imports(bad)
        except AssertionError: pass
        else: raise AssertionError('unknown DLL accepted')
        bad = copy.deepcopy(allowed); bad['SECUR32.DLL'].pop()
        try: validate_context_imports(bad)
        except AssertionError: pass
        else: raise AssertionError('missing LSA import accepted')
    seen = set(); evidence = {}
    for variant in ['Success','CommitFail','PostFinalizeFail']:
        path = folder/f'lab-trace-{variant}.msi'; dll = folder/f'lab-trace-{variant}.dll'
        digest = pe_check(dll, arch, profile)
        if profile == 'context':
            original = dll.read_bytes()
            pe = struct.unpack_from('<I', original, 60)[0]; optional = pe+24
            negatives = []
            bad = bytearray(original); struct.pack_into('<II', bad, optional+112+13*8, 1, 32); negatives.append(bad)
            bad = original.replace(b'LsaFreeReturnBuffer', b'XsaFreeReturnBuffer'); assert bad != original; negatives.append(bad)
            ordinal = struct.pack('<Q', (1 << 63) | 103)
            bad = original.replace(ordinal, struct.pack('<Q', (1 << 63) | 65535)); assert bad != original; negatives.append(bad)
            for bad in negatives:
                try: pe_check(dll, arch, 'context', image=bad, report=False)
                except (AssertionError, ValueError, struct.error): pass
                else: raise AssertionError('negative PE accepted')
            try: pe_check(dll, arch, 'historical', report=False)
            except AssertionError: pass
            else: raise AssertionError('historical profile accepted context DLL')
            print('PASS context DLL rejected by historical profile; PE delay/API/ordinal negatives')
        db = W.UINT(); check(msi.MsiOpenDatabaseW(str(path), None, C.byref(db)))
        try:
            table_names = rows(db, '_Tables', ['Name'])
            assert ['Component'] in table_names, 'missing Component table'
            assert ['FeatureComponents'] in table_names, 'missing FeatureComponents table'
            tables = {t:rows(db,t,c) for t,c in COLUMNS.items()}
            tables.update(summaryTemplate=summary(db,7), summaryPackage=summary(db,9), summaryFlags=summary(db,15))
        finally: msi.MsiCloseHandle(db)
        ids = {'productCode':'{'+cache_values[f'LAB_{variant}_PRODUCT:INTERNAL']+'}',
               'upgradeCode':'{'+cache_values[f'LAB_{variant}_UPGRADE:INTERNAL']+'}', 'packageCode':tables['summaryPackage']}
        assert not seen.intersection(ids.values()); seen.update(ids.values())
        validate(tables,digest,variant,arch,ids)
        path.with_suffix('.tables.json').write_text(json.dumps(tables,indent=2))
        mutations = []
        for table,row,col,value in [('CustomAction',0,1,'65'),('CustomAction',0,3,'Other'),
                ('Binary',0,1,'0'*64),('File',None,None,['payload']),
                ('InstallExecuteSequence',None,None,['Injected','1','1550'])]:
            bad = copy.deepcopy(tables)
            if row is None: bad[table].append(value)
            else: bad[table][row][col] = value
            mutations.append(bad)
        bad = copy.deepcopy(tables); bad['summaryPackage'] = '{E93DE42D-1F4F-4227-8056-1086581CCE51}'; mutations.append(bad)
        for action in ACTIONS:
            bad = copy.deepcopy(tables); next(r for r in bad['InstallExecuteSequence'] if r[0] == action)[2] = '6700'; mutations.append(bad)
        for prop in ['ProductCode','UpgradeCode']:
            bad = copy.deepcopy(tables); next(r for r in bad['Property'] if r[0] == prop)[1] = '{E93DE42D-1F4F-4227-8056-1086581CCE51}'; mutations.append(bad)
        bad = copy.deepcopy(tables); bad['_Tables'].append(['Registry']); mutations.append(bad)
        bad = copy.deepcopy(tables); bad['_Tables'].remove(['Component']); mutations.append(bad)
        bad = copy.deepcopy(tables); next(r for r in bad['_Columns'] if r[0] == 'Component')[3] = '0'; mutations.append(bad)
        bad = copy.deepcopy(tables); bad['Component'].append(['dummy'] * 6); mutations.append(bad)
        bad = copy.deepcopy(tables); bad['_Tables'].remove(['FeatureComponents']); mutations.append(bad)
        bad = copy.deepcopy(tables); next(r for r in bad['_Columns'] if r[0] == 'FeatureComponents')[3] = '0'; mutations.append(bad)
        bad = copy.deepcopy(tables); bad['FeatureComponents'].append(['dummy'] * 2); mutations.append(bad)
        for bad in mutations:
            try: validate(bad,digest,variant,arch,ids)
            except AssertionError: pass
            else: raise AssertionError('negative database accepted')
        evidence[variant] = dict(file=path.name,sha256=hashlib.sha256(path.read_bytes()).hexdigest(),dllSha256=digest,**ids)
    (folder/'lab-trace-evidence.json').write_text(json.dumps(evidence,indent=2))
    print(json.dumps(evidence,indent=2))
    print('PASS: 3 linked MSIs; fixed tables/sequence/identities/Binary/PE; 60 negative cases; LAB TRACE ONLY')
