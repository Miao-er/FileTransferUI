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

#endif