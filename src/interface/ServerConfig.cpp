#include "ServerConfig.h"
#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <wx/textfile.h>
#include <wx/tokenzr.h>  // 添加这行来支持 wxStringTokenizer

ServerConfig::ServerConfig() {
    // 获取配置文件路径
    wxStandardPaths& paths = wxStandardPaths::Get();
    wxString configDir = paths.GetUserConfigDir() + wxFileName::GetPathSeparator() + "FileUploadClient";
    // 确保配置目录存在
    if (!wxFileName::DirExists(configDir)) {
        wxFileName::Mkdir(configDir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
    }

    m_configPath = configDir + wxFileName::GetPathSeparator() + "servers.save";
    printf("config path: %s\n", std::string(m_configPath).c_str());
}

ServerConfig::~ServerConfig() {
    SaveConfig();
}

bool ServerConfig::LoadConfig() {
    if (!wxFileName::FileExists(m_configPath)) {
        return true; // 文件不存在是正常的
    }
    
    wxTextFile file(m_configPath);
    if (!file.Open()) {
        return false;
    }
    
    m_servers.clear();
    
    for (size_t i = 0; i < file.GetLineCount(); ++i) {
        wxString line = file.GetLine(i).Trim();
        if (line.IsEmpty() || line.StartsWith("#")) {
            continue; // 跳过空行和注释
        }

        ServerInfo server = ParseConfigLine(line);
        if (!server.name.IsEmpty()) {
            m_servers.push_back(server);
        }
    }
    
    file.Close();
    return true;
}

bool ServerConfig::SaveConfig() {
    wxTextFile file;
    
    // 如果文件存在，打开它；否则创建新文件
    if (wxFileName::FileExists(m_configPath)) {
        if (!file.Open(m_configPath)) {
            return false;
    }
        file.Clear();
    } else {
        if (!file.Create(m_configPath)) {
            return false;
}
    }

    // 添加文件头注释
    file.AddLine("# FileUploadClient Server Configuration");
    file.AddLine("# Format: name|ip|port");
    file.AddLine("");
    
    // 保存所有服务器配置
    for (const ServerInfo& server : m_servers) {
        file.AddLine(FormatConfigLine(server));
    }
    return file.Write();
}

void ServerConfig::AddServer(const ServerInfo& server) {
    m_servers.push_back(server);
}

void ServerConfig::RemoveServer(size_t index) {
    if (index < m_servers.size()) {
        m_servers.erase(m_servers.begin() + index);
    }
}

void ServerConfig::UpdateServer(size_t index, const ServerInfo& server) {
    if (index < m_servers.size()) {
        m_servers[index] = server;
    }
}

ServerInfo& ServerConfig::GetServer(size_t index) {
    static ServerInfo empty;
    if (index < m_servers.size()) {
        return m_servers[index];
    }
    return empty;
}

ServerInfo ServerConfig::ParseConfigLine(const wxString& line) {
    ServerInfo server;
    
    // 使用 | 作为分隔符解析配置行
    wxStringTokenizer tokenizer(line, "|");
    
    if (tokenizer.HasMoreTokens()) {
        server.name = tokenizer.GetNextToken().Trim();
    }
    
    if (tokenizer.HasMoreTokens()) {
        server.ip = tokenizer.GetNextToken().Trim();
    }
    
    if (tokenizer.HasMoreTokens()) {
        wxString portStr = tokenizer.GetNextToken().Trim();
        long port;
        if (portStr.ToLong(&port)) {
            server.port = static_cast<int>(port);
        }
    }
    
    return server;
}

wxString ServerConfig::FormatConfigLine(const ServerInfo& server) {
    return wxString::Format("%s|%s|%d", server.name, server.ip, server.port);
}