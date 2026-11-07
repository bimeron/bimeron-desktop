"""Offline MSI source inventory. No installer sessions; never admission evidence."""
import argparse
import ctypes as C
from ctypes import wintypes as W
import hashlib
import json
from pathlib import Path
import re
import sys
import uuid

LIMIT = 10000
TABLES = set('Property Directory Component File RemoveFile Registry RemoveRegistry ServiceInstall ServiceControl CustomAction Upgrade LaunchCondition Media ControlEvent ControlCondition EventMapping Binary'.split())

# Supported observed layouts; never inferred from the input being checked.
LAYOUTS = {
    'Binary': 'Name:11592 Data:2304',
    'Component': 'Component:11592 ComponentId:7462 Directory_:3400 Attributes:1282 Condition:7679 KeyPath:7496',
    'ControlCondition': 'Dialog_:11592 Control_:11570 Action:11570 Condition:11775',
    'ControlEvent': 'Dialog_:11592 Control_:11570 Event:11570 Argument:11775 Condition:15871 Ordering:5378',
    'CustomAction': 'Action:11592 Type:1282 Source:7496 Target:7679 ExtendedType:4356',
    'Directory': 'Directory:11592 Directory_Parent:7496 DefaultDir:4095',
    'EventMapping': 'Dialog_:11592 Control_:11570 Event:11570 Attribute:3378',
    'File': 'File:11592 Component_:3400 FileName:4095 FileSize:260 Version:7496 Language:7444 Attributes:5378 Sequence:260',
    'LaunchCondition': 'Condition:11775 Description:4095',
    'Media': 'DiskId:9474 LastSequence:260 DiskPrompt:8000 Cabinet:7679 VolumeLabel:7456 Source:7496',
    'Property': 'Property:11592 Value:3840',
    'Registry': 'Registry:11592 Root:1282 Key:4095 Name:8191 Value:7936 Component_:3400',
    'RemoveFile': 'FileKey:11592 Component_:3400 FileName:8191 DirProperty:3400 InstallMode:1282',
    'Upgrade': 'UpgradeCode:11558 VersionMin:15636 VersionMax:15636 Language:15871 Attributes:8452 Remove:7679 ActionProperty:3400',
}
for _name in 'AdminExecuteSequence AdminUISequence AdvtExecuteSequence InstallExecuteSequence InstallUISequence'.split():
    LAYOUTS[_name] = 'Action:11592 Condition:7679 Sequence:5378'

def require(ok, message):
    if not ok:
        raise ValueError(message)

def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def inspect(path):
    require(sys.platform == 'win32', 'MSI inspection requires Windows')
    require(0 < path.stat().st_size <= 1024**3, 'MSI size limit')
    before = digest(path)
    dll = C.WinDLL('msi.dll', winmode=0x800)
    signatures = {
        'MsiOpenDatabaseW': [W.LPCWSTR, W.LPCWSTR, C.POINTER(W.UINT)],
        'MsiDatabaseOpenViewW': [W.UINT, W.LPCWSTR, C.POINTER(W.UINT)],
        'MsiViewExecute': [W.UINT, W.UINT],
        'MsiViewFetch': [W.UINT, C.POINTER(W.UINT)],
        'MsiRecordGetStringW': [W.UINT, W.UINT, W.LPWSTR, C.POINTER(W.DWORD)],
        'MsiRecordReadStream': [W.UINT, W.UINT, C.c_void_p, C.POINTER(W.DWORD)],
        'MsiCloseHandle': [W.UINT],
        'MsiGetSummaryInformationW': [W.UINT, W.LPCWSTR, W.UINT, C.POINTER(W.UINT)],
        'MsiSummaryInfoGetPropertyW': [W.UINT, W.UINT, C.POINTER(W.UINT), C.POINTER(C.c_int), C.c_void_p, W.LPWSTR, C.POINTER(W.DWORD)],
    }
    for name, args in signatures.items():
        getattr(dll, name).argtypes = args
        getattr(dll, name).restype = W.UINT
    def call(name, *args):
        code = getattr(dll, name)(*args)
        require(code == 0, f'{name}: MSI error {code}')
    def rows(db, table, columns):
        require(all(re.fullmatch(r'[A-Za-z_][A-Za-z_0-9]*', s) for s in [table, *columns]), 'invalid SQL identifier')
        view = W.UINT()
        call('MsiDatabaseOpenViewW', db, 'SELECT '+','.join('`'+c+'`' for c in columns)+' FROM `'+table+'`', C.byref(view))
        result = []
        try:
            call('MsiViewExecute', view, 0)
            while True:
                record = W.UINT()
                code = dll.MsiViewFetch(view, C.byref(record))
                if code == 259:
                    break
                require(code == 0, f'fetch error {code}')
                try:
                    row = {}
                    for i, col in enumerate(columns, 1):
                        if table == 'Binary' and col == 'Data':
                            h = hashlib.sha256(); total = 0
                            while True:
                                buf = C.create_string_buffer(65536); size = W.DWORD(len(buf))
                                call('MsiRecordReadStream', record, i, buf, C.byref(size))
                                if not size.value:
                                    break
                                total += size.value
                                require(total <= 128*1024**2, 'Binary size limit')
                                h.update(buf.raw[:size.value])
                            row[col] = h.hexdigest()
                        else:
                            buf = C.create_unicode_buffer(32768); size = W.DWORD(len(buf))
                            call('MsiRecordGetStringW', record, i, buf, C.byref(size))
                            row[col] = buf.value
                    result.append(row)
                    require(len(result) <= LIMIT, 'row limit')
                finally:
                    dll.MsiCloseHandle(record)
        finally:
            dll.MsiCloseHandle(view)
        return result
    db = W.UINT()
    call('MsiOpenDatabaseW', str(path.resolve()), None, C.byref(db))
    try:
        names = sorted(r['Name'] for r in rows(db, '_Tables', ['Name']))
        require(len(names) <= 256, 'table limit')
        columns = rows(db, '_Columns', ['Table', 'Number', 'Name', 'Type'])
        tables = {}
        for name in names:
            if name in TABLES or name.endswith('Sequence'):
                cols = [r['Name'] for r in sorted(columns, key=lambda r: int(r['Number'])) if r['Table'] == name]
                tables[name] = rows(db, name, cols)
        summary = W.UINT()
        call('MsiGetSummaryInformationW', db, None, 0, C.byref(summary))
        try:
            metadata = {}
            for key, pid in [('platform', 7), ('PackageCode', 9), ('WordCount', 15)]:
                buf = C.create_unicode_buffer(32768); size = W.DWORD(len(buf)); kind = W.UINT(); number = C.c_int()
                call('MsiSummaryInfoGetPropertyW', summary, pid, C.byref(kind), C.byref(number), None, buf, C.byref(size))
                require(kind.value == (3 if key == 'WordCount' else 30), 'unsupported summary property type')
                metadata[key] = number.value if key == 'WordCount' else buf.value
        finally:
            dll.MsiCloseHandle(summary)
    finally:
        dll.MsiCloseHandle(db)
    require(digest(path) == before, 'source changed during inventory')
    return dict(schema=1, sha256=before, size=path.stat().st_size, summary=metadata, table_names=names, columns=columns, tables=tables)

def check(p):
    require(isinstance(p, dict) and type(p.get('schema')) is int and p['schema'] == 1, 'unsupported schema')
    require(set(p) == {'schema', 'sha256', 'size', 'summary', 'table_names', 'columns', 'tables'}, 'unknown projection fields')
    require(isinstance(p['columns'], list) and len(p['columns']) <= LIMIT, 'column limit')
    require(re.fullmatch('[0-9a-f]{64}', p['sha256']) is not None and type(p['size']) is int and 0 < p['size'] <= 1024**3, 'invalid source identity')
    names = p['table_names']; tables = p['tables']
    require(isinstance(names, list) and len(names) <= 256 and len(set(names)) == len(names), 'invalid table names')
    require(set(tables) == {n for n in names if n in TABLES or n.endswith('Sequence')}, 'missing or unexpected projection')
    require({'Property', 'Directory', 'Component', 'File', 'InstallExecuteSequence'} <= set(tables), 'missing required tables')
    require(isinstance(p['summary'], dict) and set(p['summary']) == {'platform', 'PackageCode', 'WordCount'}, 'invalid summary fields')
    require(type(p['summary']['WordCount']) is int and 0 <= p['summary']['WordCount'] <= 15, 'unsupported summary flags')
    column_ids = set()
    for col in p['columns']:
        require(isinstance(col, dict) and set(col) == {'Table', 'Number', 'Name', 'Type'}, 'invalid column metadata')
        require(all(isinstance(v, str) for v in col.values()), 'invalid column values')
        require(col['Table'] in names and re.fullmatch('[1-9][0-9]*', col['Number']) and re.fullmatch('[1-9][0-9]*', col['Type']), 'invalid column number/type')
        ident = (col['Table'], col['Number'])
        require(ident not in column_ids, 'duplicate column number')
        column_ids.add(ident)
    for name, rows in tables.items():
        cols = [r for r in p['columns'] if r['Table'] == name]
        require(name in LAYOUTS, 'unsupported table schema: '+name)
        layout = [item.split(':') for item in LAYOUTS[name].split()]
        require(sorted((int(c['Number']), c['Name'], c['Type']) for c in cols) == [(i, n, t) for i, (n, t) in enumerate(layout, 1)], 'unsupported column schema: '+name)
        expected = {r['Name'] for r in cols}
        require(len(expected) == len(cols) and bool(cols), 'duplicate/missing columns')
        require(isinstance(rows, list) and len(rows) <= LIMIT, 'invalid rows')
        keys = [r['Name'] for r in cols if int(r['Type']) & 8192]
        required_key = {'Property': 'Property', 'Directory': 'Directory', 'Component': 'Component', 'File': 'File'}.get(name, 'Action' if name.endswith('Sequence') else None)
        if required_key:
            require(keys == [required_key], 'unsupported primary key schema')
        require(bool(keys), 'missing primary key')
        seen = set()
        for row in rows:
            require(isinstance(row, dict) and set(row) == expected and all(isinstance(v, str) and len(v) < 32768 for v in row.values()), 'malformed row')
            for col in cols:
                value = row[col['Name']]; typ = int(col['Type'])
                require(value != '' or bool(typ & 4096), 'missing required value')
                if value and not typ & 2048:
                    require(re.fullmatch(r'-?[0-9]+', value) is not None, 'invalid integer')
                    bound = 32767 if typ & 255 == 2 else 2147483647
                    require(-bound <= int(value) <= bound, 'integer out of range')
            if name == 'Binary':
                require(re.fullmatch('[0-9a-f]{64}', row['Data']) is not None, 'invalid Binary digest')
            key = tuple(row[k] for k in keys)
            require(key not in seen, 'duplicate identifier')
            seen.add(key)
    props = {r['Property']: r['Value'] for r in tables['Property']}
    for name in ['ProductCode', 'UpgradeCode']:
        uuid.UUID(props[name].strip('{}'))
    uuid.UUID(p['summary']['PackageCode'].strip('{}'))
    require(re.fullmatch(r'\d+\.\d+\.\d+', props['ProductVersion']) is not None, 'invalid version')
    require(p['summary']['platform'].split(';')[0] in ['x64', 'Arm64'], 'unsupported architecture')
    directories = {r['Directory']: r for r in tables['Directory']}
    for key in directories:
        seen = set(); current = key
        while current:
            require(current in directories and current not in seen, 'missing/cyclic directory')
            seen.add(current); current = directories[current]['Directory_Parent']
    components = {r['Component']: r for r in tables['Component']}
    require(all(r['Directory_'] in directories for r in components.values()), 'missing component directory')
    require(all(r['Component_'] in components for r in tables['File']), 'missing file component')
    for name in ('Registry', 'RemoveFile', 'RemoveRegistry', 'ServiceInstall', 'ServiceControl'):
        require(all(r['Component_'] in components for r in tables.get(name, [])), 'missing resource component')
    files = {r['File']: r for r in tables['File']}
    registry = {r['Registry']: r for r in tables.get('Registry', [])}
    for component in components.values():
        key = component['KeyPath']
        if key:
            targets = registry if int(component['Attributes']) & 4 else files
            require(key in targets and targets[key]['Component_'] == component['Component'], 'missing component key path')
    sequences = {}
    for name, rows in tables.items():
        if name.endswith('Sequence'):
            for row in rows:
                require(-4 <= int(row['Sequence']) <= 32767, 'invalid sequence')
            sequences[name] = sorted(rows, key=lambda r: int(r['Sequence']))
    return dict(inventory_valid=True, compatibility_check='unresolved', native_eligible=False, effective_installed_state='unknown',
                identity={k: props[k] for k in ['ProductVersion', 'ProductCode', 'UpgradeCode']},
                sequences=sequences, unresolved=['All actions and conditions require review; no condition evaluation or lifecycle proof',
                'Unprojected tables: '+', '.join(sorted(set(names)-set(tables)))])

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=['inspect', 'check'])
    parser.add_argument('path', type=Path)
    args = parser.parse_args()
    try:
        if args.mode == 'inspect':
            p = inspect(args.path)
        else:
            require(args.path.stat().st_size <= 32*1024**2, 'projection size limit')
            def unique(pairs):
                result = {}
                for key, value in pairs:
                    require(key not in result, 'duplicate JSON key')
                    result[key] = value
                return result
            p = json.loads(args.path.read_text(encoding='utf-8-sig'), object_pairs_hook=unique)
        result = check(p)
        print(json.dumps(p if args.mode == 'inspect' else result, indent=2))
        return 0
    except (OSError, ValueError, KeyError, TypeError, AttributeError) as error:
        print(json.dumps(dict(inventory_valid=False, native_eligible=False, error=str(error))), file=sys.stderr)
        return 1

if __name__ == '__main__':
    sys.exit(main())
