#ifndef CLIENT_INFO_H
#define CLIENT_INFO_H

#include <string>
#include <stdint.h>
class ClientInfo;

enum ClientStatus 
{
    CLIENT_STATUS_INVALID = -1,
    CLIENT_STATUS_IDLE,
    CLIENT_STATUS_RECEIVING
};
struct FileInfo
{

    std::string fileName;
    uint64_t fileSize;
    uint64_t offset;
    ClientInfo* client;
    FileInfo(const std::string& fn, uint64_t fs, ClientInfo* c)
        : fileName(fn), fileSize(fs), offset(0), client(c) {}
};

struct ClientInfo 
{
    int fd;
    uint32_t ip;
    // 状态
    ClientStatus clientStat;
    FileInfo fileInfo;
    
    ClientInfo(int f, uint32_t i) 
    : fd(f), ip(i), clientStat(CLIENT_STATUS_IDLE), fileInfo(nullptr) {}
};

class ClientList{
private:
    unordered_map<int, ClientInfo> clientInfos;
    mutex clientInfosMutex;
public:
    ClientList(){
        clientInfos.clear();
    }
    void addClient(int fd, uint32_t ip){
        lock_guard<mutex> lock(clientInfosMutex);
        clientInfos.emplace(fd, ClientInfo(fd, ip));
    }
    void removeClient(int fd){
        lock_guard<mutex> lock(clientInfosMutex);
        if(clientInfos.find(fd) != clientInfos.end())
            clientInfos.erase(fd);
    }
    void getClientNum()
    {
        lock_guard<mutex> lock(clientInfosMutex);
        return clientInfos.size();
    }
    Client& getClientInfo(int fd){
        lock_guard<mutex> lock(clientInfosMutex);
        return clientInfos[fd];
    }
};

#endif