import 'package:flutter/material.dart';

import '../services/udp_service.dart';

class ManualControlScreen extends StatefulWidget {
  final ITransportService transport;

  const ManualControlScreen({super.key, required this.transport});

  @override
  State<ManualControlScreen> createState() => _ManualControlScreenState();
}

class _ManualControlScreenState extends State<ManualControlScreen> {
  int _speed = 180;
  int _seq = 2000;
  String _lastCommand = 'stop';

  Future<void> _sendStep(String action) async {
    _seq++;
    try {
      await widget.transport.sendStepCommand(action, _seq);
      if (!mounted) return;
      setState(() => _lastCommand = 'step $action');
    } catch (e) {
      if (!mounted) return;
      setState(() => _lastCommand = 'Error: $e');
    }
  }

  Future<void> _send(String command) async {
    _seq++;
    try {
      await widget.transport.sendManualCommand(command, _speed, _seq);
      if (!mounted) return;
      setState(() => _lastCommand = command);
    } catch (e) {
      if (!mounted) return;
      setState(() => _lastCommand = 'Error: $e');
    }
  }

  @override
  Widget build(BuildContext context) {
    return SafeArea(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          children: [
            StreamBuilder<bool>(
              stream: widget.transport.connectionStream,
              initialData: widget.transport.isConnected,
              builder: (context, snapshot) {
                final connected = snapshot.data ?? false;
                if (connected) return const SizedBox.shrink();
                return Container(
                  width: double.infinity,
                  padding:
                      const EdgeInsets.symmetric(vertical: 8, horizontal: 12),
                  margin: const EdgeInsets.only(bottom: 8),
                  decoration: BoxDecoration(
                    color: Colors.orange.shade100,
                    borderRadius: BorderRadius.circular(8),
                  ),
                  child: const Row(
                    children: [
                      Icon(Icons.wifi_off, color: Colors.deepOrange, size: 20),
                      SizedBox(width: 8),
                      Expanded(
                        child: Text(
                          'M\u1ea5t k\u1ebft n\u1ed1i v\u1edbi xe — ki\u1ec3m tra WiFi RobotCar',
                          style:
                              TextStyle(color: Colors.deepOrange, fontSize: 13),
                        ),
                      ),
                    ],
                  ),
                );
              },
            ),
            Row(
              children: [
                const Icon(Icons.speed),
                Expanded(
                  child: Slider(
                    value: _speed.toDouble(),
                    min: 120,
                    max: 220,
                    divisions: 16,
                    label: '$_speed',
                    onChanged: (value) =>
                        setState(() => _speed = value.round()),
                  ),
                ),
                SizedBox(width: 44, child: Text('$_speed')),
              ],
            ),
            const Spacer(),
            _DriveButton(
              icon: Icons.keyboard_arrow_up,
              label: 'Forward',
              onPressed: () => _sendStep('forward'),
            ),
            Row(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                _DriveButton(
                  icon: Icons.keyboard_arrow_left,
                  label: 'Left',
                  onPressed: () => _sendStep('left'),
                ),
                const SizedBox(width: 16),
                _DriveButton(
                  icon: Icons.stop,
                  label: 'Stop',
                  tone: Colors.red,
                  onPressed: () => _send('stop'),
                ),
                const SizedBox(width: 16),
                _DriveButton(
                  icon: Icons.keyboard_arrow_right,
                  label: 'Right',
                  onPressed: () => _sendStep('right'),
                ),
              ],
            ),
            _DriveButton(
              icon: Icons.keyboard_arrow_down,
              label: 'Backward',
              onPressed: () => _sendStep('backward'),
            ),
            const Spacer(),
            Text('Last command: $_lastCommand'),
          ],
        ),
      ),
    );
  }
}

class _DriveButton extends StatelessWidget {
  final IconData icon;
  final String label;
  final Color? tone;
  final VoidCallback onPressed;

  const _DriveButton({
    required this.icon,
    required this.label,
    required this.onPressed,
    this.tone,
  });

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.all(4),
      child: SizedBox.square(
        dimension: 92,
        child: FilledButton(
          style: tone == null
              ? null
              : FilledButton.styleFrom(
                  backgroundColor: tone!.withValues(alpha: 0.22),
                  foregroundColor: tone,
                ),
          onPressed: onPressed,
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              Icon(icon, size: 28),
              const SizedBox(height: 4),
              Text(label, textAlign: TextAlign.center),
            ],
          ),
        ),
      ),
    );
  }
}
