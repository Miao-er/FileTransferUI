#include "MainFrame.h"
#include "ServerConfigDialog.h"
#include "FileExplorerFrame.h"
#include "DirectoryConfigDialog.h" // 添加包含
#include <wx/msgdlg.h>
#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <wx/textfile.h>
#include <wx/utils.h> // for wxLaunchDefaultBrowser
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <iostream>
#include "../utils/LocalConf.h"
#include "../net/StreamControl.h"

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_BUTTON(ID_ADD_SERVER, MainFrame::OnAdd)
    EVT_BUTTON(ID_EDIT_SERVER, MainFrame::OnEdit)
    EVT_BUTTON(ID_DELETE_SERVER, MainFrame::OnDelete)
    EVT_BUTTON(ID_CONNECT_SERVER, MainFrame::OnConnect)
    EVT_BUTTON(ID_STORAGE_LOCATION, MainFrame::OnStorageLocation) // 添加事件绑定
    EVT_BUTTON(ID_SHOW_IN_BROWSER, MainFrame::OnShowInBrowser) // 新增事件绑定
    EVT_BUTTON(ID_EXIT, MainFrame::OnExit) // 新增退出事件绑定
    EVT_LIST_ITEM_SELECTED(ID_SERVER_LIST, MainFrame::OnServerSelected)
    EVT_LIST_ITEM_DESELECTED(ID_SERVER_LIST, MainFrame::OnServerDeselected) // 新增
    EVT_LIST_ITEM_ACTIVATED(ID_SERVER_LIST, MainFrame::OnServerDoubleClick)
    EVT_CLOSE(MainFrame::OnClose)
wxEND_EVENT_TABLE()

MainFrame::MainFrame() 
    : wxFrame(nullptr, wxID_ANY, "File Transfer Client", 
              wxDefaultPosition, wxSize(800, 600)),
      m_config(nullptr), m_explorerFrame(nullptr) {
    
    // 初始化配置
    m_config = new ServerConfig();
    m_config->LoadConfig();
    
    // 初始化UI
    InitializeUI();
    
    // 刷新服务器列表
    RefreshServerList();
    
    // 创建状态栏
    CreateStatusBar();
    SetStatusText("Ready");
}

MainFrame::~MainFrame() {
    if (m_config) {
        delete m_config;
    }
}

void MainFrame::InitializeUI() {
    wxPanel* panel = new wxPanel(this);
    
    // 创建服务器列表
    m_serverList = new wxListCtrl(panel, ID_SERVER_LIST, 
                                  wxDefaultPosition, wxDefaultSize,
                                  wxLC_REPORT | wxLC_SINGLE_SEL);
    
    // 设置列标题
    m_serverList->AppendColumn("name", wxLIST_FORMAT_LEFT, 200);
    m_serverList->AppendColumn("IP", wxLIST_FORMAT_LEFT, 150);
    m_serverList->AppendColumn("port", wxLIST_FORMAT_LEFT, 100);
    
    // 创建按钮
    m_addBtn = new wxButton(panel, ID_ADD_SERVER, "Add(+)");
    m_editBtn = new wxButton(panel, ID_EDIT_SERVER, "Edit");
    m_deleteBtn = new wxButton(panel, ID_DELETE_SERVER, "Delete(-)");
    m_connectBtn = new wxButton(panel, ID_CONNECT_SERVER, "Connect");
    m_storageBtn = new wxButton(panel, ID_STORAGE_LOCATION, _T("Edit Saved Folder")); // 添加新按钮
    m_showInBrowserBtn = new wxButton(panel, ID_SHOW_IN_BROWSER, _T("Show in Brower")); // 新增按钮
    m_exitBtn = new wxButton(panel, ID_EXIT, "Exit"); // 新增退出按钮
    
    // 初始状态下禁用某些按钮
    m_editBtn->Enable(false);
    m_deleteBtn->Enable(false);
    m_connectBtn->Enable(false);
    
    // 布局
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    
    btnSizer->Add(m_addBtn, 0, wxRIGHT, 5);
    btnSizer->Add(m_editBtn, 0, wxRIGHT, 5);
    btnSizer->Add(m_deleteBtn, 0, wxRIGHT, 5);
    btnSizer->AddStretchSpacer();
    btnSizer->Add(m_storageBtn, 0, wxRIGHT, 10); // 添加存储位置按钮
    btnSizer->Add(m_showInBrowserBtn, 0, wxRIGHT, 10); // 新增到布局
    btnSizer->Add(m_connectBtn, 0, wxRIGHT, 10); // 添加右边距
    btnSizer->Add(m_exitBtn, 0); // 新增退出按钮到布局
    
    mainSizer->Add(new wxStaticText(panel, wxID_ANY, _T("列表:")), 
                   0, wxALL, 10);
    mainSizer->Add(m_serverList, 1, wxEXPAND | wxLEFT | wxRIGHT, 10);
    mainSizer->Add(btnSizer, 0, wxEXPAND | wxALL, 10);
    
    panel->SetSizer(mainSizer);
}

void MainFrame::RefreshServerList() {
    m_serverList->DeleteAllItems();
    
    const auto& servers = m_config->GetServers();
    for (size_t i = 0; i < servers.size(); ++i) {
        long index = m_serverList->InsertItem(i, servers[i].name);
        m_serverList->SetItem(index, 1, servers[i].ip);
        m_serverList->SetItem(index, 2, wxString::Format("%d", servers[i].port));
    }
    
    UpdateButtonStates();
}

void MainFrame::UpdateButtonStates() {
    bool hasSelection = GetSelectedServerIndex() != -1;
    m_editBtn->Enable(hasSelection);
    m_deleteBtn->Enable(hasSelection);
    m_connectBtn->Enable(hasSelection);
}

int MainFrame::GetSelectedServerIndex() {
    long selected = m_serverList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    return selected == -1 ? -1 : static_cast<int>(selected);
}

void MainFrame::OnAdd(wxCommandEvent& event) {
    ServerConfigDialog dialog(this);
    if (dialog.ShowModal() == wxID_OK) {
        ServerInfo server = dialog.GetServerInfo();
        m_config->AddServer(server);
        m_config->SaveConfig();
        RefreshServerList();
        SetStatusText("Server config added successfully");
    }
}

void MainFrame::OnEdit(wxCommandEvent& event) {
    int index = GetSelectedServerIndex();
    if (index == -1) return;
    
    const ServerInfo& server = m_config->GetServer(index);
    ServerConfigDialog dialog(this, server, "Edit Server Config");

    if (dialog.ShowModal() == wxID_OK) {
        ServerInfo newServer = dialog.GetServerInfo();
        m_config->UpdateServer(index, newServer);
        m_config->SaveConfig();
        RefreshServerList();
        SetStatusText("Server config updated successfully");
    }
}

void MainFrame::OnDelete(wxCommandEvent& event) {
    int index = GetSelectedServerIndex();
    if (index == -1) return;
    
    const ServerInfo& server = m_config->GetServer(index);
    wxString message = wxString::Format("Are you sure you want to delete the server '%s'?", server.name);

    if (wxMessageBox(message, "Confirm Delete", wxYES_NO | wxICON_QUESTION) == wxYES) {
        m_config->RemoveServer(index);
        m_config->SaveConfig();
        RefreshServerList();
        SetStatusText("Server config deleted successfully");
    }
}

void MainFrame::OnConnect(wxCommandEvent& event) {
    int index = GetSelectedServerIndex();
    if (index == -1) return;
    
    ServerInfo& server = m_config->GetServer(index);
    // 如果已有文件浏览器窗口，先关闭它
    if (m_explorerFrame) {
        printf("OnConnect: %s\n", "close");
        m_explorerFrame->Destroy();
        m_explorerFrame = nullptr;
    }
    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(server.ip.ToStdString().c_str());
    addr.sin_port = htons(server.port);
    int peer_fd = socket(AF_INET, SOCK_STREAM, 0);
    auto ret = connect(peer_fd, (struct sockaddr *)&addr, sizeof(addr));
    if (ret != 0)
    {
        std::cout << "ERROR: connecting to server: " << server.ip.ToStdString().c_str() << std::endl;
        
        // 关闭socket文件描述符
        close(peer_fd);
        // 弹出错误消息框
        wxMessageBox(_T("连接失败，请检查服务器配置"), _T("连接错误"), wxOK | wxICON_ERROR, this);
        // 更新状态栏
        SetStatusText("Connection failed");
        return;
    }
    LocalConf* local_conf = new LocalConf(getConfigPath());
    local_conf->loadConf();
    HwRdma* hwrdma = new HwRdma(local_conf->getRdmaGidIndex(), (uint64_t)-1);
    if(hwrdma->init())
    {
        wxMessageBox(_T("RDMA初始化失败，请检查配置"), _T("初始化错误"), wxOK | wxICON_ERROR, this);
        close(peer_fd);
        SetStatusText("RDMA initialization failed");
        delete local_conf;
        delete hwrdma;
        return;
    }
    StreamControl* stream_control = new StreamControl(hwrdma, peer_fd, local_conf);
    int error_code = 0;
    do{
        if (stream_control->createLucpContext() == -1){
            error_code = -1;
            break;
        }
        ret = stream_control->connectPeer();
        if(ret < 0)
        {
            error_code = ret;
            break;
        }
        if (stream_control->bindMemoryRegion() == -1){
            error_code = -1;
            break;
        }
        if (stream_control->createBufferPool() == -1){
            error_code = -1;
            break;
        }    
    }while(0);
    if(error_code == -2) 
    {
        wxMessageBox(_T("连接失败，服务器未在线"), _T("连接错误"), wxOK | wxICON_ERROR, this);
        close(peer_fd);
        SetStatusText("Connection failed");
        delete local_conf;
        delete stream_control;
        delete hwrdma;
        return;
    }
    else if(error_code == -1)
    {
        wxMessageBox(_T("连接失败，请检查参数配置"), _T("创建错误"), wxOK | wxICON_ERROR, this);
        close(peer_fd);
        SetStatusText("Connection failed");
        delete local_conf;
        delete stream_control;
        delete hwrdma;
        return;
    }

    std::cout << "Connected to " << server.ip.ToStdString().c_str() << ":" << server.port << std::endl;
    server.fd = peer_fd;
    // 创建新的文件浏览器窗口
    m_explorerFrame = new FileExplorerFrame(this, server, hwrdma, stream_control, local_conf);
    m_explorerFrame->Show(true);
    // 隐藏主窗口
    Hide();

    SetStatusText(wxString::Format("Connected to %s", server.name));
}

void MainFrame::OnServerSelected(wxListEvent& event) {
    UpdateButtonStates();
}

void MainFrame::OnServerDeselected(wxListEvent& event) {
    UpdateButtonStates();
}

void MainFrame::OnServerDoubleClick(wxListEvent& event) {
    // 创建一个命令事件对象并调用连接处理函数
    wxCommandEvent evt(wxEVT_BUTTON, ID_CONNECT_SERVER);
    OnConnect(evt);
}

void MainFrame::OnFileExplorerFrameClose()
{
    m_explorerFrame = nullptr;
}

void MainFrame::OnClose(wxCloseEvent& event) {
    printf("main OnClose: %s\n", "close");
    // 确保清理所有子窗口
    if (m_explorerFrame) {
        printf("main explore OnClose: %s\n", "close");
        m_explorerFrame->Destroy();
        m_explorerFrame = nullptr;
    }
    
    // 保存配置
    if (m_config) {
        m_config->SaveConfig();
}

    event.Skip();
}

void MainFrame::OnStorageLocation(wxCommandEvent& event) {
    DirectoryConfigDialog dialog(this);
    if (dialog.ShowModal() == wxID_OK) {
        wxString selectedPath = dialog.GetSelectedPath();
        SetStatusText(wxString::Format(_T("本地存储位置已更新: %s"), selectedPath));
        
        // 可选：显示成功消息
        wxMessageBox(wxString::Format(_T("本地存储位置已成功设置为:\n%s"), selectedPath),
                     _T("设置成功"), wxOK | wxICON_INFORMATION, this);
    }
}

void MainFrame::OnShowInBrowser(wxCommandEvent& event) {
    // 读取配置文件中的存储路径
    // wxStandardPaths& paths = wxStandardPaths::Get();
    // wxString configDir = paths.GetUserConfigDir() + wxFileName::GetPathSeparator() + "FileUploadClient";
    // wxString configPath = configDir + wxFileName::GetPathSeparator() + "server.conf";
    LocalConf local_conf(getConfigPath());
    local_conf.loadConf();
    wxString storagePath = local_conf.getSavedFolderPath();

    if (storagePath.IsEmpty() || !wxFileName::DirExists(storagePath)) {
        wxMessageBox(_T("未找到有效的本地存储路径，请先设置。"), _T("提示"), wxOK | wxICON_WARNING, this);
        return;
    }

    // 打开系统文件管理器
#ifdef __WXGTK__
    wxString cmd = "xdg-open \"" + storagePath + "\"";
    wxExecute(cmd, wxEXEC_ASYNC);
#elif defined(__WXMSW__)
    wxString cmd = "explorer \"" + storagePath + "\"";
    wxExecute(cmd, wxEXEC_ASYNC);
#elif defined(__WXOSX__)
    wxString cmd = "open \"" + storagePath + "\"";
    wxExecute(cmd, wxEXEC_ASYNC);
#else
    wxMessageBox(_T("不支持的操作系统"), _T("错误"), wxOK | wxICON_ERROR, this);
#endif
}

void MainFrame::OnExit(wxCommandEvent& event) {
    Close(true);
}