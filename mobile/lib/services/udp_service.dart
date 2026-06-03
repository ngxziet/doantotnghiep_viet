import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:ui';

import 'package:udp/udp.dart';

import '../models/robot_state.dart';

/// Abstract transport interface; allows swapping UDP for simulator in tests.
abstract class ITransportService {
  /// Stream of telemetry snapshots received from the robot.
  Stream<RobotState> get telemetryStream;

  /// Stream of connection status (true = connected, false = lost).
  Stream<bool> get connectionStream;

  /// Whether we are currently connected.
  bool get isConnected;

  /// Sends [waypoints] to the robot with a sequence number [seq].
  Future<void> sendWaypoints(List<Offset> waypoints, int seq);

  /// Sends direct wheel-control command for setup/manual testing.
  Future<void> sendManualCommand(String command, int speed, int seq);

  /// Sends one discrete movement test step to the firmware.
  Future<void> sendStepCommand(String action, int seq);

  /// Starts a firmware-side calibration action.
  Future<void> sendCalibrationCommand(String mode, int seq);

  /// Resets robot pose/odometry to the map origin.
  Future<void> resetPose(int seq);

  /// Releases all resources.
  void dispose();
}

/// UDP transport to the ESP32 running in WiFi AP mode.
///
/// ESP32 AP: SSID `RobotCar`, IP `192.168.4.1`.
/// Telemetry arrives on [recvPort]; commands are sent to [sendPort].
class UdpService implements ITransportService {
  static const String esp32Ip = '192.168.4.1';
  static const int recvPort = 4210;
  static const int sendPort = 4211;
  static const Duration _connectionTimeout = Duration(seconds: 12);
  static const Duration _connectionCheckInterval = Duration(seconds: 4);

  final _controller = StreamController<RobotState>.broadcast();
  final _connController = StreamController<bool>.broadcast();
  UDP? _receiver;
  UDP? _sender;
  StreamSubscription<Datagram?>? _subscription;
  Future<void> _sendQueue = Future<void>.value();
  bool _disposed = false;
  DateTime _lastRxTime = DateTime.now();
  bool _connected = false;
  Timer? _connectionTimer;

  UdpService() {
    _bind();
    _startConnectionMonitor();
  }

  @override
  Stream<RobotState> get telemetryStream => _controller.stream;

  @override
  Stream<bool> get connectionStream => _connController.stream;

  @override
  bool get isConnected => _connected;

  Future<void> _bind() async {
    try {
      _receiver = await UDP.bind(Endpoint.any(port: const Port(recvPort)));
      _sender = await UDP.bind(Endpoint.any());
      _subscription = _receiver!.asStream(timeout: null).listen(_onDatagram);
    } catch (e) {
      if (!_controller.isClosed) _controller.addError(e);
    }
  }

  void _onDatagram(Datagram? datagram) {
    if (datagram == null || _disposed) return;
    _lastRxTime = DateTime.now();
    _setConnected(true);
    try {
      final raw = utf8.decode(datagram.data);
      final decoded = jsonDecode(raw) as Map<String, dynamic>;
      final state = RobotState.fromJson(decoded);
      if (state != null && !_controller.isClosed) _controller.add(state);
    } catch (_) {
      // Ignore malformed UDP packets.
    }
  }

  void _setConnected(bool value) {
    if (_connected != value) {
      _connected = value;
      if (!_connController.isClosed) _connController.add(value);
    }
  }

  int _rebindCount = 0;

  void _startConnectionMonitor() {
    _connectionTimer = Timer.periodic(_connectionCheckInterval, (_) {
      if (_disposed) return;
      final elapsed = DateTime.now().difference(_lastRxTime);
      if (elapsed > _connectionTimeout && _connected) {
        _setConnected(false);
        // Chỉ rebind sau 2 lần check liên tiếp mất kết nối
        // tránh rebind quá sớm khi chỉ mất sóng thoáng qua
        _rebindCount++;
        if (_rebindCount >= 2) {
          _rebind();
          _rebindCount = 0;
        }
      } else if (!_connected && elapsed > _connectionTimeout) {
        // Đã disconnect, thử rebind định kỳ
        _rebindCount++;
        if (_rebindCount >= 3) {
          _rebind();
          _rebindCount = 0;
        }
      } else {
        _rebindCount = 0;
      }
    });
  }

  Future<void> _rebind() async {
    if (_disposed) return;
    try {
      _subscription?.cancel();
      _receiver?.close();
      _sender?.close();
      _receiver = null;
      _sender = null;
      await _bind();
    } catch (_) {}
  }

  @override
  Future<void> sendWaypoints(List<Offset> waypoints, int seq) async {
    final payload = jsonEncode({
      'seq': seq,
      'type': 'route',
      'waypoints': [
        for (final point in waypoints) _encodePoint(point),
      ],
    });
    await _sendReliable(payload);
  }

  @override
  Future<void> sendManualCommand(String command, int speed, int seq) async {
    final payload = jsonEncode({
      'seq': seq,
      'type': 'manual',
      'command': command,
      'speed': speed,
    });
    await _sendReliable(payload, retries: 1);
  }

  @override
  Future<void> sendStepCommand(String action, int seq) async {
    final payload = jsonEncode({
      'seq': seq,
      'type': 'step',
      'action': action,
    });
    await _sendReliable(payload);
  }

  @override
  Future<void> sendCalibrationCommand(String mode, int seq) async {
    final payload = jsonEncode({
      'seq': seq,
      'type': 'calibrate',
      'mode': mode,
    });
    await _sendReliable(payload);
  }

  @override
  Future<void> resetPose(int seq) async {
    final payload = jsonEncode({
      'seq': seq,
      'type': 'reset_pose',
    });
    await _sendReliable(payload);
  }

  Map<String, double> _encodePoint(Offset point) {
    return {
      'x': double.parse(point.dx.toStringAsFixed(3)),
      'y': double.parse(point.dy.toStringAsFixed(3)),
    };
  }

  Future<void> _sendReliable(String payload, {int retries = 2}) {
    if (_disposed) return Future<void>.value();

    _sendQueue = _sendQueue
        .catchError((_) {})
        .then((_) => _sendPacketBurst(payload, retries: retries));
    return _sendQueue;
  }

  Future<void> _sendPacketBurst(String payload, {int retries = 2}) async {
    if (_disposed) return;

    _sender ??= await UDP.bind(Endpoint.any());
    final sender = _sender;
    if (sender == null || _disposed) return;

    final endpoint = Endpoint.unicast(
      InternetAddress(esp32Ip),
      port: const Port(sendPort),
    );
    final bytes = utf8.encode(payload);

    for (int i = 0; i < retries; i++) {
      if (_disposed) return;
      await sender.send(bytes, endpoint);
      if (i < retries - 1) {
        await Future<void>.delayed(const Duration(milliseconds: 70));
      }
    }
  }

  @override
  void dispose() {
    _disposed = true;
    _connectionTimer?.cancel();
    _subscription?.cancel();
    _receiver?.close();
    _sender?.close();
    _controller.close();
    _connController.close();
  }
}
