#include <iostream>
#include <thread>
#include <string>
#include <atomic>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <chrono>
#include <fstream>
#include <filesystem>

// 文件传输协议结构
struct FileHeader {
    char filename[256];
    uint64_t filesize;
    uint32_t checksum;
};

class TCPClient {
private:
    int client_fd;
    std::atomic<bool> connected;
    std::atomic<bool> running;
    std::thread receive_thread;
    std::atomic<bool> ack_received;
    std::string server_ip;
    int server_port;
    
public:
    TCPClient() : client_fd(-1), connected(false), running(false), ack_received(false) {}
    
    ~TCPClient() {
        disconnect();
    }
    
    bool connect_to_server(const std::string& ip, int port) {
        if (connected) {
            disconnect();
        }
        
        server_ip = ip;
        server_port = port;
        
        client_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (client_fd == -1) {
            std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
            return false;
        }
        
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        
        if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) <= 0) {
            std::cerr << "Invalid IP address: " << ip << std::endl;
            close(client_fd);
            client_fd = -1;
            return false;
        }
        printf("connecting\n");
        if (connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "Failed to connect to " << ip << ":" << port 
                     << " - " << strerror(errno) << std::endl;
            close(client_fd);
            client_fd = -1;
            return false;
        }
        
        connected = true;
        running = true;
            try {
            receive_thread = std::thread(&TCPClient::receive_loop, this);
            } catch (const std::exception& e) {
            std::cerr << "Failed to create receive thread: " << e.what() << std::endl;
            connected = false;
            running = false;
            close(client_fd);
            client_fd = -1;
            return false;
            }
        std::cout << "Successfully connected to " << ip << ":" << port << std::endl;
        return true;
    }
        
    void disconnect() {
        if (!connected && !running) return;
        
        std::cout << "Disconnecting..." << std::endl;
        
        running = false;
        connected = false;
        
        if (client_fd != -1) {
            shutdown(client_fd, SHUT_RDWR);
            close(client_fd);
            client_fd = -1;
        }
        
        if (receive_thread.joinable()) {
            try {
                receive_thread.join();
            } catch (const std::exception& e) {
                std::cerr << "Error joining receive thread: " << e.what() << std::endl;
    }
        }
    
        std::cout << "Disconnected from server" << std::endl;
        }
        
    bool send_file(const std::string& file_path) {
        if (!connected || client_fd == -1) {
            std::cerr << "Not connected to server" << std::endl;
            return false;
    }
    
        // 检查文件是否存在
        if (!std::filesystem::exists(file_path)) {
            std::cerr << "File does not exist: " << file_path << std::endl;
            return false;
        }
        
        // 获取文件信息
        std::filesystem::path path(file_path);
        std::string filename = path.filename().string();
        uint64_t filesize = std::filesystem::file_size(file_path);
        
        if (filename.length() >= 256) {
            std::cerr << "Filename too long (max 255 characters)" << std::endl;
            return false;
        }
        
        // 准备文件头
        FileHeader header;
        memset(&header, 0, sizeof(header));
        strncpy(header.filename, filename.c_str(), sizeof(header.filename) - 1);
        header.filesize = filesize;
        header.checksum = 0; // 简化版本，不计算校验和
        
        // 发送文件头
        ssize_t bytes_sent = send(client_fd, &header, sizeof(header), MSG_NOSIGNAL);
        if (bytes_sent != sizeof(header)) {
            std::cerr << "Failed to send file header" << std::endl;
            return false;
    }
    
        std::cout << "Sending file: " << filename << " (" << filesize << " bytes)" << std::endl;
        
        // 等待服务器确认
        if (!wait_for_ack()) {
            std::cerr << "Server did not acknowledge file header" << std::endl;
            return false;
        }
        
        // 发送文件数据
        return send_file_data(file_path, filesize);
    }
    
    bool is_connected() const {
        return connected;
    }
    
private:
    bool wait_for_ack() {
        ack_received = false;
        
        // 设置超时时间
        auto start_time = std::chrono::steady_clock::now();
        auto timeout = std::chrono::seconds(1); // 10秒超时
        
        // 等待接收线程设置ack_received标志
        while (!ack_received) {
            // 检查是否超时
            auto current_time = std::chrono::steady_clock::now();
            if (current_time - start_time > timeout) {
                std::cerr << "Timeout waiting for server acknowledgement" << std::endl;
                return false;
            }
            
            // 检查连接状态
            if (!connected) {
                std::cerr << "Connection lost while waiting for acknowledgement" << std::endl;
                return false;
            }
            
            // 短暂休眠避免CPU占用过高
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return true;
    }
    
    bool send_file_data(const std::string& file_path, uint64_t filesize) {
        std::ifstream file(file_path, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << file_path << std::endl;
            return false;
        }
        
        char buffer[8192];
        uint64_t bytes_sent = 0;
        uint64_t last_progress = 0;
        printf("sending\n");
        while (bytes_sent < filesize && file.good()) {
            uint64_t remaining = filesize - bytes_sent;
            size_t to_read = std::min(remaining, (uint64_t)sizeof(buffer));
            
            file.read(buffer, to_read);
            size_t bytes_read = file.gcount();
            
            if (bytes_read == 0) break;
            
            ssize_t sent = send(client_fd, buffer, bytes_read, MSG_NOSIGNAL);
            if (sent == -1) {
                std::cerr << "Failed to send file data: " << strerror(errno) << std::endl;
                file.close();
                return false;
    }
    
            bytes_sent += sent;
            
            // 显示进度
            uint64_t progress = (bytes_sent * 100) / filesize;
            if (progress != last_progress && progress % 10 == 0) {
                std::cout << "Progress: " << progress << "%" << std::endl;
                last_progress = progress;
            }
        }
        
        file.close();
        
        if (bytes_sent == filesize) {
            std::cout << "File sent successfully: " << bytes_sent << " bytes" << std::endl;
            return true;
        } else {
            std::cerr << "File transfer incomplete: " << bytes_sent << "/" << filesize << " bytes" << std::endl;
            return false;
        }
    }
    
    void receive_loop() {
        char buffer[4096];
        
        while (running && connected) {
            ssize_t bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
            
            if (bytes_received > 0) {
                buffer[bytes_received] = '\0';
                std::string response(buffer);
                
                if (response.find("READY") != std::string::npos) {
                    std::cout << "[Server]: Ready to receive files" << std::endl;
                } else if (response.find("SUCCESS") != std::string::npos) {
                    std::cout << "[Server]: File received successfully" << std::endl;
                } else if(response.find("ACK") != std::string::npos)
                {
                    ack_received = true;
                }else if (response.find("ERROR") != std::string::npos) {
                    std::cout << "[Server]: " << response << std::endl;
                } else {
                    std::cout << "[Server]: " << response << std::flush;
                }
            } else if (bytes_received == 0) {
                std::cout << "\n[INFO] Server closed the connection" << std::endl;
                connected = false;
                break;
            } else {
                if (errno == EINTR) continue;
                if (errno == EBADF || errno == ENOTCONN) {
                    break;
                }
                
                std::cerr << "\n[ERROR] Receive failed: " << strerror(errno) << std::endl;
                connected = false;
                break;
            }
        }
        
        running = false;
    }
};

class InteractiveClient {
private:
    TCPClient client;
    
public:
    void run() {
        print_welcome();
        
        try {
            while (true) {
                std::string command = get_user_input("Enter command (help for options): ");
                
                if (command.empty()) continue;
                
                if (command == "help" || command == "h") {
                    print_help();
                } else if (command == "connect" || command == "c") {
                    handle_connect();
                } else if (command == "disconnect" || command == "d") {
                    handle_disconnect();
                } else if (command == "upload" || command == "u") {
                    handle_upload();
                } else if (command == "status") {
                    handle_status();
                } else if (command == "quit" || command == "exit" || command == "q") {
                    handle_quit();
                    break;
                } else if (command.length() > 7 && command.substr(0, 7) == "upload ") {
                    // 直接上传文件: upload <filepath>
                    std::string filepath = command.substr(7);
                    upload_file(filepath);
                } else {
                    std::cout << "Unknown command: " << command << std::endl;
                    std::cout << "Type 'help' for available commands." << std::endl;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error in interactive loop: " << e.what() << std::endl;
        }
    }
    
private:
    void print_welcome() {
        std::cout << "========================================" << std::endl;
        std::cout << "       TCP File Transfer Client        " << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Type 'help' to see available commands." << std::endl;
        std::cout << std::endl;
    }
    
    void print_help() {
        std::cout << "\nAvailable Commands:" << std::endl;
        std::cout << "  connect (c)       - Connect to server" << std::endl;
        std::cout << "  disconnect (d)    - Disconnect from server" << std::endl;
        std::cout << "  upload (u)        - Upload file to server" << std::endl;
        std::cout << "  upload <filepath> - Upload file directly" << std::endl;
        std::cout << "  status            - Show connection status" << std::endl;
        std::cout << "  help (h)          - Show this help" << std::endl;
        std::cout << "  quit (q/exit)     - Exit the program" << std::endl;
        std::cout << std::endl;
}

    std::string get_user_input(const std::string& prompt) {
        std::cout << prompt;
        std::string input;
        if (!std::getline(std::cin, input)) {
            return "quit";
        }
        return input;
    }
    
    void handle_connect() {
        if (client.is_connected()) {
            std::cout << "Already connected to server. Disconnect first." << std::endl;
            return;
        }
        
        std::string ip = get_user_input("Enter server IP (default: 127.0.0.1): ");
        if (ip.empty()) {
            ip = "127.0.0.1";
        }
        
        std::string port_str = get_user_input("Enter server port (default: 8080): ");
        int port = 8080;
        if (!port_str.empty()) {
            try {
                port = std::stoi(port_str);
                if (port <= 0 || port > 65535) {
                    std::cout << "Invalid port number. Using default 8080." << std::endl;
                    port = 8080;
                }
            } catch (const std::exception& e) {
                std::cout << "Invalid port format. Using default 8080." << std::endl;
                port = 8080;
            }
        }
        
        std::cout << "Connecting to " << ip << ":" << port << "..." << std::endl;
        
        if (client.connect_to_server(ip, port)) {
            std::cout << "Connection established successfully!" << std::endl;
        } else {
            std::cout << "Failed to connect to server." << std::endl;
        }
    }
    
    void handle_disconnect() {
        if (!client.is_connected()) {
            std::cout << "Not connected to any server." << std::endl;
            return;
        }
        
        client.disconnect();
    }
    
    void handle_upload() {
        if (!client.is_connected()) {
            std::cout << "Not connected to server. Connect first." << std::endl;
            return;
        }
        
        std::string filepath = get_user_input("Enter file path to upload: ");
        if (filepath.empty()) {
            std::cout << "Empty file path. Nothing uploaded." << std::endl;
            return;
        }
        
        upload_file(filepath);
    }
    
    void upload_file(const std::string& filepath) {
        if (!client.is_connected()) {
            std::cout << "Not connected to server." << std::endl;
            return;
        }
        
        if (client.send_file(filepath)) {
            std::cout << "File uploaded successfully!" << std::endl;
        } else {
            std::cout << "Failed to upload file." << std::endl;
        }
    }
    
    void handle_status() {
        if (client.is_connected()) {
            std::cout << "Status: Connected to server" << std::endl;
        } else {
            std::cout << "Status: Not connected" << std::endl;
        }
    }
    
    void handle_quit() {
        std::cout << "Exiting..." << std::endl;
        if (client.is_connected()) {
            client.disconnect();
        }
    }
};

// 简单的命令行参数版本
void run_simple_client(const std::string& ip, int port, const std::string& filepath) {
    TCPClient client;
    
    std::cout << "Connecting to " << ip << ":" << port << "..." << std::endl;
    
    if (!client.connect_to_server(ip, port)) {
        std::cout << "Failed to connect. Exiting." << std::endl;
        return;
    }
    
    std::cout << "Uploading file: " << filepath << std::endl;
    
    if (client.send_file(filepath)) {
        std::cout << "File uploaded successfully!" << std::endl;
    } else {
        std::cout << "Failed to upload file." << std::endl;
    }
    
    client.disconnect();
}

int main(int argc, char* argv[]) {
    try {
        if (argc == 4) {
            // 简单模式: ./client <ip> <port> <filepath>
            std::string ip = argv[1];
            int port = std::stoi(argv[2]);
            std::string filepath = argv[3];
            run_simple_client(ip, port, filepath);
        } else if (argc == 1) {
            // 交互模式
            InteractiveClient interactive_client;
            interactive_client.run();
        } else {
            std::cout << "Usage:" << std::endl;
            std::cout << "  Interactive mode: " << argv[0] << std::endl;
            std::cout << "  Simple mode:      " << argv[0] << " <server_ip> <server_port> <file_path>" << std::endl;
            std::cout << std::endl;
            std::cout << "Examples:" << std::endl;
            std::cout << "  " << argv[0] << std::endl;
            std::cout << "  " << argv[0] << " 127.0.0.1 8080 /path/to/file.txt" << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}