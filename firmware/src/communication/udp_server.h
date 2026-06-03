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

    // Tạo WiFi Access Point với SSID/password, bắt đầu lắng nghe UDP trên RECV_PORT.
    // IP cố định: 192.168.4.1.
    void begin(const char* ssid, const char* password);

    // Chuyển đổi pose + khoảng cách vật cản + trạng thái thành JSON
    // và broadcast đến 192.168.4.255:UDP_SEND_PORT.
    void sendTelemetry(float x, float y, float theta,
                       float obstDistCm, const char* status,
                       bool imuConnected, bool calibrated,
                       int nodeIndex, int routeSize);

    // Phân tích lệnh JSON đến. Điền waypointXs/Ys (mảng phẳng) và số lượng.
    // Chỉ trả về true khi nhận lệnh MỚI (seq khác lần trước).
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

    // Bộ đệm gửi/nhận tái sử dụng
    char _txBuf[384];
    char _rxBuf[2048];

    // Hàm trích xuất JSON đơn giản (không cần thư viện ngoài)
    // Trả về true và gán *value khi thành công.
    static bool _extractFloat(const char* json, const char* key, float* value);
    static bool _extractInt(const char* json, const char* key, int* value);
    static bool _extractString(const char* json, const char* key,
                               char* value, int valueLen);
    int _parseWaypoints(float waypointXs[], float waypointYs[], int maxCount);
    void _resetRouteBuffer(int routeSeq, int total);
    bool _parseCommandBuffer(float waypointXs[], float waypointYs[], UdpCommand* command);
};
