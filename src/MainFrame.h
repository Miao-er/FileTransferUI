#ifndef MAINFRAME_H
#define MAINFRAME_H

#include <wx/wx.h>
#include <wx/listctrl.h>
#include "ServerConfig.h"
#include <memory>

class ServerConfigDialog;
class FileExplorerFrame;

/**
 * @brief 主窗口类
 */
class MainFrame : public wxFrame {
public:
    MainFrame();
    ~MainFrame();
    void OnFileExplorerFrameClose();
private:
    // UI 组件
    wxListCtrl* m_serverList;
    wxButton* m_addBtn;
    wxButton* m_editBtn;
    wxButton* m_deleteBtn;
    wxButton* m_connectBtn;
    
    // 数据
    ServerConfig* m_config;
    FileExplorerFrame* m_explorerFrame;
    
    // 事件处理
    void OnAdd(wxCommandEvent& event);
    void OnEdit(wxCommandEvent& event);
    void OnDelete(wxCommandEvent& event);
    void OnConnect(wxCommandEvent& event);
    void OnServerSelected(wxListEvent& event);
    void OnServerDoubleClick(wxListEvent& event);
    void OnClose(wxCloseEvent& event);
    
    // 辅助方法
    void InitializeUI();
    void RefreshServerList();
    void UpdateButtonStates();
    int GetSelectedServerIndex();
    
    wxDECLARE_EVENT_TABLE();
};

// 自定义事件ID
enum {
    ID_ADD_SERVER = 1000,
    ID_EDIT_SERVER,
    ID_DELETE_SERVER,
    ID_CONNECT_SERVER,
    ID_SERVER_LIST
};

#endif
