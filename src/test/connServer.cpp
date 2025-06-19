#include <iostream>
#include <thread>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <sys/socket.h>
#include <poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <fstream>
#include <filesystem>

// 文件传输协议结构
struct FileHeader {
    char filename[256];
    uint64_t filesize;
    uint32_t checksum;
};

struct ClientInfo {
    int fd;
    std::string ip;
    uint16_t port;
    time_t connect_time;
    
    // 文件传输状态
    bool receiving_file;
    FileHeader current_file;
    std::ofstream file_stream;
    uint64_t bytes_received;
    std::string save_path;
    
    ClientInfo(int f, const std::string& i, uint16_t p) 
        : fd(f), ip(i), port(p), connect_time(time(nullptr)),
          receiving_file(false), bytes_received(0) {}
};

class TCPServer {
private:
    int server_fd;
    std::atomic<bool> running;
    std::string save_directory;
    
    // 连接流表 - 线程安全
    std::unordered_map<int, ClientInfo> client_table;
    std::mutex client_table_mutex;
    
    // poll 相关
    std::vector<struct pollfd> poll_fds;
    std::mutex poll_fds_mutex;
    
public:
    TCPServer(int port, const std::string& save_dir = "./uploads") 
        : server_fd(-1), running(false), save_directory(save_dir) {
        // 创建保存目录
        std::filesystem::create_directories(save_directory);
        init_server(port);
    }
    
    ~TCPServer() {
        stop();
    }
    
    bool init_server(int port) {
        // 创建socket
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd == -1) {
            std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
            return false;
        }
        
        // 设置socket选项
        int opt = 1;
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            std::cerr << "Failed to set socket options: " << strerror(errno) << std::endl;
            return false;
        }
        
        // 设置非阻塞
        set_nonblocking(server_fd);
        
        // 绑定地址
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port);
        
        if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "Failed to bind: " << strerror(errno) << std::endl;
            close(server_fd);
            return false;
        }
        
        // 监听
        if (listen(server_fd, SOMAXCONN) < 0) {
            std::cerr << "Failed to listen: " << strerror(errno) << std::endl;
            close(server_fd);
            return false;
        }
        
        // 初始化 poll_fds
        {
            std::lock_guard<std::mutex> lock(poll_fds_mutex);
            struct pollfd server_pollfd;
            server_pollfd.fd = server_fd;
            server_pollfd.events = POLLIN;
            server_pollfd.revents = 0;
            poll_fds.push_back(server_pollfd);
        }
        
        std::cout << "TCP File Server initialized on port " << port << std::endl;
        std::cout << "Files will be saved to: " << save_directory << std::endl;
        return true;
    }
    
    void start() {
        running = true;
        poll_loop();
    }
    
    void stop() {
        running = false;
        
        // 关闭所有客户端连接和文件流
        {
            std::lock_guard<std::mutex> lock(client_table_mutex);
            for (auto& pair : client_table) {
                if (pair.second.file_stream.is_open()) {
                    pair.second.file_stream.close();
                }
                close(pair.second.fd);
            }
            client_table.clear();
        }
        
        if (server_fd != -1) {
            close(server_fd);
            server_fd = -1;
        }
        
        std::cout << "TCP File Server stopped" << std::endl;
    }
    
private:
    void set_nonblocking(int fd) {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags == -1) flags = 0;
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
    
    void add_client_to_poll(int client_fd) {
        std::lock_guard<std::mutex> lock(poll_fds_mutex);
        struct pollfd client_pollfd;
        client_pollfd.fd = client_fd;
        client_pollfd.events = POLLIN;
        client_pollfd.revents = 0;
        poll_fds.push_back(client_pollfd);
    }
    
    void remove_client_from_poll(int client_fd) {
        std::lock_guard<std::mutex> lock(poll_fds_mutex);
        poll_fds.erase(
            std::remove_if(poll_fds.begin(), poll_fds.end(),
                [client_fd](const struct pollfd& pfd) {
                    return pfd.fd == client_fd;
                }),
            poll_fds.end()
        );
    }
    
    void poll_loop() {
        while (running) {
            std::vector<struct pollfd> current_fds;
            
            {
                std::lock_guard<std::mutex> lock(poll_fds_mutex);
                current_fds = poll_fds;
            }
            
            if (current_fds.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            
            int nfds = poll(current_fds.data(), current_fds.size(), 1000);
            
            if (nfds == -1) {
                if (errno == EINTR) continue;
                std::cerr << "poll failed: " << strerror(errno) << std::endl;
                break;
            }
            
            if (nfds == 0) continue;
            
            for (const auto& pfd : current_fds) {
                if (pfd.revents == 0) continue;
                
                if (pfd.fd == server_fd) {
                    if (pfd.revents & POLLIN) {
                        handle_accept();
                    }
                } else {
                    if (pfd.revents & (POLLIN | POLLHUP | POLLERR)) {
                        handle_client_event(pfd);
                    }
                }
            }
        }
    }
    
    void handle_accept() {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        while (true) {
            int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
            if (client_fd == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }
                std::cerr << "Accept failed: " << strerror(errno) << std::endl;
                break;
            }
            
            set_nonblocking(client_fd);
            add_client_to_poll(client_fd);
            
            std::string client_ip = inet_ntoa(client_addr.sin_addr);
            uint16_t client_port = ntohs(client_addr.sin_port);
            
            {
                std::lock_guard<std::mutex> lock(client_table_mutex);
                client_table.emplace(client_fd, ClientInfo(client_fd, client_ip, client_port));
            }
            
            std::cout << "New client connected: " << client_ip << ":" << client_port 
                     << " (fd=" << client_fd << ")" << std::endl;
            
            // 发送欢迎消息
            std::string welcome = "READY\n";
            send(client_fd, welcome.c_str(), welcome.length(), MSG_NOSIGNAL);
            
            print_client_table();
        }
    }
    
    void handle_client_event(const struct pollfd& pfd) {
        int client_fd = pfd.fd;
        
        if (pfd.revents & (POLLHUP | POLLERR)) {
            remove_client(client_fd);
            return;
        }
        
        if (pfd.revents & POLLIN) {
            handle_client_data(client_fd);
        }
    }
    
    void handle_client_data(int client_fd) {
        std::lock_guard<std::mutex> lock(client_table_mutex);
        auto it = client_table.find(client_fd);
        if (it == client_table.end()) return;
        
        ClientInfo& client = it->second;
        
        if (!client.receiving_file) {
            // 接收文件头
            if (receive_file_header(client)) {
                start_file_reception(client);
            }
        } else {
            // 接收文件数据
            receive_file_data(client);
        }
    }
    
    bool receive_file_header(ClientInfo& client) {
        char buffer[sizeof(FileHeader)];
        ssize_t bytes_read = recv(client.fd, buffer, sizeof(FileHeader), 0);
        
        if (bytes_read == sizeof(FileHeader)) {
            memcpy(&client.current_file, buffer, sizeof(FileHeader));
            return true;
        } else if (bytes_read == 0) {
            std::cout << "Client disconnected during header reception" << std::endl;
            return false;
        } else if (bytes_read == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
            std::cerr << "Error receiving file header: " << strerror(errno) << std::endl;
            return false;
        }
        
        return false;
    }
    
    void start_file_reception(ClientInfo& client) {
        // 构建保存路径
        std::string filename = client.current_file.filename;
        client.save_path = save_directory + "/" + filename;
        
        // 如果文件已存在，添加序号
        int counter = 1;
        std::string base_path = client.save_path;
        while (std::filesystem::exists(client.save_path)) {
            size_t dot_pos = base_path.find_last_of('.');
            if (dot_pos != std::string::npos) {
                client.save_path = base_path.substr(0, dot_pos) + "_" + 
                                 std::to_string(counter) + base_path.substr(dot_pos);
            } else {
                client.save_path = base_path + "_" + std::to_string(counter);
            }
            counter++;
        }
        
        // 打开文件流
        client.file_stream.open(client.save_path, std::ios::binary);
        if (!client.file_stream.is_open()) {
            std::cerr << "Failed to create file: " << client.save_path << std::endl;
            std::string error_msg = "ERROR: Cannot create file\n";
            send(client.fd, error_msg.c_str(), error_msg.length(), MSG_NOSIGNAL);
            return;
        }
        
        client.receiving_file = true;
        client.bytes_received = 0;
        
        std::cout << "Starting to receive file: " << filename 
                  << " (" << client.current_file.filesize << " bytes)" << std::endl;
        
        // 发送确认消息
        std::string ack = "ACK\n";
        send(client.fd, ack.c_str(), ack.length(), MSG_NOSIGNAL);
    }
    
    void receive_file_data(ClientInfo& client) {
        char buffer[8192];
        uint64_t remaining = client.current_file.filesize - client.bytes_received;
        size_t to_read = std::min(remaining, (uint64_t)sizeof(buffer));
        
        ssize_t bytes_read = recv(client.fd, buffer, to_read, 0);
        
        if (bytes_read > 0) {
            client.file_stream.write(buffer, bytes_read);
            client.bytes_received += bytes_read;
            
            // 检查是否接收完成
            if (client.bytes_received >= client.current_file.filesize) {
                finish_file_reception(client);
            }
        } else if (bytes_read == 0) {
            std::cout << "Client disconnected during file transfer" << std::endl;
            cleanup_file_reception(client);
        } else if (bytes_read == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
            std::cerr << "Error receiving file data: " << strerror(errno) << std::endl;
            cleanup_file_reception(client);
        }
    }
    
    void finish_file_reception(ClientInfo& client) {
        client.file_stream.close();
        client.receiving_file = false;
        
        std::cout << "File received successfully: " << client.save_path 
                  << " (" << client.bytes_received << " bytes)" << std::endl;
        
        // 发送完成确认
        std::string success = "SUCCESS\n";
        send(client.fd, success.c_str(), success.length(), MSG_NOSIGNAL);
        
        // 重置状态，准备接收下一个文件
        memset(&client.current_file, 0, sizeof(FileHeader));
        client.bytes_received = 0;
        client.save_path.clear();
    }
    
    void cleanup_file_reception(ClientInfo& client) {
        if (client.file_stream.is_open()) {
            client.file_stream.close();
        }
        
        // 删除未完成的文件
        if (!client.save_path.empty() && std::filesystem::exists(client.save_path)) {
            std::filesystem::remove(client.save_path);
        }
        
        client.receiving_file = false;
        memset(&client.current_file, 0, sizeof(FileHeader));
        client.bytes_received = 0;
        client.save_path.clear();
    }
    
    void remove_client(int client_fd) {
        remove_client_from_poll(client_fd);
        
        {
            std::lock_guard<std::mutex> lock(client_table_mutex);
            auto it = client_table.find(client_fd);
            if (it != client_table.end()) {
                cleanup_file_reception(it->second);
                std::cout << "Client disconnected: " << it->second.ip << ":" 
                         << it->second.port << " (fd=" << client_fd << ")" << std::endl;
                client_table.erase(it);
            }
        }
        
        close(client_fd);
        print_client_table();
    }
    
public:
    void print_client_table() {
        std::lock_guard<std::mutex> lock(client_table_mutex);
        std::cout << "\n=== Current Connections ===" << std::endl;
        std::cout << "Total connections: " << client_table.size() << std::endl;
        
        for (const auto& pair : client_table) {
            const ClientInfo& info = pair.second;
            std::cout << "FD: " << info.fd 
                     << ", IP: " << info.ip 
                     << ", Port: " << info.port
                     << ", Connected: " << (time(nullptr) - info.connect_time) << "s ago";
            
            if (info.receiving_file) {
                double progress = (double)info.bytes_received / info.current_file.filesize * 100;
                std::cout << ", Receiving: " << info.current_file.filename 
                         << " (" << std::fixed << std::setprecision(1) << progress << "%)";
            }
            std::cout << std::endl;
        }
        std::cout << "========================\n" << std::endl;
    }
    
    size_t get_connection_count() {
        std::lock_guard<std::mutex> lock(client_table_mutex);
        return client_table.size();
    }
};

// 使用示例
int main(int argc, char* argv[]) {
    std::string save_dir = "./uploads";
    int port = 8080;
    
    if (argc >= 2) {
        port = std::stoi(argv[1]);
    }
    if (argc >= 3) {
        save_dir = argv[2];
    }
    
    TCPServer server(port, save_dir);
    
    // 启动服务器
    std::thread server_thread([&server]() {
        server.start();
    });
    
    // 主线程处理用户输入
    std::string input;
    std::cout << "TCP File Server started. Commands: status, quit" << std::endl;
    
    while (std::getline(std::cin, input)) {
        if (input == "quit" || input == "exit") {
            break;
        } else if (input == "status") {
            server.print_client_table();
        } else {
            std::cout << "Commands: status, quit" << std::endl;
        }
    }
    
    server.stop();
    if (server_thread.joinable()) {
        server_thread.join();
    }
    
    return 0;
}
