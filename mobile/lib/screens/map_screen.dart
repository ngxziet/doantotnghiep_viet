import 'dart:async';
import 'dart:convert';
import 'dart:math';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../models/map_data.dart';
import '../models/robot_state.dart';
import '../planning/astar.dart';
import '../planning/waypoint_builder.dart';
import '../services/udp_service.dart';
import 'map_painter.dart';
import 'map_status_widgets.dart';

/// Main navigation screen: full-screen canvas map with tap-to-navigate.
class MapScreen extends StatefulWidget {
  final ITransportService transport;

  const MapScreen({super.key, required this.transport});

  @override
  State<MapScreen> createState() => _MapScreenState();
}

class _MapScreenState extends State<MapScreen> {
  RobotState _robotState = RobotState.empty();
  RoadMap _map = RoadMap.empty();
  List<Offset> _plannedPath = [];
  final List<Offset> _trail = [];
  String? _selectedGoal;
  bool _isConnected = false;
  bool _hasTelemetry = false;
  bool _isBlockMode = false;
  int _waypointSeq = 0;

  StreamSubscription<RobotState>? _telemetrySub;
  Timer? _connectionTimer;
  static const _connectionTimeoutMs = 2000;
  static const _maxTrailPoints = 500;
  // Pixel distance within which a tap selects an edge in block mode.
  static const double _edgeTapRadiusPx = 28.0;
  // Pixel distance within which a tap selects a node.
  static const double _nodeTapRadiusPx = 60.0;
  static const double _currentWaypointToleranceM = 0.03;

  @override
  void initState() {
    super.initState();
    _loadMap();
    _startTelemetryListener();
  }

  @override
  void dispose() {
    _telemetrySub?.cancel();
    _connectionTimer?.cancel();
    super.dispose();
  }

  // ---------------------------------------------------------------------------
  // Initialisation
  // ---------------------------------------------------------------------------

  Future<void> _loadMap() async {
    try {
      final raw = await rootBundle.loadString('assets/environment.json');
      final json = jsonDecode(raw) as Map<String, dynamic>;
      setState(() => _map = RoadMap.fromJson(json));
    } catch (e) {
      debugPrint('MapScreen: failed to load map — $e');
    }
  }

  void _startTelemetryListener() {
    _telemetrySub = widget.transport.telemetryStream.listen(
      (state) {
        setState(() {
          _robotState = state;
          _isConnected = true;
          _hasTelemetry = true;

          _trail.add(Offset(state.pose.x, state.pose.y));
          if (_trail.length > _maxTrailPoints) _trail.removeAt(0);

          if (state.status == RobotState.statusArrived) {
            _plannedPath = [];
            _selectedGoal = null;
          }
        });

        // Reset connection watchdog on every received packet.
        _connectionTimer?.cancel();
        _connectionTimer = Timer(
          const Duration(milliseconds: _connectionTimeoutMs),
          () {
            if (!mounted) return;
            setState(() => _isConnected = false);
          },
        );
      },
      onError: (Object e) {
        debugPrint('MapScreen: telemetry error — $e');
        setState(() => _isConnected = false);
      },
    );
  }

  // ---------------------------------------------------------------------------
  // Navigation
  // ---------------------------------------------------------------------------

  Future<void> _onTapUp(
    TapUpDetails details,
    BoxConstraints constraints,
  ) async {
    final canvasSize = Size(constraints.maxWidth, constraints.maxHeight);

    if (_isBlockMode) {
      await _toggleNearestEdge(details.localPosition, canvasSize);
      return;
    }

    final tappedNode = _findNearestNode(
      details.localPosition,
      canvasSize,
    );
    if (tappedNode == null) return;

    final startNode = _getRouteStartNode();
    if (startNode == null || tappedNode == startNode) return;

    setState(() => _selectedGoal = tappedNode);
    await _computeAndSendPath(startNode, tappedNode);
  }

  Future<void> _computeAndSendPath(String startNode, String goal) async {
    final nodePath = AStar.findPath(_map, startNode, goal);
    if (nodePath == null) {
      setState(() => _plannedPath = []);
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('No path found to selected node.')),
      );
      return;
    }

    final waypoints = WaypointBuilder.build(_map, nodePath);
    final commandWaypoints = _dropCurrentWaypoint(waypoints);
    setState(() {
      _plannedPath = waypoints;
    });

    try {
      _waypointSeq++;
      await widget.transport.sendWaypoints(commandWaypoints, _waypointSeq);
      if (!mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text('Route sent: ${commandWaypoints.length} nodes.'),
        ),
      );
    } catch (e) {
      if (!mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Failed to send route: $e')),
      );
    }
  }

  Future<void> _toggleNearestEdge(Offset tap, Size size) async {
    final edge = _findNearestEdgeInRadius(tap, size);
    if (edge == null) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
          content: Text('Tap closer to a road segment to block it.'),
        ),
      );
      return;
    }

    final alreadyBlocked =
        _map.disabledEdges.contains('${edge.from}->${edge.to}') ||
            _map.disabledEdges.contains('${edge.to}->${edge.from}');

    setState(() {
      if (alreadyBlocked) {
        _map.enableEdge(edge.from, edge.to);
      } else {
        _map.disableEdge(edge.from, edge.to);
      }
    });

    final goal = _selectedGoal;
    final startNode = _getRouteStartNode();
    if (goal != null && startNode != null && goal != startNode) {
      await _computeAndSendPath(startNode, goal);
    }
  }

  Future<void> _resetMapBlocks() async {
    setState(() => _map.enableAllEdges());

    final goal = _selectedGoal;
    final startNode = _getRouteStartNode();
    if (goal != null && startNode != null && goal != startNode) {
      await _computeAndSendPath(startNode, goal);
    }
  }

  List<Offset> _dropCurrentWaypoint(List<Offset> waypoints) {
    if (waypoints.length < 2) return waypoints;

    final first = waypoints.first;
    final current = Offset(_robotState.pose.x, _robotState.pose.y);
    if ((first - current).distance <= _currentWaypointToleranceM) {
      return waypoints.skip(1).toList();
    }

    return waypoints;
  }

  Future<void> _resetPose() async {
    setState(() {
      _robotState = RobotState.empty();
      _plannedPath = [];
      _trail.clear();
      _selectedGoal = null;
    });

    _waypointSeq++;
    await widget.transport.resetPose(_waypointSeq);
  }

  String? _getRouteStartNode() {
    if (_map.nodes.isEmpty) return null;
    if (_hasTelemetry) {
      return _map.getNearestNode(_robotState.pose.x, _robotState.pose.y);
    }

    final nodes = [..._map.nodes]..sort((a, b) {
        final yCompare = a.y.compareTo(b.y);
        return yCompare != 0 ? yCompare : a.x.compareTo(b.x);
      });
    return nodes.first.id;
  }

  /// Finds the nearest graph node for a tap inside the map area.
  String? _findNearestNode(Offset tap, Size size) {
    final transform = _mapTransform(size);
    if (!_isInsideMap(tap, transform)) return null;

    String? best;
    double bestDist = _nodeTapRadiusPx;

    for (final node in _map.nodes) {
      final screenPos = _toScreen(node.x, node.y, transform);
      final dist = (tap - screenPos).distance;
      if (dist < bestDist) {
        bestDist = dist;
        best = node.id;
      }
    }
    return best;
  }

  bool _isInsideMap(
    Offset tap,
    ({double scale, double originX, double originY}) transform,
  ) {
    final left = transform.originX;
    final top = transform.originY;
    final right = transform.originX + _map.maxX * transform.scale;
    final bottom = transform.originY + _map.maxY * transform.scale;
    return tap.dx >= left &&
        tap.dx <= right &&
        tap.dy >= top &&
        tap.dy <= bottom;
  }

  Edge? _findNearestEdgeInRadius(Offset tap, Size size) {
    final transform = _mapTransform(size);
    final nodesById = {for (final node in _map.nodes) node.id: node};

    Edge? best;
    double bestDist = _edgeTapRadiusPx;

    for (final edge in _map.edges) {
      final from = nodesById[edge.from];
      final to = nodesById[edge.to];
      if (from == null || to == null) continue;

      final p1 = _toScreen(from.x, from.y, transform);
      final p2 = _toScreen(to.x, to.y, transform);
      final dist = _distanceToSegment(tap, p1, p2);
      if (dist < bestDist) {
        bestDist = dist;
        best = edge;
      }
    }

    return best;
  }

  ({double scale, double originX, double originY}) _mapTransform(Size size) {
    const padding = 40.0;
    final drawW = size.width - padding * 2;
    final drawH = size.height - padding * 2;
    final mapW = max(_map.maxX, 0.01);
    final mapH = max(_map.maxY, 0.01);
    final scale = min(drawW / mapW, drawH / mapH);
    final originX = padding + (drawW - mapW * scale) / 2;
    final originY = padding + (drawH - mapH * scale) / 2;
    return (scale: scale, originX: originX, originY: originY);
  }

  Offset _toScreen(
    double x,
    double y,
    ({double scale, double originX, double originY}) transform,
  ) {
    return Offset(
      transform.originX + x * transform.scale,
      transform.originY + y * transform.scale,
    );
  }

  double _distanceToSegment(Offset point, Offset a, Offset b) {
    final ab = b - a;
    final abLenSq = ab.dx * ab.dx + ab.dy * ab.dy;
    if (abLenSq == 0) return (point - a).distance;

    final ap = point - a;
    final t = ((ap.dx * ab.dx + ap.dy * ab.dy) / abLenSq).clamp(0.0, 1.0);
    final closest = Offset(a.dx + ab.dx * t, a.dy + ab.dy * t);
    return (point - closest).distance;
  }

  // ---------------------------------------------------------------------------
  // Build
  // ---------------------------------------------------------------------------

  @override
  Widget build(BuildContext context) {
    return Column(
      children: [
        Expanded(child: _buildCanvas()),
        Padding(
          padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
          child: SingleChildScrollView(
            scrollDirection: Axis.horizontal,
            child: Row(
              children: [
                ConnectionIndicator(isConnected: _isConnected),
                const SizedBox(width: 12),
                FilledButton.tonalIcon(
                  onPressed: () => setState(() => _isBlockMode = !_isBlockMode),
                  icon: Icon(_isBlockMode ? Icons.edit_road : Icons.block),
                  label: Text(_isBlockMode ? 'Tap road' : 'Block/unblock'),
                ),
                const SizedBox(width: 8),
                FilledButton.tonalIcon(
                  onPressed:
                      _map.disabledEdges.isEmpty ? null : _resetMapBlocks,
                  icon: const Icon(Icons.layers_clear),
                  label: const Text('Reset map'),
                ),
                const SizedBox(width: 8),
                FilledButton.tonalIcon(
                  onPressed: _resetPose,
                  icon: const Icon(Icons.restart_alt),
                  label: const Text('Reset pose'),
                ),
              ],
            ),
          ),
        ),
        MapStatusBar(robotState: _robotState),
      ],
    );
  }

  Widget _buildCanvas() {
    return LayoutBuilder(
      builder: (context, constraints) {
        return GestureDetector(
          onTapUp: (d) => _onTapUp(d, constraints),
          child: RepaintBoundary(
            child: CustomPaint(
              size: Size(constraints.maxWidth, constraints.maxHeight),
              painter: MapPainter(
                map: _map,
                robot: _robotState,
                plannedPath: _plannedPath,
                trail: List.unmodifiable(_trail),
                goalNodeId: _selectedGoal,
                startNodeId: _getRouteStartNode(),
                showRobot: _hasTelemetry,
              ),
            ),
          ),
        );
      },
    );
  }
}
