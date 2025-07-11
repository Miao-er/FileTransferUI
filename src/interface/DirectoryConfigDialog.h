#ifndef DIRECTORYCONFIGDIALOG_H
#define DIRECTORYCONFIGDIALOG_H

#include <wx/wx.h>
#include <wx/treectrl.h>
#include "../utils/LocalConf.h"
/**
 * @brief 自定义树节点数据类
 */
class DirTreeItemData : public wxTreeItemData {
public:
    DirTreeItemData(const wxString& path) : m_path(path) {}
    const wxString& GetPath() const { return m_path; }
private:
    wxString m_path;
};

/**
 * @brief 目录配置对话框
 */
class DirectoryConfigDialog : public wxDialog {
public:
    DirectoryConfigDialog(wxWindow* parent);
    
    /**
     * @brief 获取选定的目录路径
     * @return 目录路径
     */
    wxString GetSelectedPath() const { return m_selectedPath; }

private:
    // UI 组件
    wxTreeCtrl* m_dirTree;
    wxStaticText* m_pathLabel;
    wxStaticText* m_savedPathLabel; // 新增
    wxButton* m_confirmBtn;
    wxButton* m_cancelBtn;

    // 数据成员
    LocalConf* m_localConf;
    wxString m_selectedPath;
    wxString m_savedPath; // 当前路径，用于加载和保存
    // 私有方法
    void InitializeUI();
    void LoadCurrentPath();
    void SavePathToConfig(const wxString& path);
    void PopulateDirectoryTree();
    void AddDirectoryChildren(wxTreeItemId parent, const wxString& path);
    void UpdatePathDisplay();
    void ExpandAndSelectPath(const wxString& path); // 新增
    // 事件处理
    void OnDirSelected(wxTreeEvent& event);
    void OnDirExpanding(wxTreeEvent& event);
    void OnConfirm(wxCommandEvent& event);
    void OnCancel(wxCommandEvent& event);
    wxDECLARE_EVENT_TABLE();
};

#endif
