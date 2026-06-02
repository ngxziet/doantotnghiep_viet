import 'dart:ui';
import '../models/map_data.dart';

/// Converts a node-ID path from A* into metric [Offset] waypoints.
class WaypointBuilder {
  WaypointBuilder._(); // static-only class

  /// Maps each node ID in [nodePath] to its [Offset(x, y)] in meters.
  /// Returns an empty list for an empty path.
  static List<Offset> build(RoadMap map, List<String> nodePath) {
    return nodePath.map((id) => map.getNodePosition(id)).toList();
  }
}
