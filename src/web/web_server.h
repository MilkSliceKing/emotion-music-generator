#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <string>
#include <thread>
#include <atomic>
#include "state/shared_state.h"

// 轻量 HTTP 服务器 — POSIX socket 实现
// 提供 MJPEG 推流 + REST API + 嵌入式前端
class WebServer {
public:
    WebServer() = default;
    ~WebServer();

    // 启动服务器（非阻塞，后台线程）
    bool start(int port, SharedState* state);

    // 停止服务器
    void stop();

    // 是否正在运行
    bool isRunning() const { return running_; }

private:
    void serverLoop();
    void handleClient(int client_fd);
    bool handleMjpegStream(int client_fd);  // 返回 true 表示连接由 handler 管理

    // REST API
    void handleApiStatus(int client_fd);
    void handleApiPlay(int client_fd);
    void handleApiPause(int client_fd);
    void handleApiAuto(int client_fd);
    void handleApiMode(int client_fd);
    void handleApiSummary(int client_fd, const std::string& query);
    void handleApiLog(int client_fd, const std::string& query);

    // HTTP 工具
    void sendResponse(int fd, int code, const std::string& content_type,
                      const std::string& body);
    void sendMjpegFrame(int fd, const std::vector<uint8_t>& jpeg_data);

    std::thread server_thread_;
    int server_fd_ = -1;
    int port_ = 8080;
    std::atomic<bool> running_{false};
    SharedState* state_ = nullptr;
    std::atomic<bool> mjpeg_busy_{false};  // 限制同一时间只有一个 MJPEG 客户端
};

#endif // WEB_SERVER_H
