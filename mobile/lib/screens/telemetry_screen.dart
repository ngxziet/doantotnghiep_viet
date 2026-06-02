import 'dart:async';
import 'package:flutter/material.dart';

import '../models/robot_state.dart';
import '../services/udp_service.dart';

class TelemetryScreen extends StatefulWidget {
  final ITransportService transport;

  const TelemetryScreen({super.key, required this.transport});

  @override
  State<TelemetryScreen> createState() => _TelemetryScreenState();
}

class _TelemetryScreenState extends State<TelemetryScreen> {
  RobotState _state = RobotState.empty();
  StreamSubscription<RobotState>? _sub;
  int _speed = 130;
  int _seq = 3000;
  String _lastCommand = 'stop';

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

  Future<void> _send(String command) async {
    _seq++;
    await widget.transport.sendManualCommand(command, _speed, _seq);
    if (!mounted) return;
    setState(() => _lastCommand = command);
  }

  @override
  Widget build(BuildContext context) {
    final p = _state.pose;
    final heading = _state.headingDeg;
    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        _Metric(label: 'X', value: '${p.x.toStringAsFixed(3)} m'),
        _Metric(label: 'Y', value: '${p.y.toStringAsFixed(3)} m'),
        _Metric(label: 'Heading', value: '${heading.toStringAsFixed(1)} deg'),
        _Metric(
            label: 'Node', value: '${_state.nodeIndex}/${_state.routeSize}'),
        _Metric(
            label: 'Calibration', value: _state.calibrated ? 'OK' : 'Needed'),
        _Metric(label: 'Obstacle', value: _formatObstacle()),
        _Metric(label: 'Status', value: _state.status.replaceAll('_', ' ')),
        const Divider(height: 32),
        Row(
          children: [
            const Icon(Icons.speed),
            Expanded(
              child: Slider(
                value: _speed.toDouble(),
                min: 60,
                max: 220,
                divisions: 16,
                label: '$_speed',
                onChanged: (value) => setState(() => _speed = value.round()),
              ),
            ),
            SizedBox(width: 44, child: Text('$_speed')),
          ],
        ),
        const SizedBox(height: 16),
        Text('Test tung banh', style: Theme.of(context).textTheme.titleMedium),
        const SizedBox(height: 12),
        _WheelTestRow(
          title: 'Banh trai',
          forwardCommand: 'left_forward',
          backwardCommand: 'left_backward',
          onSend: _send,
        ),
        const SizedBox(height: 12),
        _WheelTestRow(
          title: 'Banh phai',
          forwardCommand: 'right_forward',
          backwardCommand: 'right_backward',
          onSend: _send,
        ),
        const SizedBox(height: 20),
        SizedBox(
          height: 56,
          child: FilledButton.icon(
            style: FilledButton.styleFrom(
              backgroundColor: Colors.red.withValues(alpha: 0.22),
              foregroundColor: Colors.red,
            ),
            onPressed: () => _send('stop'),
            icon: const Icon(Icons.stop),
            label: const Text('Stop'),
          ),
        ),
        const SizedBox(height: 16),
        Text('Last command: $_lastCommand'),
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

class _WheelTestRow extends StatelessWidget {
  final String title;
  final String forwardCommand;
  final String backwardCommand;
  final Future<void> Function(String command) onSend;

  const _WheelTestRow({
    required this.title,
    required this.forwardCommand,
    required this.backwardCommand,
    required this.onSend,
  });

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(title),
        const SizedBox(height: 8),
        Row(
          children: [
            Expanded(
              child: SizedBox(
                height: 56,
                child: FilledButton.icon(
                  onPressed: () => onSend(forwardCommand),
                  icon: const Icon(Icons.keyboard_arrow_up),
                  label: const Text('Tien'),
                ),
              ),
            ),
            const SizedBox(width: 12),
            Expanded(
              child: SizedBox(
                height: 56,
                child: FilledButton.icon(
                  onPressed: () => onSend(backwardCommand),
                  icon: const Icon(Icons.keyboard_arrow_down),
                  label: const Text('Lui'),
                ),
              ),
            ),
          ],
        ),
      ],
    );
  }
}
