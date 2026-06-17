import 'dart:async';
import 'dart:math';
import 'dart:ui';

import '../models/robot_state.dart';
import 'udp_service.dart';

/// Simulates robot movement along waypoints without real hardware.
///
/// Broadcasts fake telemetry at 10 Hz. When [sendWaypoints] is called the
/// simulated robot begins moving toward each waypoint in order using simple
/// linear interpolation at 0.5 m/s (0.05 m per 100 ms tick). When within
/// [_arrivalThreshold] metres of the current waypoint it advances to the
/// next. After all waypoints are consumed status becomes 'arrived'.
class SimulatorService implements ITransportService {
  static const double _speedPerTick = 0.05; // metres per 100 ms tick
  static const double _arrivalThreshold = 0.03; // metres — "close enough"
  static const int _tickMs = 100; // telemetry interval

  final _controller = StreamController<RobotState>.broadcast();
  final _connController = StreamController<bool>.broadcast();

  Pose _pose = const Pose.zero();
  String _status = RobotState.statusIdle;
  double _obstacleDistCm = -1.0;
  bool _calibrated = true;

  final List<Offset> _waypoints = [];
  int _waypointIndex = 0;

  Timer? _ticker;
  Timer? _obstacleResetTimer;

  SimulatorService() {
    _ticker = Timer.periodic(
      const Duration(milliseconds: _tickMs),
      (_) => _tick(),
    );
    _connController.add(true);
  }

  // ---------------------------------------------------------------------------
  // ITransportService
  // ---------------------------------------------------------------------------

  @override
  Stream<RobotState> get telemetryStream => _controller.stream;

  @override
  Stream<bool> get connectionStream => _connController.stream;

  @override
  bool get isConnected => true;

  /// Loads a new waypoint list and starts moving toward the first one.
  @override
  Future<void> sendWaypoints(List<Offset> waypoints, int seq) async {
    _waypoints
      ..clear()
      ..addAll(waypoints);
    _waypointIndex = 0;
    if (_waypoints.isNotEmpty) {
      _status = RobotState.statusMoving;
    }
  }

  @override
  Future<void> sendManualCommand(String command, int speed, int seq) async {
    final step = (speed.clamp(0, 255) / 255.0) * 0.12;
    switch (command) {
      case 'forward':
        _pose = Pose(
          x: _pose.x + step * cos(_pose.theta),
          y: _pose.y - step * sin(_pose.theta),
          theta: _pose.theta,
        );
        break;
      case 'backward':
        _pose = Pose(
          x: _pose.x - step * cos(_pose.theta),
          y: _pose.y + step * sin(_pose.theta),
          theta: _pose.theta,
        );
        break;
      case 'left':
        _pose = Pose(x: _pose.x, y: _pose.y, theta: _pose.theta + pi / 8);
        break;
      case 'right':
        _pose = Pose(x: _pose.x, y: _pose.y, theta: _pose.theta - pi / 8);
        break;
      case 'left_forward':
      case 'right_forward':
        _pose = Pose(
          x: _pose.x + step * 0.5 * cos(_pose.theta),
          y: _pose.y - step * 0.5 * sin(_pose.theta),
          theta: _pose.theta,
        );
        break;
      case 'left_backward':
      case 'right_backward':
        _pose = Pose(
          x: _pose.x - step * 0.5 * cos(_pose.theta),
          y: _pose.y + step * 0.5 * sin(_pose.theta),
          theta: _pose.theta,
        );
        break;
      default:
        _status = RobotState.statusIdle;
        _waypoints.clear();
        return;
    }
    _status = RobotState.statusMoving;
  }

  @override
  Future<void> sendAutonomousCommand(String command, int seq) async {
    if (command == 'start') {
      _status = RobotState.statusExploring;
    } else {
      _status = RobotState.statusIdle;
    }
  }

  @override
  Future<void> sendStepCommand(String action, int seq) async {
    _waypoints.clear();
    _waypointIndex = 0;
    switch (action) {
      case 'forward':
        _pose = Pose(
          x: _pose.x + 0.5 * cos(_pose.theta),
          y: _pose.y - 0.5 * sin(_pose.theta),
          theta: _pose.theta,
        );
        break;
      case 'left':
        _pose = Pose(
            x: _pose.x,
            y: _pose.y,
            theta: _normalizeAngle(_pose.theta + pi / 2));
        break;
      case 'right':
        _pose = Pose(
            x: _pose.x,
            y: _pose.y,
            theta: _normalizeAngle(_pose.theta - pi / 2));
        break;
      case 'backward':
        _pose = Pose(
            x: _pose.x, y: _pose.y, theta: _normalizeAngle(_pose.theta + pi));
        break;
      default:
        return;
    }
    _status = RobotState.statusArrived;
  }

  @override
  Future<void> sendCalibrationCommand(String mode, int seq) async {
    _calibrated = true;
  }

  @override
  Future<void> resetPose(int seq) async {
    _pose = const Pose.zero();
    _status = RobotState.statusIdle;
    _waypoints.clear();
    _waypointIndex = 0;
    if (!_controller.isClosed) {
      _controller.add(
        RobotState(
          pose: _pose,
          obstacleDistCm: _obstacleDistCm,
          status: _status,
          imuConnected: true,
          headingDeg: _pose.theta * 180 / pi,
          nodeIndex: _waypointIndex,
          routeSize: _waypoints.length,
          calibrated: _calibrated,
        ),
      );
    }
  }

  @override
  void dispose() {
    _ticker?.cancel();
    _obstacleResetTimer?.cancel();
    _controller.close();
    _connController.close();
  }

  // ---------------------------------------------------------------------------
  // Obstacle simulation — call from tests or debug UI
  // ---------------------------------------------------------------------------

  /// Simulates an obstacle at [distCm] centimetres for 3 seconds then clears.
  void triggerObstacle(double distCm) {
    _obstacleDistCm = distCm;
    _status = RobotState.statusObstacleDetected;

    _obstacleResetTimer?.cancel();
    _obstacleResetTimer = Timer(const Duration(seconds: 3), () {
      _obstacleDistCm = -1.0;
      if (_status == RobotState.statusObstacleDetected) {
        _status = _waypoints.isNotEmpty && _waypointIndex < _waypoints.length
            ? RobotState.statusMoving
            : RobotState.statusIdle;
      }
    });
  }

  // ---------------------------------------------------------------------------
  // Internal simulation tick
  // ---------------------------------------------------------------------------

  void _tick() {
    if (_status == RobotState.statusMoving) {
      _stepTowardWaypoint();
    }

    if (!_controller.isClosed) {
      _controller.add(
        RobotState(
          pose: _pose,
          obstacleDistCm: _obstacleDistCm,
          status: _status,
          imuConnected: true,
          headingDeg: _pose.theta * 180 / pi,
          nodeIndex: _waypointIndex,
          routeSize: _waypoints.length,
          calibrated: _calibrated,
        ),
      );
    }
  }

  void _stepTowardWaypoint() {
    if (_waypoints.isEmpty || _waypointIndex >= _waypoints.length) {
      _status = RobotState.statusArrived;
      _waypoints.clear();
      return;
    }

    final target = _waypoints[_waypointIndex];
    final dx = target.dx - _pose.x;
    final dy = target.dy - _pose.y;
    final dist = sqrt(dx * dx + dy * dy);

    if (dist <= _arrivalThreshold) {
      // Snap to waypoint position and advance index.
      _pose = Pose(x: target.dx, y: target.dy, theta: _pose.theta);
      _waypointIndex++;

      if (_waypointIndex >= _waypoints.length) {
        _status = RobotState.statusArrived;
        _waypoints.clear();
      }
      return;
    }

    // Move one tick toward the target.
    final step = min(_speedPerTick, dist);
    final theta = atan2(dy, dx);
    _pose = Pose(
      x: _pose.x + step * cos(theta),
      y: _pose.y + step * sin(theta),
      theta: theta,
    );
  }

  static double _normalizeAngle(double angle) {
    while (angle > pi) {
      angle -= 2 * pi;
    }
    while (angle < -pi) {
      angle += 2 * pi;
    }
    return angle;
  }
}
