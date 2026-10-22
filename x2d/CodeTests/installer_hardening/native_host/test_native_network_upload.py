"""离线 USB 网络上传故障测试；不连接相机。"""
import hashlib
import unittest
from unittest.mock import Mock, patch
import native_network_upload as net

DATA = b'x' * 150000
DIGEST = hashlib.sha256(DATA).hexdigest()


class Tests(unittest.TestCase):
    def setUp(self):
        self.host = Mock()
        self.host.log = []
        self.host.file_state.side_effect = ['ABSENT', 'ABSENT', DIGEST]
        self.host.one.side_effect = ['FREE', '', '0']
        self.conn = Mock()
        self.conn.recv.return_value = b''
        self.factory = patch.object(net.socket, 'socket', return_value=self.conn)
        self.factory.start()
        self.addCleanup(self.factory.stop)

    def run_upload(self):
        return net.upload(self.host, DATA, net.STAGE, 'z12345678', DIGEST)

    def test_success(self):
        self.assertTrue(self.run_upload())
        self.assertEqual(b''.join(c.args[0] for c in self.conn.sendall.call_args_list), DATA)
        self.assertEqual(self.host.one.call_count, 3)
        self.assertTrue(self.host.log)
        self.conn.close.assert_called_once()

    def test_no_interface_no_device_calls(self):
        self.conn.bind.side_effect = OSError('no interface')
        self.assertFalse(self.run_upload())
        self.assertEqual(self.host.mock_calls, [])

    def test_reuse_verified_source(self):
        self.host.file_state.side_effect = [DIGEST]
        self.assertTrue(self.run_upload())
        self.host.one.assert_not_called()
        self.conn.connect.assert_not_called()

    def test_partial_source_refused(self):
        self.host.file_state.side_effect = ['bad']
        with self.assertRaises(RuntimeError): self.run_upload()
        self.host.one.assert_not_called()

    def test_pending_receiver_refused(self):
        self.host.file_state.side_effect = ['ABSENT', 'bad']
        with self.assertRaises(RuntimeError): self.run_upload()
        self.host.one.assert_not_called()

    def test_busy_port_not_dispatched(self):
        self.host.one.side_effect = ['BUSY']
        with self.assertRaises(RuntimeError): self.run_upload()
        self.assertEqual(self.host.one.call_count, 1)

    def test_send_failure_not_replayed(self):
        self.conn.sendall.side_effect = TimeoutError()
        with self.assertRaises(TimeoutError): self.run_upload()
        self.assertEqual(self.conn.sendall.call_count, 1)
        self.assertEqual(self.host.one.call_count, 2)

    def test_dispatch_timeout_not_replayed(self):
        self.host.one.side_effect = ['FREE', TimeoutError()]
        with self.assertRaises(TimeoutError): self.run_upload()
        self.conn.connect.assert_not_called()

    def test_wrong_hash(self):
        self.host.file_state.side_effect = ['ABSENT', 'ABSENT', 'bad']
        with self.assertRaises(RuntimeError): self.run_upload()

    def test_connection_refusal_only_retries_connect(self):
        self.conn.connect.side_effect = [ConnectionRefusedError(), None]
        with patch.object(net.time, 'sleep'):
            self.assertTrue(self.run_upload())
        self.assertEqual(self.conn.connect.call_count, 2)
        self.assertEqual(self.host.one.call_count, 3)
        self.assertEqual(b''.join(c.args[0] for c in self.conn.sendall.call_args_list), DATA)

    def test_connection_timeout_not_replayed(self):
        self.conn.connect.side_effect = TimeoutError()
        with self.assertRaises(TimeoutError): self.run_upload()
        self.assertEqual(self.conn.connect.call_count, 1)
        self.conn.sendall.assert_not_called()

    def test_missing_receipt(self):
        self.host.one.side_effect = ['FREE', ''] + ['WAIT'] * 20
        with patch.object(net.time, 'sleep'):
            with self.assertRaises(RuntimeError): self.run_upload()
        self.assertEqual(self.host.one.call_count, 22)

    def test_receiver_failure(self):
        self.host.one.side_effect = ['FREE', '', '124']
        with self.assertRaises(RuntimeError): self.run_upload()

    def test_unexpected_reply(self):
        self.conn.recv.return_value = b'unexpected'
        with self.assertRaises(RuntimeError): self.run_upload()

    def test_invalid_inputs(self):
        for stage, token, digest in [(net.STAGE, 'z;bad', DIGEST), ('/tmp', 'z12345678', DIGEST), (net.STAGE, 'z12345678', 'bad')]:
            with self.assertRaises(ValueError): net.upload(self.host, DATA, stage, token, digest)
        self.host.one.assert_not_called()

    def test_command_bounded_noclobber(self):
        for token in ('z12345678', 'native-install'):
            command = net.receiver_command(token)
            self.assertLessEqual(len(command.encode('ascii')), 231)
            self.assertIn('set -C;', command)
            self.assertIn('timeout -k 1 30', command)

    def test_complete_install_archive(self):
        self.assertTrue(net.upload(self.host, DATA, net.STAGE, 'native-install', DIGEST))
        self.assertEqual(b''.join(c.args[0] for c in self.conn.sendall.call_args_list), DATA)
        self.assertIn('native-install.gz', self.host.one.call_args_list[1].args[0])


if __name__ == '__main__': unittest.main()
