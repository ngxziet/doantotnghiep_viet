import 'package:flutter_test/flutter_test.dart';
import 'package:mobile/models/robot_state.dart';

void main() {
  group('RobotState.fromJson', () {
    test('parses valid JSON correctly', () {
      final json = {
        'pose': {'x': 0.35, 'y': 0.12, 'theta': 1.57},
        'obstacle_dist_cm': 45.2,
        'status': 'moving',
        'imu_connected': true,
        'heading_deg': 90.0,
        'node_index': 2,
        'route_size': 5,
        'calibrated': true,
      };
      final state = RobotState.fromJson(json);
      expect(state, isNotNull);
      expect(state!.pose.x, closeTo(0.35, 1e-9));
      expect(state.pose.y, closeTo(0.12, 1e-9));
      expect(state.pose.theta, closeTo(1.57, 1e-9));
      expect(state.obstacleDistCm, closeTo(45.2, 1e-9));
      expect(state.status, equals('moving'));
      expect(state.imuConnected, isTrue);
      expect(state.headingDeg, closeTo(90.0, 1e-9));
      expect(state.nodeIndex, equals(2));
      expect(state.routeSize, equals(5));
      expect(state.calibrated, isTrue);
    });

    test('handles malformed JSON (missing pose) → returns null', () {
      final json = {
        'obstacle_dist_cm': 10.0,
        'status': 'idle',
        // 'pose' key intentionally absent
      };
      final state = RobotState.fromJson(json);
      expect(state, isNull);
    });

    test('handles non-map pose value → returns null', () {
      final json = {
        'pose': 'bad_value',
        'obstacle_dist_cm': 10.0,
        'status': 'idle',
      };
      final state = RobotState.fromJson(json);
      expect(state, isNull);
    });

    test('missing obstacle_dist_cm → defaults to -1.0', () {
      final json = {
        'pose': {'x': 0.0, 'y': 0.0, 'theta': 0.0},
        'status': 'idle',
        // obstacle_dist_cm absent
      };
      final state = RobotState.fromJson(json);
      expect(state, isNotNull);
      expect(state!.obstacleDistCm, equals(-1.0));
    });

    test('missing imu_connected falls back from status', () {
      final json = {
        'pose': {'x': 0.0, 'y': 0.0, 'theta': 0.0},
        'status': RobotState.statusImuMissing,
      };
      final state = RobotState.fromJson(json);
      expect(state, isNotNull);
      expect(state!.imuConnected, isFalse);
    });

    test('missing status → defaults to idle', () {
      final json = {
        'pose': {'x': 0.0, 'y': 0.0, 'theta': 0.0},
        'obstacle_dist_cm': 30.0,
      };
      final state = RobotState.fromJson(json);
      expect(state, isNotNull);
      expect(state!.status, equals(RobotState.statusIdle));
    });

    test('missing heading and progress fields use defaults', () {
      final json = {
        'pose': {'x': 0.0, 'y': 0.0, 'theta': 1.5707963267948966},
        'status': 'idle',
      };
      final state = RobotState.fromJson(json);
      expect(state, isNotNull);
      expect(state!.headingDeg, closeTo(90.0, 1e-6));
      expect(state.nodeIndex, equals(0));
      expect(state.routeSize, equals(0));
      expect(state.calibrated, isFalse);
    });
  });

  group('RobotState.empty', () {
    test('constructs with zero pose and idle status', () {
      final state = RobotState.empty();
      expect(state.pose.x, 0.0);
      expect(state.pose.y, 0.0);
      expect(state.pose.theta, 0.0);
      expect(state.obstacleDistCm, -1.0);
      expect(state.status, RobotState.statusIdle);
      expect(state.imuConnected, isFalse);
      expect(state.headingDeg, 0.0);
      expect(state.nodeIndex, 0);
      expect(state.routeSize, 0);
      expect(state.calibrated, isFalse);
    });
  });
}
