"""本次固定清单清理的只读门禁回归；不导入 USB，不接触相机。"""
import ast
from pathlib import Path
import unittest


def load_unused(many):
    source = Path(__file__).with_name('manual_complete_uninstall.py')
    tree = ast.parse(source.read_text(encoding='utf8'))
    main = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == 'main')
    function = next(n for n in main.body if isinstance(n, ast.FunctionDef) and n.name == 'unused')
    services = [
        dict(service='camera-service', pid='350', start_after='52'),
        dict(service='camera-gui', pid='366', start_after='60'),
        dict(service='camera-storage', pid='393', start_after='88'),
    ]
    scope = dict(many=many, prior=dict(service_maps=services))
    exec(compile(ast.Module(body=[function], type_ignores=[]), str(source), 'exec'), scope)
    return scope['unused']


class ReferenceGateTests(unittest.TestCase):
    def test_complete_scoped_observation(self):
        commands = []
        expected = ['MATCH=1', '350', '52', 'END', '366', '60', 'END',
                    '393', '88', 'END', 'END', 'IDLE']
        def many(values):
            commands.extend(values)
            return expected
        load_unused(many)('/blackbox/x2d-private-read-abi-04-adb')
        self.assertEqual(len(commands), 12)
        self.assertTrue(all(len(c.encode('ascii')) <= 231 for c in commands))
        self.assertFalse(any('rm ' in c or 'rmdir ' in c or 'kill ' in c for c in commands))
        self.assertEqual(sum('ls -l' in c for c in commands), 3)

    def test_every_unconfirmed_field_stops(self):
        expected = ['MATCH=1', '350', '52', 'END', '366', '60', 'END',
                    '393', '88', 'END', 'END', 'IDLE']
        for index in range(len(expected)):
            for bad in ('', 'ERROR\nEND', 'USED\nEND'):
                with self.subTest(index=index, bad=bad):
                    values = list(expected)
                    values[index] = bad
                    with self.assertRaises(AssertionError):
                        load_unused(lambda _: values)('/data/local/tmp/x2d-bootstrap')

    def test_transport_failure_is_not_retried(self):
        calls = []
        def many(commands):
            calls.append(commands)
            raise RuntimeError('USB_INCOMPLETE')
        with self.assertRaisesRegex(RuntimeError, 'USB_INCOMPLETE'):
            load_unused(many)('/blackbox/x2d-native-stage')
        self.assertEqual(len(calls), 1)

    def test_batch_intent_precedes_dispatch(self):
        tree = ast.parse(Path(__file__).with_name(
            'manual_complete_uninstall.py').read_text(encoding='utf8'))
        loop = next(n for n in ast.walk(tree) if isinstance(n, ast.For)
                    and isinstance(n.target, ast.Name) and n.target.id == 'offset'
                    and any(isinstance(v, ast.Constant) and v.value == 'dispatch-'
                            for v in ast.walk(n)))
        self.assertIsInstance(loop.body[0], ast.Expr)
        self.assertEqual(loop.body[0].value.func.id, 'durable')
        self.assertEqual(loop.body[1].value.func.id, 'many')


if __name__ == '__main__':
    unittest.main()
