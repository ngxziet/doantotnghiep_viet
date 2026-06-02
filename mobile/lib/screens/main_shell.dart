import 'package:flutter/material.dart';

import '../services/udp_service.dart';
import 'connection_test_screen.dart';
import 'manual_control_screen.dart';
import 'map_screen.dart';
import 'telemetry_screen.dart';

class MainShell extends StatefulWidget {
  final ITransportService transport;

  const MainShell({super.key, required this.transport});

  @override
  State<MainShell> createState() => _MainShellState();
}

class _MainShellState extends State<MainShell> {
  int _index = 0;

  static const _titles = [
    'Map',
    'ESP32 Test',
    'Manual Control',
    'Wheel Test',
  ];

  @override
  Widget build(BuildContext context) {
    final screens = [
      MapScreen(transport: widget.transport),
      ConnectionTestScreen(transport: widget.transport),
      ManualControlScreen(transport: widget.transport),
      TelemetryScreen(transport: widget.transport),
    ];

    return Scaffold(
      appBar: AppBar(title: Text(_titles[_index])),
      body: IndexedStack(index: _index, children: screens),
      bottomNavigationBar: NavigationBar(
        selectedIndex: _index,
        onDestinationSelected: (value) => setState(() => _index = value),
        destinations: const [
          NavigationDestination(icon: Icon(Icons.map), label: 'Map'),
          NavigationDestination(
              icon: Icon(Icons.wifi_tethering), label: 'Test'),
          NavigationDestination(icon: Icon(Icons.gamepad), label: 'Control'),
          NavigationDestination(icon: Icon(Icons.build), label: 'Wheels'),
        ],
      ),
    );
  }
}
