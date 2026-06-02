import 'package:flutter_test/flutter_test.dart';
import 'package:mobile/models/map_data.dart';

/// Valid 4-node, 3-edge JSON matching assets/environment.json.
Map<String, dynamic> _validJson() => {
      'nodes': [
        {'id': 'A', 'x': 0.0, 'y': 0.0, 'label': 'Start'},
        {'id': 'B', 'x': 0.5, 'y': 0.0, 'label': 'Junction 1'},
        {'id': 'C', 'x': 0.5, 'y': 0.5, 'label': 'Junction 2'},
        {'id': 'D', 'x': 1.0, 'y': 0.0, 'label': 'End'},
      ],
      'edges': [
        {'from': 'A', 'to': 'B', 'bidirectional': true},
        {'from': 'B', 'to': 'C', 'bidirectional': true},
        {'from': 'B', 'to': 'D', 'bidirectional': true},
      ],
    };

void main() {
  group('RoadMap.fromJson', () {
    test('loads 4 nodes and 3 edges from valid JSON', () {
      final map = RoadMap.fromJson(_validJson());
      expect(map.nodes.length, 4);
      expect(map.edges.length, 3);
      expect(map.nodes.map((n) => n.id).toList(),
          containsAll(['A', 'B', 'C', 'D']));
    });

    test('throws FormatException when nodes field is missing', () {
      expect(
        () => RoadMap.fromJson({'edges': []}),
        throwsA(isA<FormatException>()),
      );
    });
  });

  group('RoadMap.getNeighbors', () {
    late RoadMap map;

    setUp(() => map = RoadMap.fromJson(_validJson()));

    test('returns correct adjacent nodes for B (bidirectional edges)', () {
      final neighbors = map.getNeighbors('B');
      expect(neighbors, containsAll(['A', 'C', 'D']));
      expect(neighbors.length, 3);
    });

    test('returns empty list for isolated node', () {
      // Add isolated node with no edges.
      final iso = RoadMap(
        nodes: [...map.nodes, Node(id: 'Z', x: 9.0, y: 9.0, label: 'Z')],
        edges: map.edges,
      );
      expect(iso.getNeighbors('Z'), isEmpty);
    });
  });

  group('RoadMap edge toggling', () {
    late RoadMap map;

    setUp(() => map = RoadMap.fromJson(_validJson()));

    test('disableEdge removes node from neighbors', () {
      map.disableEdge('B', 'D');
      expect(map.getNeighbors('B'), isNot(contains('D')));
      // Bidirectional: reverse direction also disabled.
      expect(map.getNeighbors('D'), isNot(contains('B')));
    });

    test('enableEdge restores neighbor after disable', () {
      map.disableEdge('B', 'D');
      map.enableEdge('B', 'D');
      expect(map.getNeighbors('B'), contains('D'));
      expect(map.getNeighbors('D'), contains('B'));
    });

    test('enableAllEdges clears all disabled edges', () {
      map.disableEdge('A', 'B');
      map.disableEdge('B', 'C');
      map.enableAllEdges();
      expect(map.disabledEdges, isEmpty);
      expect(map.getNeighbors('A'), contains('B'));
    });
  });

  group('RoadMap.getNearestNode', () {
    late RoadMap map;

    setUp(() => map = RoadMap.fromJson(_validJson()));

    test('returns closest node by Euclidean distance', () {
      // Point very close to C (0.5, 0.5).
      expect(map.getNearestNode(0.49, 0.49), equals('C'));
    });

    test('returns null for empty map', () {
      expect(RoadMap.empty().getNearestNode(0.0, 0.0), isNull);
    });
  });
}
