import 'dart:math';
import 'package:flutter/material.dart';
import '../models/map_data.dart';
import '../models/robot_state.dart';

/// Renders the road graph, robot pose, planned path, and movement trail
/// onto a Flutter [Canvas].
class MapPainter extends CustomPainter {
  final RoadMap map;
  final RobotState robot;
  final List<Offset> plannedPath; // metric coordinates
  final List<Offset> trail; // metric coordinates
  final String? goalNodeId;
  final String? startNodeId;
  final bool showRobot;

  const MapPainter({
    required this.map,
    required this.robot,
    required this.plannedPath,
    required this.trail,
    this.showRobot = true,
    this.goalNodeId,
    this.startNodeId,
  });

  static const double _padding = 40.0;
  static const double _nodeRadius = 5.0;
  static const double _robotSize = 12.0;

  // ---------------------------------------------------------------------------
  // Coordinate transform: metric (m) → screen (px)
  // ---------------------------------------------------------------------------

  Offset _toScreen(double x, double y, Size size) {
    final drawW = size.width - _padding * 2;
    final drawH = size.height - _padding * 2;
    final mapW = max(map.maxX, 0.01);
    final mapH = max(map.maxY, 0.01);
    final scale = min(drawW / mapW, drawH / mapH);
    final originX = _padding + (drawW - mapW * scale) / 2;
    final originY = _padding + (drawH - mapH * scale) / 2;
    return Offset(originX + x * scale, originY + y * scale);
  }

  // ---------------------------------------------------------------------------
  // Paint
  // ---------------------------------------------------------------------------

  @override
  void paint(Canvas canvas, Size size) {
    _drawEdges(canvas, size);
    _drawTrail(canvas, size);
    _drawPlannedPath(canvas, size);
    _drawNodes(canvas, size);
    if (showRobot) _drawRobot(canvas, size);
  }

  void _drawEdges(Canvas canvas, Size size) {
    final normalPaint = Paint()
      ..color = Colors.grey.shade400
      ..strokeWidth = 2.0
      ..style = PaintingStyle.stroke;

    final disabledPaint = Paint()
      ..color = Colors.red.shade300
      ..strokeWidth = 2.0
      ..style = PaintingStyle.stroke;

    final nodesById = {for (final n in map.nodes) n.id: n};

    for (final edge in map.edges) {
      final fromNode = nodesById[edge.from];
      final toNode = nodesById[edge.to];
      if (fromNode == null || toNode == null) continue;

      final p1 = _toScreen(fromNode.x, fromNode.y, size);
      final p2 = _toScreen(toNode.x, toNode.y, size);

      final fwdKey = '${edge.from}->${edge.to}';
      final revKey = '${edge.to}->${edge.from}';
      final isDisabled = map.disabledEdges.contains(fwdKey) ||
          map.disabledEdges.contains(revKey);

      if (isDisabled) {
        _drawDashedLine(canvas, p1, p2, disabledPaint);
      } else {
        canvas.drawLine(p1, p2, normalPaint);
      }
    }
  }

  void _drawDashedLine(Canvas canvas, Offset p1, Offset p2, Paint paint) {
    const dashLen = 8.0;
    const gapLen = 5.0;
    final dx = p2.dx - p1.dx;
    final dy = p2.dy - p1.dy;
    final total = sqrt(dx * dx + dy * dy);
    if (total == 0) return;
    final ux = dx / total;
    final uy = dy / total;
    double walked = 0;
    bool drawing = true;
    while (walked < total) {
      final segLen = drawing ? dashLen : gapLen;
      final end = min(walked + segLen, total);
      if (drawing) {
        canvas.drawLine(
          Offset(p1.dx + ux * walked, p1.dy + uy * walked),
          Offset(p1.dx + ux * end, p1.dy + uy * end),
          paint,
        );
      }
      walked = end;
      drawing = !drawing;
    }
  }

  void _drawTrail(Canvas canvas, Size size) {
    if (trail.length < 2) return;
    final paint = Paint()
      ..color = Colors.lightBlue.withValues(alpha: 0.4)
      ..strokeWidth = 2.0
      ..style = PaintingStyle.stroke
      ..strokeJoin = StrokeJoin.round;

    final path = Path();
    final first = _toScreen(trail.first.dx, trail.first.dy, size);
    path.moveTo(first.dx, first.dy);
    for (int i = 1; i < trail.length; i++) {
      final pt = _toScreen(trail[i].dx, trail[i].dy, size);
      path.lineTo(pt.dx, pt.dy);
    }
    canvas.drawPath(path, paint);
  }

  void _drawPlannedPath(Canvas canvas, Size size) {
    if (plannedPath.length < 2) return;
    final paint = Paint()
      ..color = Colors.red
      ..strokeWidth = 3.0
      ..style = PaintingStyle.stroke;

    for (int i = 0; i < plannedPath.length - 1; i++) {
      final p1 = _toScreen(plannedPath[i].dx, plannedPath[i].dy, size);
      final p2 = _toScreen(plannedPath[i + 1].dx, plannedPath[i + 1].dy, size);
      canvas.drawLine(p1, p2, paint);
    }
  }

  void _drawNodes(Canvas canvas, Size size) {
    final circlePaint = Paint()
      ..color = Colors.white
      ..style = PaintingStyle.fill;
    final borderPaint = Paint()
      ..color = Colors.grey.shade600
      ..strokeWidth = 1.5
      ..style = PaintingStyle.stroke;
    final goalPaint = Paint()
      ..color = Colors.yellow.shade600
      ..style = PaintingStyle.fill;
    final startPaint = Paint()
      ..color = Colors.greenAccent.shade400
      ..style = PaintingStyle.fill;

    const labelStyle = TextStyle(
      color: Colors.white70,
      fontSize: 10,
      fontWeight: FontWeight.bold,
    );

    for (final node in map.nodes) {
      final pos = _toScreen(node.x, node.y, size);
      final isGoal = node.id == goalNodeId;
      final isStart = node.id == startNodeId;

      final fillPaint =
          isGoal ? goalPaint : (isStart ? startPaint : circlePaint);
      canvas.drawCircle(pos, _nodeRadius, fillPaint);
      canvas.drawCircle(pos, _nodeRadius, borderPaint);

      if (map.nodes.length <= 24 || isGoal) {
        final tp = TextPainter(
          text: TextSpan(text: node.label, style: labelStyle),
          textDirection: TextDirection.ltr,
        )..layout();
        tp.paint(
          canvas,
          pos.translate(-tp.width / 2, -_nodeRadius - tp.height - 2),
        );
      }
    }
  }

  void _drawRobot(Canvas canvas, Size size) {
    final pos = _toScreen(robot.pose.x, robot.pose.y, size);
    final theta = robot.pose.theta;

    final paint = Paint()
      ..color = Colors.red
      ..style = PaintingStyle.fill;

    // Telemetry theta uses standard robot yaw: positive means left turn.
    // The map's screen Y grows downward, so invert sin(theta) for display.
    final tip = Offset(
      pos.dx + _robotSize * cos(theta),
      pos.dy - _robotSize * sin(theta),
    );
    final left = Offset(
      pos.dx + _robotSize * 0.6 * cos(theta + pi * 0.75),
      pos.dy - _robotSize * 0.6 * sin(theta + pi * 0.75),
    );
    final right = Offset(
      pos.dx + _robotSize * 0.6 * cos(theta - pi * 0.75),
      pos.dy - _robotSize * 0.6 * sin(theta - pi * 0.75),
    );

    final triangle = Path()
      ..moveTo(tip.dx, tip.dy)
      ..lineTo(left.dx, left.dy)
      ..lineTo(right.dx, right.dy)
      ..close();

    canvas.drawPath(triangle, paint);
  }

  @override
  bool shouldRepaint(MapPainter old) =>
      robot.pose != old.robot.pose ||
      plannedPath.length != old.plannedPath.length ||
      trail.length != old.trail.length ||
      goalNodeId != old.goalNodeId ||
      startNodeId != old.startNodeId ||
      showRobot != old.showRobot ||
      map.disabledEdges.length != old.map.disabledEdges.length;
}
