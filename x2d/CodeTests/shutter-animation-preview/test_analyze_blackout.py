"""合成事件验证；不访问设备。"""
import unittest
from analyze_blackout import analyze


def run(events):
    return analyze(dict(model='X2D 100C', firmware='4.2.0', possiblyTruncated=False,
                        events=[dict(timeMs=t, kind=k, value=v) for t, k, v in events]))


class ProbeAnalysisTests(unittest.TestCase):
    def test_complete_capture_and_recovery_are_separate(self):
        result = run([(100, 'show_request', 'ShowModeNone'),
                      (120, 'sequence_event', 'ExposureStarted:FinishExposure'),
                      (600, 'sequence_event', 'ExposureFinished:None'),
                      (800, 'show_request', 'ShowModeGui'),
                      (850, 'gui_state', 'liveview')])
        self.assertEqual([i['durationMs'] for i in result['intervals']], [700, 250])
        self.assertIsNone(result['physicalBlackoutMs'])
        self.assertFalse(result['physicalBlackoutMeasured'])

    def test_menu_hide_is_not_capture(self):
        result = run([(100, 'show_request', 'ShowModeNone'), (200, 'gui_state', 'main_menu'),
                      (5000, 'show_request', 'ShowModeGui')])
        self.assertEqual(result['intervals'], [])
        self.assertEqual(result['excludedUnconfirmedHideIntervals'], 1)

    def test_playback_cancels_gui_interval(self):
        result = run([(100, 'gui_state', 'exposing'), (200, 'gui_state', 'browse_view'),
                      (900, 'gui_state', 'liveview')])
        self.assertEqual(result['intervals'], [])

    def test_incomplete_events_are_not_invented(self):
        self.assertEqual(run([(100, 'gui_state', 'exposing')])['incompleteIntervals'], 1)
        self.assertEqual(run([(100, 'gui_state', 'liveview')])['intervals'], [])

    def test_reversed_clock_is_rejected(self):
        with self.assertRaises(ValueError):
            run([(100, 'gui_state', 'exposing'), (99, 'gui_state', 'liveview')])

    def test_other_model_is_rejected(self):
        with self.assertRaises(ValueError):
            analyze(dict(model='X2D II', firmware='4.2.0', events=[]))

    def test_duplicate_start_preserves_first_boundary(self):
        result = run([(100, 'gui_state', 'exposing'), (110, 'gui_state', 'exposing'),
                      (600, 'gui_state', 'liveview')])
        self.assertEqual(result['intervals'][0]['durationMs'], 500)


if __name__ == '__main__':
    unittest.main()
