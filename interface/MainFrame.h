#ifndef MAINFRAME_H
#define MAINFRAME_H

#include <wx/wx.h>
#include <wx/listctrl.h>
#include "ServerConfig.h"
#include <memory>

class ServerConfigDialog;
class FileExplorerFrame;
class DirectoryConfigDialog; // 添加前向声明

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
    wxButton* m_storageBtn; // 添加存储位置按钮
    wxButton* m_showInBrowserBtn; // 新增按钮声明
    wxButton* m_exitBtn; // 新增退出按钮
    
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
    void OnStorageLocation(wxCommandEvent& event); // 添加存储位置处理方法
    void OnShowInBrowser(wxCommandEvent& event); // 新增事件声明
    void OnExit(wxCommandEvent& event); // 新增退出事件处理方法
    void OnClose(wxCloseEvent& event);
    void OnServerDeselected(wxListEvent& event); // 新增
    
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
    ID_SERVER_LIST,
    ID_STORAGE_LOCATION, // 添加存储位置事件ID
    ID_SHOW_IN_BROWSER, // 新增事件ID
    ID_EXIT // 新增退出事件ID
};

#endif
