#include "udp_server.h"
#include <string.h>
#include <stdlib.h>

void UdpServer::begin(const char* ssid, const char* password) {
    WiFi.mode(WIFI_AP);
    WiFi.setSleep(false);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1),
                      IPAddress(192, 168, 4, 1),
                      IPAddress(255, 255, 255, 0));
    WiFi.softAP(ssid, password, 6);
    _udpRx.begin(UDP_RECV_PORT);
}

void UdpServer::sendTelemetry(float x, float y, float theta,
                              float obstDistCm, const char* status,
                              bool imuConnected, bool calibrated,
                              int nodeIndex, int routeSize) {
    float headingDeg = theta * (180.0f / PI);
    snprintf(_txBuf, sizeof(_txBuf),
             "{\"pose\":{\"x\":%.4f,\"y\":%.4f,\"theta\":%.4f},"
             "\"heading_deg\":%.1f,\"node_index\":%d,\"route_size\":%d,"
             "\"obstacle_dist_cm\":%.1f,\"status\":\"%s\","
             "\"imu_connected\":%s,\"calibrated\":%s}",
             x, y, theta, headingDeg, nodeIndex, routeSize,
             obstDistCm, status,
             imuConnected ? "true" : "false",
             calibrated ? "true" : "false");

    IPAddress broadcast(192, 168, 4, 255);
    _udpTx.beginPacket(broadcast, UDP_SEND_PORT);
    _udpTx.write((const uint8_t*)_txBuf, strlen(_txBuf));
    _udpTx.endPacket();
}

// --- Minimal JSON helpers (no heap allocation, no external lib) --------------

bool UdpServer::_extractFloat(const char* json, const char* key, float* value) {
    // Find "key": in the string
    const char* p = strstr(json, key);
    if (!p) return false;
    p += strlen(key);
    // Skip whitespace and colon
    while (*p == ' ' || *p == ':') p++;
    if (*p == '\0') return false;
    char* end = nullptr;
    float v = strtof(p, &end);
    if (end == p) return false;
    *value = v;
    return true;
}

bool UdpServer::_extractInt(const char* json, const char* key, int* value) {
    const char* p = strstr(json, key);
    if (!p) return false;
    p += strlen(key);
    while (*p == ' ' || *p == ':') p++;
    if (*p == '\0') return false;
    char* end = nullptr;
    long v = strtol(p, &end, 10);
    if (end == p) return false;
    *value = (int)v;
    return true;
}

bool UdpServer::_extractString(const char* json, const char* key,
                               char* value, int valueLen) {
    const char* p = strstr(json, key);
    if (!p || valueLen <= 0) return false;
    p += strlen(key);
    while (*p == ' ' || *p == ':') p++;
    if (*p != '"') return false;
    p++;

    int i = 0;
    while (*p != '\0' && *p != '"' && i < valueLen - 1) {
        value[i++] = *p++;
    }
    value[i] = '\0';
    return *p == '"';
}

void UdpServer::_resetRouteBuffer(int routeSeq, int total) {
    _routeSeq = routeSeq;
    _routeTotal = constrain(total, 0, UDP_MAX_WAYPOINTS);
    _routeReceived = 0;

    for (int i = 0; i < UDP_MAX_WAYPOINTS; i++) {
        _routeSlots[i] = false;
        _routeXs[i] = 0.0f;
        _routeYs[i] = 0.0f;
    }
}

int UdpServer::_parseWaypoints(float waypointXs[], float waypointYs[],
                               int maxCount) {
    int wpCount = 0;
    const char* cursor = strstr(_rxBuf, "\"waypoints\"");
    if (!cursor) return 0;

    while (wpCount < maxCount) {
        cursor = strchr(cursor, '{');
        if (!cursor) break;

        const char* objEnd = strchr(cursor, '}');
        if (!objEnd) break;

        char obj[64];
        int objLen = (int)(objEnd - cursor + 1);
        if (objLen >= (int)sizeof(obj)) {
            cursor = objEnd + 1;
            continue;
        }
        memcpy(obj, cursor, objLen);
        obj[objLen] = '\0';

        float wx = 0.0f, wy = 0.0f;
        bool hasX = _extractFloat(obj, "\"x\"", &wx);
        bool hasY = _extractFloat(obj, "\"y\"", &wy);

        if (hasX && hasY) {
            waypointXs[wpCount] = wx;
            waypointYs[wpCount] = wy;
            wpCount++;
        }

        cursor = objEnd + 1;
    }

    return wpCount;
}

// Parse: {"seq":N,"waypoints":[{"x":X,"y":Y},...]}
bool UdpServer::receiveCommand(float waypointXs[], float waypointYs[], UdpCommand* command) {
    bool gotCommand = false;

    int packetSize = 0;
    while ((packetSize = _udpRx.parsePacket()) > 0) {
        int len = _udpRx.read(_rxBuf, sizeof(_rxBuf) - 1);
        if (len <= 0) continue;
        _rxBuf[len] = '\0';

        if (packetSize >= (int)sizeof(_rxBuf)) {
            Serial.printf("UDP command too large: %d bytes, max=%u\n",
                          packetSize, (unsigned)(sizeof(_rxBuf) - 1));
            continue;
        }

        gotCommand = _parseCommandBuffer(waypointXs, waypointYs, command) || gotCommand;
    }

    return gotCommand;
}

bool UdpServer::_parseCommandBuffer(float waypointXs[], float waypointYs[],
                                    UdpCommand* command) {
    if (!command) return false;

    // Extract sequence number
    int seq = 0;
    if (!_extractInt(_rxBuf, "\"seq\"", &seq)) return false;

    // Deduplicate: ignore if same seq as last received
    if (seq == _lastSeq) return false;
    _lastSeq = seq;
    *command = UdpCommand{};
    command->seq = seq;

    char type[24] = "";
    _extractString(_rxBuf, "\"type\"", type, sizeof(type));

    if (strcmp(type, "reset_pose") == 0) {
        command->type = UdpCommandType::ResetPose;
        return true;
    } else if (strcmp(type, "manual") == 0) {
        command->type = UdpCommandType::Manual;
        _extractString(_rxBuf, "\"command\"", command->manualCommand,
                       sizeof(command->manualCommand));
        _extractInt(_rxBuf, "\"speed\"", &command->manualSpeed);
        return true;
    } else if (strcmp(type, "step") == 0) {
        command->type = UdpCommandType::Step;
        _extractString(_rxBuf, "\"action\"", command->stepAction,
                       sizeof(command->stepAction));
        return true;
    } else if (strcmp(type, "calibrate") == 0) {
        command->type = UdpCommandType::Calibrate;
        _extractString(_rxBuf, "\"mode\"", command->calibrateMode,
                       sizeof(command->calibrateMode));
        return true;
    } else if (strcmp(type, "route_chunk") == 0) {
        int routeSeq = 0, chunkIndex = 0, chunkCount = 0, start = 0, total = 0;
        if (!_extractInt(_rxBuf, "\"route_seq\"", &routeSeq) ||
            !_extractInt(_rxBuf, "\"chunk_index\"", &chunkIndex) ||
            !_extractInt(_rxBuf, "\"chunk_count\"", &chunkCount) ||
            !_extractInt(_rxBuf, "\"start\"", &start) ||
            !_extractInt(_rxBuf, "\"total\"", &total)) {
            return false;
        }

        if (total < 0 || total > UDP_MAX_WAYPOINTS ||
            start < 0 || start >= UDP_MAX_WAYPOINTS ||
            chunkIndex < 0 || chunkIndex >= chunkCount) {
            Serial.printf("Invalid route chunk: route=%d start=%d total=%d chunk=%d/%d\n",
                          routeSeq, start, total, chunkIndex, chunkCount);
            return false;
        }

        if (routeSeq != _routeSeq || total != _routeTotal) {
            _resetRouteBuffer(routeSeq, total);
        }

        float chunkXs[UDP_MAX_WAYPOINTS];
        float chunkYs[UDP_MAX_WAYPOINTS];
        int chunkSize = _parseWaypoints(chunkXs, chunkYs, UDP_MAX_WAYPOINTS);

        for (int i = 0; i < chunkSize && start + i < _routeTotal; i++) {
            int slot = start + i;
            _routeXs[slot] = chunkXs[i];
            _routeYs[slot] = chunkYs[i];
            if (!_routeSlots[slot]) {
                _routeSlots[slot] = true;
                _routeReceived++;
            }
        }

        Serial.printf("Route chunk: route=%d chunk=%d/%d received=%d/%d\n",
                      routeSeq, chunkIndex + 1, chunkCount,
                      _routeReceived, _routeTotal);

        if (_routeReceived < _routeTotal) {
            return false;
        }

        for (int i = 0; i < _routeTotal; i++) {
            waypointXs[i] = _routeXs[i];
            waypointYs[i] = _routeYs[i];
        }

        command->seq = routeSeq;
        command->type = UdpCommandType::Route;
        command->waypointCount = _routeTotal;
        _resetRouteBuffer(-1, 0);
        return true;
    }

    if (!strstr(_rxBuf, "\"waypoints\"")) {
        command->type = UdpCommandType::Ping;
        return true; // valid seq update, zero waypoints
    }

    command->type = UdpCommandType::Route;
    command->waypointCount = _parseWaypoints(waypointXs, waypointYs, UDP_MAX_WAYPOINTS);
    if (command->waypointCount <= 0) {
        command->type = UdpCommandType::Ping;
    }
    return true;
}
