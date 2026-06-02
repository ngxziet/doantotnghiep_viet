import 'dart:math';
import 'package:flutter_test/flutter_test.dart';

/// Pure-math odometry calculations mirroring firmware odometry.cpp logic.
/// No hardware or Flutter dependencies — runs as a plain Dart unit test.

/// Applies one odometry step and returns updated (x, y, theta).
/// [d]     distance travelled this tick (metres)
/// [theta] current heading (radians)
(double x, double y) odometryStep(double x, double y, double theta, double d) {
  return (x + d * cos(theta), y + d * sin(theta));
}

/// Complementary filter for heading fusion.
/// [thetaImu] heading from IMU (radians)
/// [thetaEnc] heading from encoder dead-reckoning (radians)
/// [alpha]    IMU trust weight (0..1), typically 0.98
double complementaryFilter(double thetaImu, double thetaEnc, double alpha) {
  return alpha * thetaImu + (1.0 - alpha) * thetaEnc;
}

void main() {
  const epsilon = 1e-3; // ±1 mm tolerance

  group('Odometry position update', () {
    test('d=0.10m theta=0 → x increases by 0.10, y unchanged', () {
      final (x, y) = odometryStep(0.0, 0.0, 0.0, 0.10);
      expect(x, closeTo(0.10, epsilon));
      expect(y, closeTo(0.0, epsilon));
    });

    test('d=0.10m theta=pi/2 → x unchanged, y increases by 0.10', () {
      final (x, y) = odometryStep(0.0, 0.0, pi / 2, 0.10);
      expect(x, closeTo(0.0, epsilon));
      expect(y, closeTo(0.10, epsilon));
    });

    test('d=0 → pose unchanged regardless of theta', () {
      final (x, y) = odometryStep(1.5, 2.3, pi / 4, 0.0);
      expect(x, closeTo(1.5, epsilon));
      expect(y, closeTo(2.3, epsilon));
    });

    test('d=0.10m theta=pi → x decreases by 0.10, y unchanged', () {
      final (x, y) = odometryStep(0.0, 0.0, pi, 0.10);
      expect(x, closeTo(-0.10, epsilon));
      expect(y, closeTo(0.0, epsilon));
    });
  });

  group('Complementary filter', () {
    test('alpha=0.98: fused theta ≈ 0.998 for imu=1.0, enc=0.9', () {
      // 0.98 * 1.0 + 0.02 * 0.9 = 0.98 + 0.018 = 0.998
      final result = complementaryFilter(1.0, 0.9, 0.98);
      expect(result, closeTo(0.998, epsilon));
    });

    test('alpha=1.0 → full IMU trust', () {
      final result = complementaryFilter(1.0, 0.9, 1.0);
      expect(result, closeTo(1.0, epsilon));
    });

    test('alpha=0.0 → full encoder trust', () {
      final result = complementaryFilter(1.0, 0.9, 0.0);
      expect(result, closeTo(0.9, epsilon));
    });

    test('equal inputs → output equals input regardless of alpha', () {
      final result = complementaryFilter(0.5, 0.5, 0.98);
      expect(result, closeTo(0.5, epsilon));
    });
  });
}
