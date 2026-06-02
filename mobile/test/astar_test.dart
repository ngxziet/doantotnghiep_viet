import 'package:flutter_test/flutter_test.dart';
import 'package:mobile/models/map_data.dart';
import 'package:mobile/planning/astar.dart';

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Builds a RoadMap from inline JSON-style maps.
RoadMap _buildMap(
    List<Map<String, dynamic>> nodes, List<Map<String, dynamic>> edges) {
  return RoadMap.fromJson({'nodes': nodes, 'edges': edges});
}

/// Linear graph:  A — B — C — D
RoadMap _linearMap() => _buildMap(
      [
        {'id': 'A', 'x': 0.0, 'y': 0.0},
        {'id': 'B', 'x': 1.0, 'y': 0.0},
        {'id': 'C', 'x': 2.0, 'y': 0.0},
        {'id': 'D', 'x': 3.0, 'y': 0.0},
      ],
      [
        {'from': 'A', 'to': 'B', 'bidirectional': true},
        {'from': 'B', 'to': 'C', 'bidirectional': true},
        {'from': 'C', 'to': 'D', 'bidirectional': true},
      ],
    );

/// Branching graph:
///   A — B — D
///       |
///       C (detour, longer)
RoadMap _branchingMap() => _buildMap(
      [
        {'id': 'A', 'x': 0.0, 'y': 0.0},
        {'id': 'B', 'x': 1.0, 'y': 0.0},
        {'id': 'C', 'x': 1.0, 'y': 5.0}, // long detour
        {'id': 'D', 'x': 2.0, 'y': 0.0},
        {'id': 'Z', 'x': 99.0, 'y': 99.0}, // isolated
      ],
      [
        {'from': 'A', 'to': 'B', 'bidirectional': true},
        {'from': 'B', 'to': 'C', 'bidirectional': true},
        {'from': 'C', 'to': 'D', 'bidirectional': true},
        {'from': 'B', 'to': 'D', 'bidirectional': true},
      ],
    );

/// Equal distance routes:
///   A - B - C - D       (straight, 0 turns)
///   A - E - F - D       (zig-zag, 2 turns)
RoadMap _equalDistanceTurnMap() => _buildMap(
      [
        {'id': 'A', 'x': 0.0, 'y': 0.0},
        {'id': 'B', 'x': 1.0, 'y': 0.0},
        {'id': 'C', 'x': 2.0, 'y': 0.0},
        {'id': 'D', 'x': 3.0, 'y': 0.0},
        {'id': 'E', 'x': 0.0, 'y': 1.0},
        {'id': 'F', 'x': 3.0, 'y': 1.0},
      ],
      [
        {'from': 'A', 'to': 'E', 'bidirectional': true},
        {'from': 'E', 'to': 'F', 'bidirectional': true},
        {'from': 'F', 'to': 'D', 'bidirectional': true},
        {'from': 'A', 'to': 'B', 'bidirectional': true},
        {'from': 'B', 'to': 'C', 'bidirectional': true},
        {'from': 'C', 'to': 'D', 'bidirectional': true},
      ],
    );

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

void main() {
  group('AStar.findPath', () {
    test('finds path in linear graph A→B→C→D', () {
      final map = _linearMap();
      final path = AStar.findPath(map, 'A', 'D');
      expect(path, isNotNull);
      expect(path!.first, 'A');
      expect(path.last, 'D');
      // Must visit all intermediate nodes in order.
      expect(path, equals(['A', 'B', 'C', 'D']));
    });

    test('finds shortest path in branching graph, skips long detour', () {
      final map = _branchingMap();
      final path = AStar.findPath(map, 'A', 'D');
      expect(path, isNotNull);
      // Shortest: A→B→D, not A→B→C→D.
      expect(path, equals(['A', 'B', 'D']));
    });

    test('prefers fewer turns when path distance is tied', () {
      final map = _equalDistanceTurnMap();
      final path = AStar.findPath(map, 'A', 'D');
      expect(path, equals(['A', 'B', 'C', 'D']));
    });

    test('returns null when goal is unreachable (isolated node)', () {
      final map = _branchingMap();
      final path = AStar.findPath(map, 'A', 'Z');
      expect(path, isNull);
    });

    test('returns [start] when start == goal', () {
      final map = _linearMap();
      final path = AStar.findPath(map, 'B', 'B');
      expect(path, equals(['B']));
    });

    test('avoids disabled edge and finds alternative path', () {
      final map = _branchingMap();
      // Disable the direct B→D edge; force path through C.
      map.disableEdge('B', 'D');
      final path = AStar.findPath(map, 'A', 'D');
      expect(path, isNotNull);
      expect(path, equals(['A', 'B', 'C', 'D']));
    });

    test('returns null when all paths to goal are disabled', () {
      final map = _linearMap();
      // The only route to D is through C; disable C→D in both directions.
      map.disableEdge('C', 'D');
      final path = AStar.findPath(map, 'A', 'D');
      expect(path, isNull);
    });
  });
}
