"""只分析白名单软件事件；不连接设备、不读取照片、不把日志时间当成屏幕光学测量。"""
import argparse
import json
from pathlib import Path


def analyze(record):
    if record.get('model') != 'X2D 100C' or record.get('firmware') != '4.2.0':
        raise ValueError('仅支持明确标注的 X2D 100C / 4.2.0 记录')
    events = record['events']
    previous = -1
    for event in events:
        stamp = event['timeMs']
        if not isinstance(stamp, (float, int)) or not 0 <= stamp < 1e12 or stamp < previous:
            raise ValueError('时间戳无效或乱序；拒绝自行排序掩盖日志问题')
        previous = stamp
    starts = {}
    intervals = []
    excluded_non_capture = 0
    for event in events:
        kind, value, stamp = event['kind'], event['value'], event['timeMs']
        if kind == 'gui_state':
            if value == 'exposing':
                starts.setdefault('gui_exposing_to_liveview', stamp)
            elif value == 'liveview':
                start = starts.pop('gui_exposing_to_liveview', None)
                if start is not None:
                    intervals.append(dict(metric='gui_exposing_to_liveview', durationMs=stamp-start, startMs=start, endMs=stamp))
                start = starts.pop('exposure_finished_to_liveview', None)
                if start is not None:
                    intervals.append(dict(metric='exposure_finished_to_liveview', durationMs=stamp-start, startMs=start, endMs=stamp))
            else:
                # 自动回放或菜单不是黑屏，不能把停留时间计入。
                starts.pop('gui_exposing_to_liveview', None)
        elif kind == 'show_request':
            if value == 'ShowModeNone':
                starts.setdefault('hide_to_show_request', stamp)
            elif value in ('ShowModeGui', 'ShowModeGuiSilent'):
                start = starts.pop('hide_to_show_request', None)
                if start is not None:
                    exposure_started = any(e['kind'] == 'sequence_event' and e['value'].startswith('ExposureStarted:')
                                           and start <= e['timeMs'] <= stamp for e in events)
                    exposure_finished = any(e['kind'] == 'sequence_event' and e['value'].startswith('ExposureFinished:')
                                            and start <= e['timeMs'] <= stamp for e in events)
                    if exposure_started and exposure_finished:
                        intervals.append(dict(metric='hide_to_show_request', durationMs=stamp-start, startMs=start, endMs=stamp,
                                              captureEventsPresent=True))
                    else:
                        excluded_non_capture += 1
            else:
                starts.pop('hide_to_show_request', None)
        elif kind == 'sequence_event':
            if value.startswith('ExposureFinished:'):
                starts['exposure_finished_to_liveview'] = stamp
            elif value.startswith(('PrepareExposureStart:', 'ExposureStarted:')):
                starts.pop('exposure_finished_to_liveview', None)
    return dict(model='X2D 100C', firmware='4.2.0', verification='device_software_log_only',
                physicalBlackoutMeasured=False, physicalBlackoutMs=None,
                intervals=intervals, incompleteIntervals=len(starts),
                excludedUnconfirmedHideIntervals=excluded_non_capture,
                possiblyTruncated=record.get('possiblyTruncated', True),
                coverageGuaranteed=record.get('coverageGuaranteed', False),
                limitations=['软件状态或显示请求间隔，不代表屏幕像素变黑至恢复的实际时长。',
                             '日志分辨率为毫秒，但日志调度和屏幕刷新延迟未测量。',
                             '日志缺失、自动回放或未进入相关状态时不能给出黑屏结论。'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('events', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    result = analyze(json.loads(args.events.read_text(encoding='utf-8-sig')))
    text = json.dumps(result, ensure_ascii=False, indent=2)
    if args.output:
        args.output.write_text(text+'\n', encoding='utf-8')
    print(text)


if __name__ == '__main__':
    main()
