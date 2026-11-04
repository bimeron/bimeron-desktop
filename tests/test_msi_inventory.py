import copy
import importlib.util
import pathlib
import unittest
import xml.etree.ElementTree as ET

ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('inventory', ROOT / 'scripts/msi_inventory.py')
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)

def fixture():
    import json
    return json.loads((ROOT / 'tests/fixtures/msi-inventory.json').read_text())

class InventoryTests(unittest.TestCase):
    def test_projection(self):
        p = fixture(); result = m.check(p)
        self.assertTrue(result['inventory_valid'])
        self.assertFalse(result['native_eligible'])
        self.assertEqual(result['sequences']['InstallExecuteSequence'][0]['Sequence'], '25')
        self.assertTrue(result['unresolved'])

    def test_invalid(self):
        mutations = [
            lambda p: p.update(schema=2),
            lambda p: p.update(sha256='bad'),
            lambda p: p['summary'].update(platform='Intel;1033'),
            lambda p: p['summary'].pop('PackageCode'),
            lambda p: p['tables']['Property'].append(p['tables']['Property'][0].copy()),
            lambda p: p['tables']['File'][0].update(Component_='missing'),
            lambda p: p['tables']['Component'][0].update(Directory_='missing'),
            lambda p: p['tables']['Directory'][0].update(Directory_Parent=p['tables']['Directory'][0]['Directory']),
            lambda p: p['tables']['InstallExecuteSequence'][0].update(Sequence='bad'),
            lambda p: p['tables'].pop('File'),
            lambda p: p['tables']['File'][0].update(Extra='bad'),
            lambda p: p['columns'].append(p['columns'][0].copy()),
        ]
        for mutation in mutations:
            with self.subTest(mutation=mutation):
                p = fixture(); mutation(p)
                with self.assertRaises((ValueError, KeyError, TypeError)):
                    m.check(p)

    def test_unknown_review(self):
        p = fixture(); p['table_names'].append('UnknownTable')
        p['tables']['InstallExecuteSequence'][0]['Action'] = 'UnknownAction'
        result = m.check(p)
        self.assertIn('UnknownTable', result['unresolved'][1])
        self.assertFalse(result['native_eligible'])

    def test_review_regressions(self):
        mutations = [
            lambda p: p['summary'].update(extra=1),
            lambda p: p['summary'].pop('WordCount'),
            lambda p: p['summary'].update(WordCount='2'),
            lambda p: p['tables']['Binary'][0].update(Data='wrong'),
            lambda p: p['columns'][0].update(Number='garbage'),
            lambda p: p['columns'][0].update(Type='1'),
            lambda p: p['columns'][0].update(Type='garbage'),
        ]
        for mutate in mutations:
            p = fixture(); mutate(p)
            with self.assertRaises((ValueError, KeyError, TypeError)):
                m.check(p)
        for table, column in [('File', 'FileName'), ('Component', 'Attributes')]:
            p = fixture()
            p['columns'] = [c for c in p['columns'] if (c['Table'], c['Name']) != (table, column)]
            for row in p['tables'][table]:
                del row[column]
            with self.assertRaises(ValueError):
                m.check(p)

    def test_current_definitions(self):
        tree = ET.parse(ROOT / 'packaging/windows/options.wxs')
        ns = {'w': 'http://schemas.microsoft.com/wix/2006/wi'}
        actions = tree.findall('.//w:CustomAction', ns)
        self.assertEqual([a.attrib['Id'] for a in actions], ['LaunchBimeron'])
        self.assertEqual(actions[0].attrib['ExeCommand'], '"[INSTALL_ROOT]bimeron-desktop.exe"')
        publish = tree.find('.//w:Publish[@Event="DoAction"]', ns)
        self.assertEqual(publish.attrib['Value'], 'LaunchBimeron')
        self.assertIn('NOT Installed', publish.text)
        self.assertEqual(tree.findall('.//w:InstallExecuteSequence', ns), [])
        cmake = (ROOT / 'CMakeLists.txt').read_text()
        self.assertIn('E93DE42D-1F4F-4227-8056-1086581CCE51', cmake)
        self.assertIn('set(CPACK_WIX_UI_REF "BimeronUI")', cmake)
        self.assertNotIn('foreground-prototype', cmake)

if __name__ == '__main__':
    unittest.main()
