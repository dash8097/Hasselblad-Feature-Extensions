"""从用户提供的本地素材生成机内 WAV；不下载、不连接设备。依赖 numpy、soundfile。"""
import argparse
import hashlib
import json
from pathlib import Path
import wave
import numpy as np
import soundfile as sf

D = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path, help='自行取得且有权使用的本地音频')
    args = parser.parse_args()
    samples, rate = sf.read(args.source, dtype='float32', always_2d=True)
    if not 0 < len(samples) <= rate*5 or samples.shape[1] not in (1, 2):
        raise ValueError('仅接受五秒以内单声道或双声道素材')
    if not np.isfinite(samples).all():
        raise ValueError('音频样本非有限值')
    frames = round(len(samples)*48000/rate)
    position = np.arange(frames)*rate/48000
    resampled = np.column_stack([np.interp(position, np.arange(len(samples)), samples[:, c])
                                 for c in range(samples.shape[1])])
    if resampled.shape[1] == 1:
        resampled = np.repeat(resampled, 2, axis=1)
    # 保持原振幅；不自动归一化或限幅，超范围就停止。
    # 与本轮 libsndfile PCM16 写出保持同一量化约定。
    quantized = np.floor(resampled*32768)
    if np.any(quantized < -32768) or np.any(quantized > 32767):
        raise ValueError('素材超出 PCM16 范围，请先处理素材')
    pcm = quantized.astype('<i2')
    out = D/'outputs'
    out.mkdir(exist_ok=True)
    path = out/'ciallo-48k-full.wav'
    with wave.open(str(path), 'wb') as wav:
        wav.setnchannels(2); wav.setsampwidth(2); wav.setframerate(48000)
        wav.writeframes(pcm.tobytes())
    peak = float(np.abs(pcm.astype(np.int32)).max())/32768
    report = dict(rate=48000, channels=2, frames=frames, samplePeak=peak,
                  samplePeakDbfs=float(20*np.log10(peak)) if peak else None,
                  fullScaleSamples=int(np.count_nonzero((pcm == -32768) | (pcm == 32767))),
                  sha256=hashlib.sha256(path.read_bytes()).hexdigest())
    (out/'audio-preparation.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
