import 'dart:math';
import 'dart:ui';

/// A node in the road graph with metric coordinates.
class Node {
  final String id;
  final double x; // meters
  final double y; // meters
  final String label;

  const Node({
    required this.id,
    required this.x,
    required this.y,
    required this.label,
  });

  factory Node.fromJson(Map<String, dynamic> json) {
    return Node(
      id: json['id'] as String,
      x: (json['x'] as num).toDouble(),
      y: (json['y'] as num).toDouble(),
      label: json['label'] as String? ?? json['id'] as String,
    );
  }
}

/// A directed or bidirectional edge between two nodes.
class Edge {
  final String from;
  final String to;
  final bool bidirectional;

  const Edge({
    required this.from,
    required this.to,
    this.bidirectional = true,
  });

  factory Edge.fromJson(Map<String, dynamic> json) {
    return Edge(
      from: json['from'] as String,
      to: json['to'] as String,
      bidirectional: json['bidirectional'] as bool? ?? true,
    );
  }

  /// Canonical key for this edge (always smaller-id first for symmetry).
  String get key => _edgeKey(from, to);
}

String _edgeKey(String a, String b) => '$a->$b';

/// The road graph: nodes, edges, and runtime-disabled edges.
class RoadMap {
  final List<Node> nodes;
  final List<Edge> edges;

  /// Set of disabled edge keys (directional: 'from->to').
  final Set<String> disabledEdges;

  RoadMap({
    required this.nodes,
    required this.edges,
    Set<String>? disabledEdges,
  }) : disabledEdges = disabledEdges ?? {};

  factory RoadMap.fromJson(Map<String, dynamic> json) {
    if (!json.containsKey('nodes')) {
      throw FormatException("Missing required field 'nodes' in map JSON");
    }
    final nodeList = (json['nodes'] as List)
        .map((e) => Node.fromJson(e as Map<String, dynamic>))
        .toList();
    final edgeList = (json['edges'] as List? ?? [])
        .map((e) => Edge.fromJson(e as Map<String, dynamic>))
        .toList();
    return RoadMap(nodes: nodeList, edges: edgeList);
  }

  factory RoadMap.empty() => RoadMap(nodes: [], edges: []);

  /// Map from nodeId → Node for fast lookup (cached).
  late final Map<String, Node> _nodeMap = {for (final n in nodes) n.id: n};

  /// Returns neighbors of [nodeId] respecting disabled edges.
  List<String> getNeighbors(String nodeId) {
    final result = <String>[];
    for (final edge in edges) {
      if (edge.from == nodeId) {
        final key = _edgeKey(edge.from, edge.to);
        if (!disabledEdges.contains(key)) result.add(edge.to);
      }
      if (edge.bidirectional && edge.to == nodeId) {
        final key = _edgeKey(edge.to, edge.from);
        if (!disabledEdges.contains(key)) result.add(edge.from);
      }
    }
    return result;
  }

  /// Disables traversal from [from] to [to] (and reverse if bidirectional).
  void disableEdge(String from, String to) {
    disabledEdges.add(_edgeKey(from, to));
    // Also disable reverse direction if a bidirectional edge exists.
    for (final e in edges) {
      if (e.bidirectional &&
          ((e.from == from && e.to == to) || (e.from == to && e.to == from))) {
        disabledEdges.add(_edgeKey(to, from));
        break;
      }
    }
  }

  /// Re-enables traversal from [from] to [to] (and reverse).
  void enableEdge(String from, String to) {
    disabledEdges.remove(_edgeKey(from, to));
    disabledEdges.remove(_edgeKey(to, from));
  }

  /// Clears all disabled edges.
  void enableAllEdges() => disabledEdges.clear();

  /// Returns the screen-space Offset of [nodeId] in metric coordinates.
  Offset getNodePosition(String nodeId) {
    final node = _nodeMap[nodeId];
    if (node == null) throw ArgumentError('Node $nodeId not found');
    return Offset(node.x, node.y);
  }

  /// Returns the id of the node nearest to (x, y) by Euclidean distance.
  /// Returns null if the map has no nodes.
  String? getNearestNode(double x, double y) {
    if (nodes.isEmpty) return null;
    String? nearest;
    double bestDist = double.infinity;
    for (final node in nodes) {
      final dx = node.x - x;
      final dy = node.y - y;
      final dist = sqrt(dx * dx + dy * dy);
      if (dist < bestDist) {
        bestDist = dist;
        nearest = node.id;
      }
    }
    return nearest;
  }

  /// Bounding box helpers for coordinate transforms.
  double get maxX => nodes.isEmpty ? 1.0 : nodes.map((n) => n.x).reduce(max);
  double get maxY => nodes.isEmpty ? 1.0 : nodes.map((n) => n.y).reduce(max);
}
