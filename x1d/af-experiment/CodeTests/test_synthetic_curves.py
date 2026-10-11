"""仅验证公开合成输入的可重建性及消费者所需结构；不运行 AF 仿真。"""
import json
import math
from pathlib import Path
import unittest
from generate_synthetic_curves import dataset


class SyntheticInputTests(unittest.TestCase):
    def test_checked_in_fixture_matches_generator(self):
        stored = json.loads((Path(__file__).parent/'Fixtures/SyntheticAfCurves.json').read_text(encoding='utf-8'))
        self.assertEqual(stored, dataset())

    def test_consumer_grid_and_case_count(self):
        data = dataset()
        self.assertEqual(data['sigmaPixels'], [i*.25 for i in range(97)])
        self.assertEqual(len(data['curves']), 8)
        identities = {(c['scene'], c['roi'], c['metric']) for c in data['curves']}
        self.assertEqual(len(identities), 8)
        self.assertEqual(data['cvScale'], 256)

    def test_finite_nonnegative_single_peak(self):
        for curve in dataset()['curves']:
            values = curve['values']
            self.assertEqual(len(values), 97)
            self.assertTrue(all(math.isfinite(x) and x >= 0 for x in values))
            self.assertTrue(all(a >= b for a,b in zip(values, values[1:])))
            self.assertGreater(values[0], values[-1])
            self.assertEqual(curve['originalProxyCv'], round(values[0]*256))


if __name__ == '__main__':
    unittest.main()
