/// Robot pose in 2D space.
class Pose {
  final double x; // meters
  final double y; // meters
  final double theta; // radians

  const Pose({required this.x, required this.y, required this.theta});

  factory Pose.fromJson(Map<String, dynamic> json) {
    return Pose(
      x: (json['x'] as num).toDouble(),
      y: (json['y'] as num).toDouble(),
      theta: (json['theta'] as num).toDouble(),
    );
  }

  const Pose.zero()
      : x = 0.0,
        y = 0.0,
        theta = 0.0;

  @override
  bool operator ==(Object other) =>
      other is Pose && other.x == x && other.y == y && other.theta == theta;

  @override
  int get hashCode => Object.hash(x, y, theta);
}

/// Complete telemetry snapshot received from the robot.
class RobotState {
  /// Known status strings sent by the firmware.
  static const String statusMoving = 'moving';
  static const String statusIdle = 'idle';
  static const String statusArrived = 'arrived';
  static const String statusObstacleDetected = 'obstacle_detected';
  static const String statusEmergencyStop = 'emergency_stop';
  static const String statusImuMissing = 'imu_missing';

  // Chế độ tự hành né vật cản
  static const String statusExploring = 'exploring';
  static const String statusScanning = 'scanning';
  static const String statusAvoiding = 'avoiding';
  static const String statusStuck = 'stuck';

  final Pose pose;
  final double obstacleDistCm;
  final String status;
  final bool imuConnected;
  final double headingDeg;
  final int nodeIndex;
  final int routeSize;
  final bool calibrated;

  const RobotState({
    required this.pose,
    required this.obstacleDistCm,
    required this.status,
    required this.imuConnected,
    this.headingDeg = 0.0,
    this.nodeIndex = 0,
    this.routeSize = 0,
    this.calibrated = false,
  });

  /// Parses a telemetry JSON map. Returns null on any parse error so the
  /// caller can silently discard malformed UDP packets.
  static RobotState? fromJson(Map<String, dynamic> json) {
    try {
      final poseJson = json['pose'] as Map<String, dynamic>;
      final pose = Pose.fromJson(poseJson);
      final dist = (json['obstacle_dist_cm'] as num?)?.toDouble() ?? -1.0;
      final status = json['status'] as String? ?? statusIdle;
      final imuConnected =
          (json['imu_connected'] as bool?) ?? (status != statusImuMissing);
      final headingDeg = (json['heading_deg'] as num?)?.toDouble() ??
          pose.theta * 180.0 / 3.141592653589793;
      final nodeIndex = (json['node_index'] as num?)?.toInt() ?? 0;
      final routeSize = (json['route_size'] as num?)?.toInt() ?? 0;
      final calibrated = json['calibrated'] as bool? ?? false;
      return RobotState(
        pose: pose,
        obstacleDistCm: dist,
        status: status,
        imuConnected: imuConnected,
        headingDeg: headingDeg,
        nodeIndex: nodeIndex,
        routeSize: routeSize,
        calibrated: calibrated,
      );
    } catch (_) {
      return null;
    }
  }

  factory RobotState.empty() => const RobotState(
        pose: Pose.zero(),
        obstacleDistCm: -1.0,
        status: statusIdle,
        imuConnected: false,
        headingDeg: 0.0,
        nodeIndex: 0,
        routeSize: 0,
        calibrated: false,
      );

  @override
  bool operator ==(Object other) =>
      other is RobotState &&
      other.pose == pose &&
      other.obstacleDistCm == obstacleDistCm &&
      other.status == status &&
      other.imuConnected == imuConnected &&
      other.headingDeg == headingDeg &&
      other.nodeIndex == nodeIndex &&
      other.routeSize == routeSize &&
      other.calibrated == calibrated;

  @override
  int get hashCode => Object.hash(
        pose,
        obstacleDistCm,
        status,
        imuConnected,
        headingDeg,
        nodeIndex,
        routeSize,
        calibrated,
      );
}
