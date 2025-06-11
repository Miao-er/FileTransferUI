#include "MainFrame.h"
#include "ServerConfigDialog.h"
#include "FileExplorerFrame.h"
#include <wx/msgdlg.h>

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_BUTTON(ID_ADD_SERVER, MainFrame::OnAdd)
    EVT_BUTTON(ID_EDIT_SERVER, MainFrame::OnEdit)
    EVT_BUTTON(ID_DELETE_SERVER, MainFrame::OnDelete)
    EVT_BUTTON(ID_CONNECT_SERVER, MainFrame::OnConnect)
    EVT_LIST_ITEM_SELECTED(ID_SERVER_LIST, MainFrame::OnServerSelected)
    EVT_LIST_ITEM_ACTIVATED(ID_SERVER_LIST, MainFrame::OnServerDoubleClick)
    EVT_CLOSE(MainFrame::OnClose)
wxEND_EVENT_TABLE()

MainFrame::MainFrame() 
    : wxFrame(nullptr, wxID_ANY, "文件上传客户端", 
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
    SetStatusText("就绪");
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
    m_serverList->AppendColumn("名称", wxLIST_FORMAT_LEFT, 200);
    m_serverList->AppendColumn("IP地址", wxLIST_FORMAT_LEFT, 150);
    m_serverList->AppendColumn("端口", wxLIST_FORMAT_LEFT, 100);
    
    // 创建按钮
    m_addBtn = new wxButton(panel, ID_ADD_SERVER, "添加(+)");
    m_editBtn = new wxButton(panel, ID_EDIT_SERVER, "编辑");
    m_deleteBtn = new wxButton(panel, ID_DELETE_SERVER, "删除");
    m_connectBtn = new wxButton(panel, ID_CONNECT_SERVER, "连接");
    
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
    btnSizer->Add(m_connectBtn, 0);
    
    mainSizer->Add(new wxStaticText(panel, wxID_ANY, "服务器列表:"), 
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
        SetStatusText("服务器配置已添加");
    }
}

void MainFrame::OnEdit(wxCommandEvent& event) {
    int index = GetSelectedServerIndex();
    if (index == -1) return;
    
    const ServerInfo& server = m_config->GetServer(index);
    ServerConfigDialog dialog(this, server, "编辑服务器");
    
    if (dialog.ShowModal() == wxID_OK) {
        ServerInfo newServer = dialog.GetServerInfo();
        m_config->UpdateServer(index, newServer);
        m_config->SaveConfig();
        RefreshServerList();
        SetStatusText("服务器配置已更新");
    }
}

void MainFrame::OnDelete(wxCommandEvent& event) {
    int index = GetSelectedServerIndex();
    if (index == -1) return;
    
    const ServerInfo& server = m_config->GetServer(index);
    wxString message = wxString::Format("确定要删除服务器 '%s' 吗？", server.name);
    
    if (wxMessageBox(message, "确认删除", wxYES_NO | wxICON_QUESTION) == wxYES) {
        m_config->RemoveServer(index);
        m_config->SaveConfig();
        RefreshServerList();
        SetStatusText("服务器配置已删除");
    }
}

void MainFrame::OnConnect(wxCommandEvent& event) {
    int index = GetSelectedServerIndex();
    if (index == -1) return;
    
    const ServerInfo& server = m_config->GetServer(index);
    printf("OnConnect: %s\n", std::string(server.name).c_str());
    // 如果已有文件浏览器窗口，先关闭它
    if (m_explorerFrame) {
        printf("OnConnect: %s\n", "close");
        m_explorerFrame->Destroy();
        m_explorerFrame = nullptr;
    }
    
    // 创建新的文件浏览器窗口
    m_explorerFrame = new FileExplorerFrame(this, server);
    m_explorerFrame->Show(true);
    
    // 隐藏主窗口
    Hide();
    
    SetStatusText(wxString::Format("已连接到 %s", server.name));
}

void MainFrame::OnServerSelected(wxListEvent& event) {
    UpdateButtonStates();
}

void MainFrame::OnServerDoubleClick(wxListEvent& event) {
    // 创建一个命令事件对象并调用连接处理函数
    wxCommandEvent cmdEvent(wxEVT_COMMAND_BUTTON_CLICKED, ID_CONNECT_SERVER);
    OnConnect(cmdEvent);
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