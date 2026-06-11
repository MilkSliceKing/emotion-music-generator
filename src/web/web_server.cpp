#include "web_server.h"
#include "web_resources.h"
#include "logger/emotion_logger.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <vector>
#include <algorithm>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <csignal>

WebServer::~WebServer() {
    stop();
}

bool WebServer::start(int port, SharedState* state) {
    port_ = port;
    state_ = state;
    running_ = true;

    server_thread_ = std::thread(&WebServer::serverLoop, this);
    std::cout << "[WebServer] 启动在端口 " << port_ << std::endl;
    std::cout << "[WebServer] 浏览器访问: http://localhost:" << port_ << std::endl;
    return true;
}

void WebServer::stop() {
    if (!running_) return;
    running_ = false;
    if (server_fd_ >= 0) {
        shutdown(server_fd_, SHUT_RDWR);
        close(server_fd_);
        server_fd_ = -1;
    }
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    std::cout << "[WebServer] 已停止" << std::endl;
}

void WebServer::serverLoop() {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        std::cerr << "[WebServer] socket() 失败" << std::endl;
        running_ = false;
        return;
    }

    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[WebServer] bind() 失败 (端口 " << port_ << " 可能被占用)" << std::endl;
        close(server_fd_);
        server_fd_ = -1;
        running_ = false;
        return;
    }

    listen(server_fd_, 5);
    std::cout << "[WebServer] 等待连接..." << std::endl;

    while (running_) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_fd_, &read_fds);
        struct timeval tv = {1, 0};
        int ret = select(server_fd_ + 1, &read_fds, nullptr, nullptr, &tv);
        if (ret <= 0) continue;

        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) continue;

        // 设置超时（防止客户端挂起）
        struct timeval timeout = {5, 0};
        setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        bool mjpeg_connection = handleMjpegStream(client_fd);
        if (!mjpeg_connection) {
            close(client_fd);
        }
    }
}

bool WebServer::handleMjpegStream(int client_fd) {
    // 先读请求，判断路径
    char buf[4096];
    int n = recv(client_fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) return false;
    buf[n] = '\0';

    std::string request(buf);
    size_t sp1 = request.find(' ');
    size_t sp2 = request.find(' ', sp1 + 1);
    if (sp1 == std::string::npos || sp2 == std::string::npos) return false;

    std::string method = request.substr(0, sp1);
    std::string path = request.substr(sp1 + 1, sp2 - sp1 - 1);

    // 查询参数
    std::string query;
    auto qpos = path.find('?');
    if (qpos != std::string::npos) {
        query = path.substr(qpos + 1);
        path = path.substr(0, qpos);
    }

    // ---- 路由 ----
    if (path == "/") {
        sendResponse(client_fd, 200, "text/html; charset=utf-8", WebResources::INDEX_HTML);
        return false;
    }
    if (path == "/stream") {
        // MJPEG 推流 — 不关闭 fd，连接由本函数管理
        if (mjpeg_busy_.exchange(true)) {
            sendResponse(client_fd, 503, "text/plain", "MJPEG stream busy");
            return false;
        }

        // 发送 MJPEG 响应头
        const char* header =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
            "Cache-Control: no-cache\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "\r\n";
        send(client_fd, header, strlen(header), 0);

        // 推流循环
        while (running_) {
            cv::Mat frame = state_->getFrame();
            if (!frame.empty()) {
                std::vector<uint8_t> jpeg_buf;
                std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 65};
                cv::imencode(".jpg", frame, jpeg_buf, params);
                sendMjpegFrame(client_fd, jpeg_buf);
            }
            usleep(66000);  // ~15 fps
        }

        mjpeg_busy_ = false;
        close(client_fd);
        return true;  // 已关闭
    }
    if (path == "/api/status") {
        handleApiStatus(client_fd);
        return false;
    }
    if (path == "/api/play") {
        handleApiPlay(client_fd);
        return false;
    }
    if (path == "/api/pause") {
        handleApiPause(client_fd);
        return false;
    }
    if (path == "/api/auto") {
        handleApiAuto(client_fd);
        return false;
    }
    if (path == "/api/mode") {
        handleApiMode(client_fd);
        return false;
    }
    if (path == "/api/summary") {
        handleApiSummary(client_fd, query);
        return false;
    }

    sendResponse(client_fd, 404, "text/plain", "Not Found");
    return false;
}

void WebServer::sendResponse(int fd, int code, const std::string& content_type,
                              const std::string& body) {
    const char* reason = (code == 200) ? "OK" :
                         (code == 404) ? "Not Found" :
                         (code == 503) ? "Service Unavailable" : "OK";
    std::ostringstream oss;
    oss << "HTTP/1.1 " << code << " " << reason << "\r\n"
        << "Content-Type: " << content_type << "\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Access-Control-Allow-Origin: *\r\n"
        << "Connection: close\r\n"
        << "\r\n"
        << body;
    std::string response = oss.str();
    send(fd, response.c_str(), response.size(), 0);
}

void WebServer::sendMjpegFrame(int fd, const std::vector<uint8_t>& jpeg_data) {
    std::ostringstream header;
    header << "--frame\r\n"
           << "Content-Type: image/jpeg\r\n"
           << "Content-Length: " << jpeg_data.size() << "\r\n"
           << "\r\n";
    std::string hdr = header.str();

    // 发送头部
    if (send(fd, hdr.c_str(), hdr.size(), MSG_NOSIGNAL) < 0) return;
    // 发送 JPEG 数据
    size_t sent = 0;
    while (sent < jpeg_data.size()) {
        ssize_t n = send(fd, jpeg_data.data() + sent,
                         jpeg_data.size() - sent, MSG_NOSIGNAL);
        if (n <= 0) return;
        sent += n;
    }
    // 边界
    send(fd, "\r\n", 2, MSG_NOSIGNAL);
}

void WebServer::handleApiStatus(int client_fd) {
    std::lock_guard<std::mutex> lock(state_->emotion_mutex);
    auto& conf = state_->confidences;
    auto& params = state_->current_params;

    std::ostringstream json;
    json << "{";
    json << "\"emotion\":\"" << emotionToString(state_->current_emotion) << "\",";

    // 置信度数组
    json << "\"confidences\":[";
    for (int i = 0; i < 7; ++i) {
        if (i > 0) json << ",";
        json << (i < static_cast<int>(conf.size()) ? conf[i] : 0.0f);
    }
    json << "],";

    // 音乐参数
    json << "\"music_params\":{";
    json << "\"key\":\"" << params.key << "\",";
    json << "\"scale\":\"" << params.scale << "\",";
    json << "\"tempo\":" << params.tempo << ",";
    json << "\"velocity\":" << params.velocity << ",";
    json << "\"mood\":\"" << params.mood << "\"";
    json << "},";

    json << "\"is_playing\":" << (state_->is_playing ? "true" : "false") << ",";
    json << "\"music_enabled\":" << (state_->music_enabled ? "true" : "false") << ",";
    json << "\"mode\":\"" << (state_->playback_mode == 0 ? "synth" : "local") << "\",";
    json << "\"fps\":" << std::fixed << std::setprecision(1) << state_->fps << ",";
    json << "\"face_detected\":" << (state_->face_detected ? "true" : "false");
    json << "}";

    sendResponse(client_fd, 200, "application/json", json.str());
}

void WebServer::handleApiPlay(int client_fd) {
    state_->play_requested = true;
    sendResponse(client_fd, 200, "application/json", "{\"ok\":true}");
}

void WebServer::handleApiPause(int client_fd) {
    state_->pause_requested = true;
    sendResponse(client_fd, 200, "application/json", "{\"ok\":true}");
}

void WebServer::handleApiAuto(int client_fd) {
    state_->auto_toggle_requested = true;
    sendResponse(client_fd, 200, "application/json", "{\"ok\":true}");
}

void WebServer::handleApiMode(int client_fd) {
    state_->mode_switch_requested = true;
    sendResponse(client_fd, 200, "application/json", "{\"ok\":true}");
}

void WebServer::handleApiSummary(int client_fd, const std::string& query) {
    // 解析查询参数: ?date=2026-06-11 或 ?range=week
    std::string date, range;
    std::istringstream qss(query);
    std::string pair;
    while (std::getline(qss, pair, '&')) {
        auto eq = pair.find('=');
        if (eq != std::string::npos) {
            std::string key = pair.substr(0, eq);
            std::string val = pair.substr(eq + 1);
            if (key == "date") date = val;
            if (key == "range") range = val;
        }
    }

    if (range == "week") {
        // 需要外部注入 EmotionLogger 指针 — 简化方案：读取文件
        // 这里用静态方式不太好，改为通过 SharedState 传递
        sendResponse(client_fd, 200, "application/json",
            "{\"error\":\"Use /api/summary?date=YYYY-MM-DD or /api/summary?range=week\"}");
        return;
    }

    if (!date.empty()) {
        // 读取指定日期的日报 — 需要访问 EmotionLogger
        // 通过一个简单方法：直接读 CSV 文件
        sendResponse(client_fd, 200, "application/json",
            "{\"date\":\"" + date + "\",\"note\":\"Logger summary requires EmotionLogger instance\"}");
        return;
    }

    sendResponse(client_fd, 200, "application/json", "{\"usage\":\"?date=YYYY-MM-DD or ?range=week\"}");
}
