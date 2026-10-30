"""Read-only MSI/PE fixture proof. Never opens an installer session or host registry."""
import ctypes as C
from ctypes import wintypes as W
import copy
import hashlib
import json
from pathlib import Path
import struct
import sys

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

def eligible_input(e):
    # Explicit runtime-observation model; not MSI properties as attestation.
    return (e == {'rollback': True, 'patches': [], 'transforms': [],
                  'maintenance': 'fresh-fixture', 'mutationProperties': {}})

def validate(t, digest):
    assert {r[0] for r in t['_Tables']} == {'AdminExecuteSequence','AdminUISequence','AdvtExecuteSequence','Binary','CustomAction','Directory','Feature','File','InstallExecuteSequence','InstallUISequence','LaunchCondition','Media','Property','Upgrade','_Validation'}
    assert t['File'] == [] and t['Media'] == []
    for sequence in ['AdminExecuteSequence','AdminUISequence','AdvtExecuteSequence','InstallUISequence']:
        assert all(r[0] not in {'ForegroundPrototypeGate', 'ForegroundPostFinalizeGate'} for r in t[sequence])
    assert t['CustomAction'] == sorted([
        ['ForegroundPrototypeGate', '1', 'ForegroundPrototypeBinary', 'ForegroundPrototype'],
        ['ForegroundPostFinalizeGate', '1', 'ForegroundPrototypeBinary', 'ForegroundPostFinalize']])
    assert t['Binary'] == [['ForegroundPrototypeBinary', digest]]
    seq = {r[0]: (r[1], int(r[2])) for r in t['InstallExecuteSequence']}
    assert seq['ForegroundPrototypeGate'][0] == seq['ForegroundPostFinalizeGate'][0] == '1'
    assert seq['ForegroundPrototypeGate'][1] < seq['InstallInitialize'][1]
    assert seq['InstallInitialize'][1] < seq['InstallExecute'][1] < seq['RemoveExistingProducts'][1] < seq['InstallFinalize'][1]
    assert seq['InstallFinalize'][1] < seq['ForegroundPostFinalizeGate'][1]
    assert [r[0] for r in t['InstallExecuteSequence'] if int(r[2]) > seq['InstallFinalize'][1]] == ['ForegroundPostFinalizeGate']
    assert t['Directory'] == [['TARGETDIR', '', 'SourceDir']]
    assert t['Feature'] == [['EmptyFixture', '', 'Fixture', '', '2', '1', '', '0']]
    expected = {
        'CostFinalize': ('', 1000), 'CostInitialize': ('', 800), 'FileCost': ('', 900),
        'FindRelatedProducts': ('', 25), 'ForegroundPostFinalizeGate': ('1', 6601),
        'ForegroundPrototypeGate': ('1', 1499), 'InstallExecute': ('NOT Installed', 6500),
        'InstallFinalize': ('', 6600), 'InstallInitialize': ('', 1500),
        'InstallValidate': ('', 1400), 'LaunchConditions': ('', 100),
        'MigrateFeatureStates': ('', 1200), 'ProcessComponents': ('', 1600),
        'PublishFeatures': ('', 6300), 'PublishProduct': ('', 6400),
        'RegisterProduct': ('', 6100), 'RegisterUser': ('', 6000),
        'RemoveExistingProducts': ('', 6501), 'UnpublishFeatures': ('', 1800),
        'ValidateProductID': ('', 700)}
    assert seq == expected  # No unknown or moved resource actions/conditions.
    props = dict(t['Property'])
    assert set(props) == {'ALLUSERS','Manufacturer','ProductCode','ProductLanguage',
                          'ProductName','ProductVersion','SecureCustomProperties','UpgradeCode'}
    assert props['Manufacturer'] == 'Bimeron' and props['ProductLanguage'] == '1033'
    assert props['SecureCustomProperties'] == 'WIX_DOWNGRADE_DETECTED;WIX_UPGRADE_DETECTED'
    assert props['UpgradeCode'] == '{A8945D41-FEAD-441D-AC52-FB832E9E6BE9}'
    assert props['ProductName'] == 'Bimeron foreground feasibility fixture'
    assert props['ProductVersion'] == '0.0.1' and props['ALLUSERS'] == '1'
    assert not set(props) & {'TRANSFORMS', 'PATCH', 'DISABLEROLLBACK', 'REINSTALL', 'REMOVE', 'ADDLOCAL'}

def pe_check(path, arch):
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
    assert {'ForegroundPrototype','ForegroundPostFinalize'} <= set(exports)
    # Reject all delay imports, not merely unfamiliar ones.
    assert struct.unpack_from('<II', b, optional+112+13*8) == (0, 0)
    imports = []; rva, size = struct.unpack_from('<II', b, optional+112+8)
    o = offset(rva)
    while any(b[o:o+20]):
        name = offset(struct.unpack_from('<I', b, o+12)[0])
        imports.append(b[name:b.index(0, name)].decode().upper()); o += 20
    assert set(imports) <= {'KERNEL32.DLL', 'ADVAPI32.DLL', 'MSI.DLL'}
    print('fixed system imports; no delay imports:', imports)
    return hashlib.sha256(b).hexdigest()

if __name__ == '__main__':
    path, dll, arch = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve(), sys.argv[3]
    digest = pe_check(dll, arch)
    db = W.UINT(); check(msi.MsiOpenDatabaseW(str(path), None, C.byref(db))) # MSIDBOPEN_READONLY
    columns = {'_Tables':['Name'], 'File':['File'], 'Media':['DiskId'],
               **{name:['Action','Condition','Sequence'] for name in ['AdminExecuteSequence','AdminUISequence','AdvtExecuteSequence','InstallUISequence']}, 'CustomAction':['Action','Type','Source','Target'], 'Binary':['Name','Data'],
               'InstallExecuteSequence':['Action','Condition','Sequence'],
               'Directory':['Directory','Directory_Parent','DefaultDir'],
               'Feature':['Feature','Feature_Parent','Title','Description','Display','Level','Directory_','Attributes'],
               'Property':['Property','Value']}
    try: tables = {t:rows(db, t, c) for t,c in columns.items()}
    finally: msi.MsiCloseHandle(db)
    path.with_suffix('.tables.json').write_text(json.dumps(tables, indent=2))
    validate(tables, digest)
    for table, row, column, value in [('CustomAction',0,1,'65'), ('CustomAction',0,1,'129'),
            ('CustomAction',0,2,'InstalledHelper'), ('CustomAction',0,3,'OtherExport'), ('Binary',0,1,'bad'),
            ('Directory',0,2,'other'), ('Feature',0,5,'0')]:
        bad = copy.deepcopy(tables); bad[table][row][column] = value
        try: validate(bad, digest)
        except AssertionError: pass
        else: raise AssertionError('unsafe effective database accepted')
    for name in ['ForegroundPrototypeGate', 'ForegroundPostFinalizeGate', 'RemoveExistingProducts']:
        bad = copy.deepcopy(tables)
        row = next(r for r in bad['InstallExecuteSequence'] if r[0] == name)
        row[1] = '0' if 'Gate' in name else row[1]
        row[2] = '6700'
        try: validate(bad, digest)
        except AssertionError: pass
        else: raise AssertionError('unsafe effective sequence accepted')
    for name in ['ForegroundPrototypeGate','ForegroundPostFinalizeGate','InstallExecute','RemoveExistingProducts']:
        bad = copy.deepcopy(tables)
        next(r for r in bad['InstallExecuteSequence'] if r[0] == name)[1] = '0'
        try: validate(bad, digest)
        except AssertionError: pass
        else: raise AssertionError('unsafe condition accepted')
    good = {'rollback':True,'patches':[],'transforms':[],'maintenance':'fresh-fixture','mutationProperties':{}}
    assert eligible_input(good) and not eligible_input({})
    for key,value in [('rollback',False),('patches',['x.msp']),('transforms',['x.mst']),
                      ('maintenance','repair'),('mutationProperties',{'INSTALLDIR':'C:\\temp'})]:
        bad = dict(good); bad[key] = value; assert not eligible_input(bad)
    print('PASS: read-only linked MSI, Binary digest, PE imports and negative effective-input fixtures; production trust unavailable')
