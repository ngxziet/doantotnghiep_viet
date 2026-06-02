#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "../config.h"

static constexpr int   UDP_RECV_PORT      = 4211;
static constexpr int   UDP_SEND_PORT      = 4210;
static constexpr int   UDP_MAX_WAYPOINTS  = RobotConfig::ROUTE_MAX_WAYPOINTS;

enum class UdpCommandType {
    None,
    Route,
    Manual,
    Step,
    ResetPose,
    Calibrate,
    Ping
};

struct UdpCommand {
    UdpCommandType type = UdpCommandType::None;
    int seq = 0;
    int waypointCount = 0;
    char manualCommand[24] = {};
    int manualSpeed = 0;
    char stepAction[24] = {};
    char calibrateMode[24] = {};
};

class UdpServer {
public:
    UdpServer() = default;

    // Create soft-AP with given SSID/password, start UDP listener on RECV_PORT.
    // AP IP is always 192.168.4.1.
    void begin(const char* ssid, const char* password);

    // Serialize pose + obstacle distance + status string and broadcast to
    // 192.168.4.255:UDP_SEND_PORT.
    // Format: {"pose":{"x":X,"y":Y,"theta":T},"obstacle_dist_cm":D,
    //          "status":"S","imu_connected":true}
    void sendTelemetry(float x, float y, float theta,
                       float obstDistCm, const char* status,
                       bool imuConnected, bool calibrated,
                       int nodeIndex, int routeSize);

    // Parse inbound JSON command. Populates waypointXs/Ys (flat arrays) and count.
    // outSeq receives the command sequence number.
    // Returns true only when a NEW command (seq != last) is received.
    bool receiveCommand(float waypointXs[], float waypointYs[], UdpCommand* command);

private:
    WiFiUDP _udpRx;
    WiFiUDP _udpTx;
    int     _lastSeq = -1;
    int     _routeSeq = -1;
    int     _routeTotal = 0;
    int     _routeReceived = 0;
    bool    _routeSlots[UDP_MAX_WAYPOINTS] = {};
    float   _routeXs[UDP_MAX_WAYPOINTS] = {};
    float   _routeYs[UDP_MAX_WAYPOINTS] = {};

    // Reusable send/receive buffers
    char _txBuf[384];
    char _rxBuf[2048];

    // Simple JSON number extraction helpers (no external lib needed)
    // Returns true and sets *value on success.
    static bool _extractFloat(const char* json, const char* key, float* value);
    static bool _extractInt(const char* json, const char* key, int* value);
    static bool _extractString(const char* json, const char* key,
                               char* value, int valueLen);
    int _parseWaypoints(float waypointXs[], float waypointYs[], int maxCount);
    void _resetRouteBuffer(int routeSeq, int total);
    bool _parseCommandBuffer(float waypointXs[], float waypointYs[], UdpCommand* command);
};
