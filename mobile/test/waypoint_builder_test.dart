import 'package:flutter_test/flutter_test.dart';
import 'package:mobile/models/map_data.dart';
import 'package:mobile/planning/waypoint_builder.dart';

RoadMap _twoNodeMap() => RoadMap.fromJson({
      'nodes': [
        {'id': 'A', 'x': 0.0, 'y': 0.0},
        {'id': 'B', 'x': 1.0, 'y': 0.5},
        {'id': 'C', 'x': 2.0, 'y': 1.0},
      ],
      'edges': [
        {'from': 'A', 'to': 'B', 'bidirectional': true},
        {'from': 'B', 'to': 'C', 'bidirectional': true},
      ],
    });

void main() {
  group('WaypointBuilder.build', () {
    late RoadMap map;
    setUp(() => map = _twoNodeMap());

    test('empty path returns empty list', () {
      expect(WaypointBuilder.build(map, []), isEmpty);
    });

    test('single node path returns one Offset matching node coords', () {
      final result = WaypointBuilder.build(map, ['B']);
      expect(result.length, 1);
      expect(result.first.dx, closeTo(1.0, 1e-9));
      expect(result.first.dy, closeTo(0.5, 1e-9));
    });

    test('multi-node path returns Offsets in order', () {
      final result = WaypointBuilder.build(map, ['A', 'B', 'C']);
      expect(result.length, 3);
      expect(result[0].dx, closeTo(0.0, 1e-9));
      expect(result[0].dy, closeTo(0.0, 1e-9));
      expect(result[1].dx, closeTo(1.0, 1e-9));
      expect(result[1].dy, closeTo(0.5, 1e-9));
      expect(result[2].dx, closeTo(2.0, 1e-9));
      expect(result[2].dy, closeTo(1.0, 1e-9));
    });
  });
}
