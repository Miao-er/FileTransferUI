#ifndef SERVERCONFIG_H
#define SERVERCONFIG_H

#include <wx/wx.h>
#include <wx/textfile.h>
#include <vector>
struct ServerInfo {
    wxString name;
    wxString ip;
    int port;
    int fd;
    
    ServerInfo() : port(0) {}
    ServerInfo(const wxString& n, const wxString& i, int p) 
        : name(n), ip(i), port(p), fd(-1) {}
};

/**
 * @brief 服务器配置管理类
 */
class ServerConfig {
public:
    ServerConfig();
    ~ServerConfig();
    
    /**
     * @brief 加载服务器配置
     * @return 是否成功加载
     */
    bool LoadConfig();
    
    /**
     * @brief 保存服务器配置
     * @return 是否成功保存
     */
    bool SaveConfig();
    
    /**
     * @brief 添加服务器配置
     * @param server 服务器信息
     */
    void AddServer(const ServerInfo& server);
    
    /**
     * @brief 删除服务器配置
     * @param index 索引
     */
    void RemoveServer(size_t index);
    
    /**
     * @brief 更新服务器配置
     * @param index 索引
     * @param server 新的服务器信息
     */
    void UpdateServer(size_t index, const ServerInfo& server);
    
    /**
     * @brief 获取所有服务器配置
     * @return 服务器列表
     */
    const std::vector<ServerInfo>& GetServers() const { return m_servers; }
    
    /**
     * @brief 获取指定服务器配置
     * @param index 索引
     * @return 服务器信息
     */
    ServerInfo& GetServer(size_t index);

private:
    std::vector<ServerInfo> m_servers;
    wxString m_configPath;
    
    /**
     * @brief 解析配置行
     * @param line 配置行
     * @return 服务器信息
     */
    ServerInfo ParseConfigLine(const wxString& line);
    
    /**
     * @brief 格式化配置行
     * @param server 服务器信息
     * @return 配置行
     */
    wxString FormatConfigLine(const ServerInfo& server);
};

#endif
