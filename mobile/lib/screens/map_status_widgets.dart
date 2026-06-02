import 'dart:math';
import 'package:flutter/material.dart';
import '../models/robot_state.dart';

/// Bottom status bar showing live pose, obstacle distance, and robot status.
class MapStatusBar extends StatelessWidget {
  final RobotState robotState;

  const MapStatusBar({super.key, required this.robotState});

  @override
  Widget build(BuildContext context) {
    final p = robotState.pose;
    final thetaDeg = robotState.headingDeg.toStringAsFixed(1);
    final dist = robotState.obstacleDistCm < 0
        ? '—'
        : '${robotState.obstacleDistCm.toStringAsFixed(1)} cm';
    final progress = robotState.routeSize <= 0
        ? '—'
        : '${robotState.nodeIndex.clamp(0, robotState.routeSize)}/${robotState.routeSize}';
    final obstacleColor =
        robotState.obstacleDistCm > 0 && robotState.obstacleDistCm < 20
            ? Colors.red
            : Colors.white70;

    return Container(
      color: Colors.grey.shade900,
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
      child: Wrap(
        spacing: 14,
        runSpacing: 6,
        crossAxisAlignment: WrapCrossAlignment.center,
        children: [
          Text(
            'x=${p.x.toStringAsFixed(2)}  '
            'y=${p.y.toStringAsFixed(2)}',
            style: const TextStyle(fontSize: 12, color: Colors.white70),
          ),
          Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              Transform.rotate(
                angle: -p.theta + pi / 2,
                child: const Icon(Icons.navigation,
                    size: 16, color: Colors.lightBlueAccent),
              ),
              const SizedBox(width: 4),
              Text(
                '$thetaDeg°',
                style: const TextStyle(fontSize: 12, color: Colors.white70),
              ),
            ],
          ),
          Text(
            'Node: $progress',
            style: const TextStyle(fontSize: 12, color: Colors.white70),
          ),
          Text(
            'Obs: $dist',
            style: TextStyle(fontSize: 12, color: obstacleColor),
          ),
          Text(
            robotState.calibrated ? 'Cal OK' : 'Cal needed',
            style: TextStyle(
              fontSize: 12,
              color: robotState.calibrated
                  ? Colors.greenAccent
                  : Colors.amberAccent,
            ),
          ),
          Text(
            robotState.status.replaceAll('_', ' '),
            style: const TextStyle(fontSize: 12, color: Colors.white70),
          ),
        ],
      ),
    );
  }
}

/// AppBar trailing widget: coloured dot + label showing connection state.
class ConnectionIndicator extends StatelessWidget {
  final bool isConnected;

  const ConnectionIndicator({super.key, required this.isConnected});

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(right: 12),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          CircleAvatar(
            radius: 5,
            backgroundColor: isConnected ? Colors.green : Colors.red,
          ),
          const SizedBox(width: 6),
          Text(
            isConnected ? 'Connected' : 'Disconnected',
            style: const TextStyle(fontSize: 12),
          ),
        ],
      ),
    );
  }
}
