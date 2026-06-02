import 'dart:async';
import 'package:flutter/material.dart';

import '../models/robot_state.dart';
import '../services/udp_service.dart';
import 'map_status_widgets.dart';

class ConnectionTestScreen extends StatefulWidget {
  final ITransportService transport;

  const ConnectionTestScreen({super.key, required this.transport});

  @override
  State<ConnectionTestScreen> createState() => _ConnectionTestScreenState();
}

class _ConnectionTestScreenState extends State<ConnectionTestScreen> {
  RobotState _state = RobotState.empty();
  StreamSubscription<RobotState>? _sub;
  Timer? _refreshTimer;
  DateTime? _lastPacketAt;
  int _packetCount = 0;
  int _seq = 1000;
  String _lastAction = 'Waiting for telemetry';

  bool get _connected =>
      _lastPacketAt != null &&
      DateTime.now().difference(_lastPacketAt!).inSeconds < 3;

  @override
  void initState() {
    super.initState();
    _sub = widget.transport.telemetryStream.listen((state) {
      setState(() {
        _state = state;
        _packetCount++;
        _lastPacketAt = DateTime.now();
      });
    }, onError: (Object e) {
      setState(() => _lastAction = 'UDP error: $e');
    });
    _refreshTimer = Timer.periodic(
      const Duration(seconds: 1),
      (_) {
        if (mounted) setState(() {});
      },
    );
  }

  @override
  void dispose() {
    _sub?.cancel();
    _refreshTimer?.cancel();
    super.dispose();
  }

  Future<void> _sendPing() async {
    _seq++;
    try {
      await widget.transport.sendWaypoints(const [], _seq);
      if (!mounted) return;
      setState(() => _lastAction = 'Ping sent: seq $_seq');
    } catch (e) {
      if (!mounted) return;
      setState(() => _lastAction = 'Ping failed: $e');
    }
  }

  Future<void> _sendCalibration(String mode, String label) async {
    _seq++;
    try {
      await widget.transport.sendCalibrationCommand(mode, _seq);
      if (!mounted) return;
      setState(() => _lastAction = '$label sent: seq $_seq');
    } catch (e) {
      if (!mounted) return;
      setState(() => _lastAction = '$label failed: $e');
    }
  }

  @override
  Widget build(BuildContext context) {
    final last = _lastPacketAt == null
        ? 'No packet yet'
        : '${DateTime.now().difference(_lastPacketAt!).inMilliseconds} ms ago';
    final mpuStatus = _state.imuConnected ? 'Connected' : 'Missing';

    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        ConnectionIndicator(isConnected: _connected),
        const SizedBox(height: 16),
        _InfoTile(label: 'ESP32 AP', value: 'RobotCar / 192.168.4.1'),
        _InfoTile(label: 'Telemetry port', value: '4210'),
        _InfoTile(label: 'Command port', value: '4211'),
        _InfoTile(label: 'Packets received', value: '$_packetCount'),
        _InfoTile(label: 'Last packet', value: last),
        _InfoTile(label: 'Status', value: _state.status),
        _InfoTile(label: 'MPU6050', value: mpuStatus),
        _InfoTile(
            label: 'Heading',
            value: '${_state.headingDeg.toStringAsFixed(1)} deg'),
        _InfoTile(
            label: 'Node progress',
            value: '${_state.nodeIndex}/${_state.routeSize}'),
        _InfoTile(
            label: 'Calibration', value: _state.calibrated ? 'OK' : 'Needed'),
        const SizedBox(height: 12),
        FilledButton.icon(
          onPressed: _sendPing,
          icon: const Icon(Icons.send),
          label: const Text('Send Ping'),
        ),
        const SizedBox(height: 8),
        FilledButton.tonalIcon(
          onPressed: () => _sendCalibration('imu', 'IMU calibration'),
          icon: const Icon(Icons.explore),
          label: const Text('Calibrate IMU'),
        ),
        const SizedBox(height: 8),
        FilledButton.tonalIcon(
          onPressed: () =>
              _sendCalibration('motor_balance', 'Motor balance reset'),
          icon: const Icon(Icons.tune),
          label: const Text('Reset Motor Auto-Tune'),
        ),
        const SizedBox(height: 8),
        FilledButton.tonalIcon(
          onPressed: () => _sendCalibration('encoder', 'Encoder scale reset'),
          icon: const Icon(Icons.straighten),
          label: const Text('Reset Encoder Scale'),
        ),
        const SizedBox(height: 8),
        Text(_lastAction, style: Theme.of(context).textTheme.bodySmall),
      ],
    );
  }
}

class _InfoTile extends StatelessWidget {
  final String label;
  final String value;

  const _InfoTile({required this.label, required this.value});

  @override
  Widget build(BuildContext context) {
    return ListTile(
      contentPadding: EdgeInsets.zero,
      title: Text(label),
      trailing: Text(value, textAlign: TextAlign.right),
    );
  }
}
