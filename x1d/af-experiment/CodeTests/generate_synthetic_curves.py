"""生成纯解析对焦测试曲线；不读取照片，不复现历史图像证据。"""
import argparse
import json
import math
from pathlib import Path


def dataset():
    positions = [i/4 for i in range(97)]
    curves = []
    for scene, amplitude, width in [('analytic_broad', 160, 5), ('analytic_narrow', 80, 2)]:
        for roi, multiplier in [('small', 1), ('large', 1.25)]:
            for metric, exponent in [('gradient', 1), ('laplacian', 2)]:
                values = [round(amplitude*multiplier*math.exp(-exponent*(x/width)**2/2), 12)
                          for x in positions]
                curves.append({'scene': scene, 'roi': roi, 'metric': metric, 'values': values,
                               'maxAtSigma': 0, 'originalProxyCv': round(values[0]*256)})
    return {'method': 'analytic_gaussians_no_image_input', 'cvScale': 256,
            'sigmaPixels': positions, 'curves': curves,
            'note': '字段名兼容旧测试接口；横轴是合成单位，不是实际图像像素或光学离焦。'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.write_text(json.dumps(dataset(), ensure_ascii=False, indent=2)+'\n',
                           encoding='utf-8', newline='\n')
