
#include <unistd.h>
#include <iostream>
#include <signal.h>
#include <vector>
#include <atomic>
#include <mutex>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string>
#include <unordered_map>
#include <wx/wx.h>
#include <wx/stdpaths.h>
#include <wx/filename.h>

#include "LocalConf.h"
#include "../rdma/HwRdma.h"
#include "../rdma/StreamControl.h"
#include "ClientInfo.h"
using namespace std;

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

void recvData(HwRdma *hwrdma, int peer_fd,  LocalConf* local_conf, ClientList* client_list)
{
    StreamControl stream_control(hwrdma, peer_fd, local_conf->getDefaultRate(),  client_list);
    std::shared_ptr<int> x(NULL, [&](int *)
                           {    
                                close(peer_fd); 
                                client_list->removeClient(peer_fd);
                            });
    if(stream_control.swapBufferConfig() == -1)
        return;
    if (stream_control.bindMemoryRegion() == -1)
        return;
    if (stream_control.createBufferPool() == -1)
        return;
    if (stream_control.createLucpContext() == -1)
        return;
    if (stream_control.connectPeer() == -1)
        return;
    if (stream_control.postRecvFile() == -1)
        return;
}
void sendData(HwRdma *hwrdma, int peer_fd, LocalConf *local_conf)
{
    StreamControl stream_control(hwrdma, peer_fd, local_conf->getDefaultRate(), nullptr);
    std::shared_ptr<int> x(NULL, [&](int *)
                           {    
                                close(peer_fd); 
                            });
    if(stream_control.swapBufferConfig() == -1)
        return;
    if (stream_control.bindMemoryRegion() == -1)
        return;
    if (stream_control.createBufferPool() == -1)
        return;
    if (stream_control.createLucpContext() == -1)
        return;
    if (stream_control.connectPeer() == -1)
        return;
    if (stream_control.postSendFile() == -1)
        return;
}

string getConfigPath()
{
    wxStandardPaths& paths = wxStandardPaths::Get();
    wxString configDir = paths.GetUserConfigDir() + wxFileName::GetPathSeparator() + "FileUploadClient";
    if (!wxFileName::DirExists(configDir)) {
        wxFileName::Mkdir(configDir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
    }

    m_configPath = configDir + wxFileName::GetPathSeparator() + "local.conf";
    return m_configPath.ToStdString();
}
int main(int narg, char *argv[])
{
    LocalConf local_conf(getConfigPath());
    local_conf.loadConf();
    signal(SIGPIPE, SIG_IGN);
    // Create an hdRDMA object
    HwRdma hwrdma(local_conf.getRdmaGidIndex(), 1, local_conf.getBlockSize() * local_conf.getBlockNum() * local_conf.getMaxThreadNum() * 1024);
    hwrdma.init();

    {
        struct sockaddr_in addr;
        bzero(&addr, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(local_conf.getLocalPort());

        int reuse = 1;
        auto server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
        setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));
        auto ret = bind(server_sockfd, (struct sockaddr *)&addr, sizeof(addr));
        if (ret != 0)
        {
            cout << "ERROR: binding server socket!" << endl;
            return -1;
        }
        listen(server_sockfd, local_conf.getMaxThreadNum());

        // Loop forever accepting connections
        cout << "Listening for connections on port ... " << local_conf.getLocalPort() << endl;
        ClientList client_list;
        while (1)
        {
            int peer_sockfd = -1;
            struct sockaddr_in peer_addr;
            socklen_t peer_addr_len = sizeof(struct sockaddr_in);
            peer_sockfd = accept(server_sockfd, (struct sockaddr *)&peer_addr, &peer_addr_len);
            if (peer_sockfd < 0)
            {
                cout << "Failed connection!  errno=" << errno << endl;
                continue;
            }
            {
                if(client_list.getClientNum() >= local_conf.getMaxThreadNum())
                {
                    close(peer_sockfd);
                    continue;
                }
                client_list.addClient(peer_sockfd, peer_addr.sin_addr.s_addr);
                cout << "Connection from " << inet_ntoa(peer_addr.sin_addr) << endl;
            }

            // Create a new thread to handle this connection
            std::thread thr(recvData, &hwrdma, peer_sockfd, &local_conf, &client_list);
            thr.detach();
        }
    }

    // Connect to remote peer if we are in client mode.
    // This will attempt to connect to an hdRDMA object listening on the
    // specified host/port. If a connection is made then the RDMA transfer
    // information will be exchanged and stored in the hdRDMA object and
    // be made available for transfers. If the connection cannot be made
    // then it will exit the program with an error message.
    {
        // cout << "IP address: " << HDRDMA_REMOTE_ADDR << endl;
        thread *th_arr[16];
        int client_count = 0;
        // Create socket and connect it to remote host
        while (client_count < HDRDMA_SUPPORT_THREADS)
        {

            th_arr[client_count] = new thread([&]()
            {
                struct sockaddr_in addr;
                bzero(&addr, sizeof(addr));
                addr.sin_family = AF_INET;
                addr.sin_addr.s_addr = inet_addr(HDRDMA_REMOTE_ADDR.c_str());
                addr.sin_port = htons(HDRDMA_REMOTE_PORT);
                int peer_fd = socket(AF_INET, SOCK_STREAM, 0);
                auto ret = connect(peer_fd, (struct sockaddr *)&addr, sizeof(addr));
                if (ret != 0)
                {
                    cout << "ERROR: connecting to server: " << HDRDMA_REMOTE_ADDR << endl;
                    exit(-1);
                }
                else{
#ifdef DEBUG
                    cout << "Connected to " << HDRDMA_REMOTE_ADDR << ":" << HDRDMA_REMOTE_PORT << endl;
#endif
                    sendData(&hwrdma, peer_fd); 
                }

            });
            usleep(300000);
            client_count ++;
        }
        for(int i = 0; i < HDRDMA_SUPPORT_THREADS && i < 16; i++)
        {
            th_arr[i]->join();
            delete th_arr[i];
        }
    }
    return 0;
}

