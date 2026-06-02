import 'package:flutter/material.dart';

import 'screens/main_shell.dart';
import 'services/simulator_service.dart';
import 'services/udp_service.dart';

void main() {
  final useSimulator =
      bool.tryParse(const String.fromEnvironment('USE_SIMULATOR')) ?? false;
  final ITransportService transport =
      useSimulator ? SimulatorService() : UdpService();
  runApp(AutonomousVehicleApp(transport: transport));
}

class AutonomousVehicleApp extends StatefulWidget {
  final ITransportService transport;

  const AutonomousVehicleApp({super.key, required this.transport});

  @override
  State<AutonomousVehicleApp> createState() => _AutonomousVehicleAppState();
}

class _AutonomousVehicleAppState extends State<AutonomousVehicleApp> {
  @override
  void dispose() {
    widget.transport.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Autonomous Vehicle',
      debugShowCheckedModeBanner: false,
      theme: ThemeData.dark(useMaterial3: true),
      home: MainShell(transport: widget.transport),
    );
  }
}
