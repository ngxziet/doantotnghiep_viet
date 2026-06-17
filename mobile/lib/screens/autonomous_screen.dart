import 'dart:async';
import 'package:flutter/material.dart';

import '../models/robot_state.dart';
import '../services/udp_service.dart';

/// Màn hình điều khiển chế độ tự hành né vật cản.
///
/// Gửi lệnh start/stop tới firmware và hiển thị telemetry (trạng thái, khoảng
/// cách vật cản, hướng, vị trí) theo thời gian thực.
class AutonomousScreen extends StatefulWidget {
  final ITransportService transport;

  const AutonomousScreen({super.key, required this.transport});

  @override
  State<AutonomousScreen> createState() => _AutonomousScreenState();
}

class _AutonomousScreenState extends State<AutonomousScreen> {
  RobotState _state = RobotState.empty();
  StreamSubscription<RobotState>? _sub;
  int _seq = 5000;
  bool _running = false;

  @override
  void initState() {
    super.initState();
    _sub = widget.transport.telemetryStream.listen((state) {
      setState(() => _state = state);
    });
  }

  @override
  void dispose() {
    _sub?.cancel();
    super.dispose();
  }

  Future<void> _start() async {
    _seq++;
    await widget.transport.sendAutonomousCommand('start', _seq);
    if (!mounted) return;
    setState(() => _running = true);
  }

  Future<void> _stop() async {
    _seq++;
    await widget.transport.sendAutonomousCommand('stop', _seq);
    if (!mounted) return;
    setState(() => _running = false);
  }

  @override
  Widget build(BuildContext context) {
    final p = _state.pose;
    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        Card(
          color: _running ? Colors.green.withValues(alpha: 0.12) : null,
          child: Padding(
            padding: const EdgeInsets.all(16),
            child: Row(
              children: [
                Icon(
                  _running ? Icons.smart_toy : Icons.smart_toy_outlined,
                  color: _running ? Colors.green : Colors.grey,
                  size: 32,
                ),
                const SizedBox(width: 12),
                Text(
                  _running ? 'Tu hanh: DANG CHAY' : 'Tu hanh: DA DUNG',
                  style: Theme.of(context).textTheme.titleMedium,
                ),
              ],
            ),
          ),
        ),
        const SizedBox(height: 12),
        _Metric(label: 'Status', value: _state.status.replaceAll('_', ' ')),
        _Metric(label: 'Obstacle', value: _formatObstacle()),
        _Metric(
            label: 'Heading',
            value: '${_state.headingDeg.toStringAsFixed(1)} deg'),
        _Metric(label: 'X', value: '${p.x.toStringAsFixed(3)} m'),
        _Metric(label: 'Y', value: '${p.y.toStringAsFixed(3)} m'),
        _Metric(label: 'IMU', value: _state.imuConnected ? 'OK' : 'Missing'),
        const Divider(height: 32),
        SizedBox(
          height: 64,
          child: FilledButton.icon(
            style: FilledButton.styleFrom(
              backgroundColor: Colors.green.withValues(alpha: 0.85),
            ),
            onPressed: _running ? null : _start,
            icon: const Icon(Icons.play_arrow),
            label: const Text('Start tu hanh'),
          ),
        ),
        const SizedBox(height: 12),
        SizedBox(
          height: 64,
          child: FilledButton.icon(
            style: FilledButton.styleFrom(
              backgroundColor: Colors.red.withValues(alpha: 0.85),
            ),
            onPressed: _stop,
            icon: const Icon(Icons.stop),
            label: const Text('Stop'),
          ),
        ),
      ],
    );
  }

  String _formatObstacle() {
    final dist = _state.obstacleDistCm;
    return dist < 0 ? 'No reading' : '${dist.toStringAsFixed(1)} cm';
  }
}

class _Metric extends StatelessWidget {
  final String label;
  final String value;

  const _Metric({required this.label, required this.value});

  @override
  Widget build(BuildContext context) {
    return ListTile(
      contentPadding: EdgeInsets.zero,
      title: Text(label),
      trailing: Text(value, style: Theme.of(context).textTheme.titleMedium),
    );
  }
}
