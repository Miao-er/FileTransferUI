#ifndef FILEEXPLORERFRAME_H
#define FILEEXPLORERFRAME_H

#include <wx/wx.h>
#include <wx/treectrl.h>
#include <wx/listctrl.h>
#include <wx/splitter.h>
#include <wx/dir.h>
#include "ServerConfig.h"
#include "../net/StreamControl.h"
class UploadProgressDialog;

/**
 * @brief 自定义树节点数据类
 */
class TreeItemData : public wxTreeItemData {
public:
    TreeItemData(const wxString& path) : m_path(path) {}
    const wxString& GetPath() const { return m_path; }
private:
    wxString m_path;
};

/**
 * @brief 文件浏览器窗口
 */
class FileExplorerFrame : public wxFrame {
public:
    FileExplorerFrame(wxWindow* parent, const ServerInfo& server, StreamControl *stream_control);
    virtual ~FileExplorerFrame();

private:
    // UI 组件
    wxSplitterWindow* m_splitter;
    wxTreeCtrl* m_dirTree;
    wxListCtrl* m_fileList;
    wxButton* m_uploadBtn;
    wxButton* m_backBtn;
    wxButton* m_upBtn;  // 添加返回上级按钮
    wxStaticText* m_statusText;
    
    // 数据
    ServerInfo m_serverInfo;
    StreamControl *m_streamControl;
    wxString m_currentPath;
    wxString m_selectedFile;
    UploadProgressDialog* m_progressDialog;
    
    // 事件处理
    void OnDirSelected(wxTreeEvent& event);
    void OnDirExpanding(wxTreeEvent& event);
    void OnFileSelected(wxListEvent& event);
    void OnFileDoubleClick(wxListEvent& event);
    void OnUpload(wxCommandEvent& event);
    void OnBack(wxCommandEvent& event);
    void OnUpDirectory(wxCommandEvent& event);  // 添加返回上级方法
    void OnClose(wxCloseEvent& event);
    
    // 辅助方法
    void InitializeUI();
    void PopulateDirectoryTree();
    void PopulateFileList(const wxString& path);
    void AddDirectoryChildren(wxTreeItemId parent, const wxString& path);
    bool HasSubdirectories(const wxString& path);
    wxString GetSelectedFilePath();
    void UpdateStatus(const wxString& message);
    wxString FormatFileSize(wxULongLong size);
    wxString GetFileType(const wxString& ext);
    void ReturnToMainFrame();
    void SafeReturnToMainFrame();
    void SyncDirectoryTreeSelection(const wxString& path);
    void FindAndSelectTreeItem(wxTreeItemId parentId, const wxString& targetPath);  // 添加这行
    void AddParentDirectoryItem();
    
    wxDECLARE_EVENT_TABLE();
};

// 外部传输函数声明
extern "C" {
    int transfer_file(const char* filename);
}

#endif
