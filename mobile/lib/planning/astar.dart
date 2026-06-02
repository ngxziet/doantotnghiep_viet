import 'dart:math';
import '../models/map_data.dart';

/// A* pathfinding on a [RoadMap] graph.
///
/// Heuristic: Euclidean distance between node positions (admissible because
/// the straight-line distance never overestimates actual travel distance).
class AStar {
  AStar._(); // static-only class

  static const double _epsilon = 1e-6;
  static const double _turnAngleEpsilonRad = 0.15;

  /// Finds the shortest path from [start] to [goal] on [map].
  ///
  /// Returns an ordered list of node IDs (inclusive of start and goal),
  /// [start] alone when start == goal, or null when no path exists.
  static List<String>? findPath(RoadMap map, String start, String goal) {
    if (start == goal) return [start];

    // g-score: actual cost from start to this node.
    final gScore = <String, double>{start: 0.0};

    // f-score: g + heuristic estimate to goal.
    final fScore = <String, double>{start: _heuristic(map, start, goal)};

    // turn-score: secondary cost used only when distance is effectively tied.
    final turnScore = <String, int>{start: 0};

    // Predecessor map for path reconstruction.
    final cameFrom = <String, String>{};

    // Open set implemented as a simple list — map is small (< 100 nodes).
    final openSet = <String>{start};

    while (openSet.isNotEmpty) {
      // Pick node with lowest f-score.
      final current = openSet.reduce(
        (a, b) => _isBetterOpenNode(a, b, fScore, turnScore) ? a : b,
      );

      if (current == goal) return _reconstructPath(cameFrom, current);

      openSet.remove(current);

      for (final neighbor in map.getNeighbors(current)) {
        // Edge cost = Euclidean distance between the two node positions.
        final tentativeG = (gScore[current] ?? double.infinity) +
            _heuristic(map, current, neighbor);
        final tentativeTurns = (turnScore[current] ?? 0) +
            _turnPenalty(map, cameFrom[current], current, neighbor);
        final currentBestG = gScore[neighbor] ?? double.infinity;
        final currentBestTurns = turnScore[neighbor] ?? 1 << 30;

        if (tentativeG < currentBestG - _epsilon ||
            ((tentativeG - currentBestG).abs() <= _epsilon &&
                tentativeTurns < currentBestTurns)) {
          cameFrom[neighbor] = current;
          gScore[neighbor] = tentativeG;
          fScore[neighbor] = tentativeG + _heuristic(map, neighbor, goal);
          turnScore[neighbor] = tentativeTurns;
          openSet.add(neighbor);
        }
      }
    }

    // Open set exhausted — no path found.
    return null;
  }

  static double _heuristic(RoadMap map, String a, String b) {
    final pa = map.getNodePosition(a);
    final pb = map.getNodePosition(b);
    final dx = pa.dx - pb.dx;
    final dy = pa.dy - pb.dy;
    return sqrt(dx * dx + dy * dy);
  }

  static bool _isBetterOpenNode(
    String a,
    String b,
    Map<String, double> fScore,
    Map<String, int> turnScore,
  ) {
    final fa = fScore[a] ?? double.infinity;
    final fb = fScore[b] ?? double.infinity;
    if ((fa - fb).abs() > _epsilon) return fa < fb;
    return (turnScore[a] ?? 1 << 30) <= (turnScore[b] ?? 1 << 30);
  }

  static int _turnPenalty(
    RoadMap map,
    String? previous,
    String current,
    String next,
  ) {
    if (previous == null) return 0;

    final p0 = map.getNodePosition(previous);
    final p1 = map.getNodePosition(current);
    final p2 = map.getNodePosition(next);
    final a1 = atan2(p1.dy - p0.dy, p1.dx - p0.dx);
    final a2 = atan2(p2.dy - p1.dy, p2.dx - p1.dx);
    var diff = (a2 - a1).abs();
    while (diff > pi) {
      diff = (diff - 2 * pi).abs();
    }

    return diff > _turnAngleEpsilonRad ? 1 : 0;
  }

  static List<String> _reconstructPath(
    Map<String, String> cameFrom,
    String current,
  ) {
    final path = [current];
    String node = current;
    while (cameFrom.containsKey(node)) {
      node = cameFrom[node]!;
      path.insert(0, node);
    }
    return path;
  }
}
