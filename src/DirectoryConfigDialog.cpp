#include "DirectoryConfigDialog.h"
#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/textfile.h>
#include <wx/stdpaths.h>
#include <wx/msgdlg.h>

wxBEGIN_EVENT_TABLE(DirectoryConfigDialog, wxDialog)
    EVT_TREE_SEL_CHANGED(wxID_ANY, DirectoryConfigDialog::OnDirSelected)
    EVT_TREE_ITEM_EXPANDING(wxID_ANY, DirectoryConfigDialog::OnDirExpanding)
    EVT_BUTTON(wxID_OK, DirectoryConfigDialog::OnConfirm)
    EVT_BUTTON(wxID_CANCEL, DirectoryConfigDialog::OnCancel)
wxEND_EVENT_TABLE()

DirectoryConfigDialog::DirectoryConfigDialog(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, _T("修改本地存储位置"),
               wxDefaultPosition, wxSize(500, 500),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
    InitializeUI();
    LoadCurrentPath();
}

void DirectoryConfigDialog::InitializeUI() {
    wxPanel* panel = new wxPanel(this);

    // 新增：显示之前已保存的路径
    m_savedPathLabel = new wxStaticText(panel, wxID_ANY, _T("当前接收文件存储位置："));

    // 提示文本
    wxStaticText* promptText = new wxStaticText(panel, wxID_ANY, _T("请选择新的本地存储位置"));
    promptText->SetFont(promptText->GetFont().Bold());

    // 当前选择路径显示
    // m_pathLabel->SetFont(m_pathLabel->GetFont().Bold());

    // 目录树
    m_dirTree = new wxTreeCtrl(panel, wxID_ANY,
                               wxDefaultPosition, wxDefaultSize,
                               wxTR_DEFAULT_STYLE | wxTR_SINGLE);

    // 按钮
    m_confirmBtn = new wxButton(panel, wxID_OK, _T("确认"));
    m_cancelBtn = new wxButton(panel, wxID_CANCEL, _T("取消"));

    // 按钮布局
    wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    btnSizer->AddStretchSpacer();
    btnSizer->Add(m_confirmBtn, 0, wxRIGHT, 10);
    btnSizer->Add(m_cancelBtn, 0);

    // 主布局
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    mainSizer->Add(promptText, 0,wxALL | wxALIGN_CENTER_HORIZONTAL, 15);
    mainSizer->Add(m_savedPathLabel, 0, wxEXPAND | wxLEFT | wxRIGHT, 15); // 新增
    mainSizer->Add(m_dirTree, 1, wxEXPAND | wxLEFT | wxRIGHT, 15);
    mainSizer->Add(btnSizer, 0, wxEXPAND | wxALL, 15);

    panel->SetSizer(mainSizer);

    // 设置整体布局
    wxBoxSizer* dlgSizer = new wxBoxSizer(wxVERTICAL);
    dlgSizer->Add(panel, 1, wxEXPAND);
    SetSizer(dlgSizer);

    SetMinSize(wxSize(400, 400));
    PopulateDirectoryTree();
    m_confirmBtn->SetDefault();
}
void DirectoryConfigDialog::LoadCurrentPath() {
    // 从配置文件读取当前路径
    wxStandardPaths& paths = wxStandardPaths::Get();
    wxString configDir = paths.GetUserConfigDir() + wxFileName::GetPathSeparator() + "FileUploadClient";
    wxString configPath = configDir + wxFileName::GetPathSeparator() + "server.conf";
    
    wxString currentPath;

    if (wxFileName::FileExists(configPath)) {
        wxTextFile file(configPath);
        if (file.Open()) {
            for (size_t i = 0; i < file.GetLineCount(); ++i) {
                wxString line = file.GetLine(i).Trim();
                if (line.StartsWith("storage_path=")) {
                    m_savedPath = line.Mid(13); // 新增
                    currentPath = m_savedPath;
                    break;
                }
            }
            file.Close();
        }
    }

    // 如果没有找到配置或路径无效，使用默认路径
    if (currentPath.IsEmpty() || !wxFileName::DirExists(currentPath)) {
        currentPath = wxStandardPaths::Get().GetDocumentsDir();
        wxTextFile file;
        wxArrayString lines;
        lines.Add("storage_path=" + currentPath);
        if (file.Create(configPath) || file.Open(configPath)) {
            file.Clear();
            for (const wxString& line : lines) {
                file.AddLine(line);
            }
            file.Write();
            file.Close();
        }
    }

    m_selectedPath = currentPath;
    // 新增：显示之前保存的路径（即使无效也显示）
    if (m_savedPathLabel) {
        m_savedPathLabel->SetLabel("当前接收文件存储位置：" + m_savedPath);
    }
    UpdatePathDisplay();
}

void DirectoryConfigDialog::SavePathToConfig(const wxString& path) {
    wxStandardPaths& paths = wxStandardPaths::Get();
    wxString configDir = paths.GetUserConfigDir() + wxFileName::GetPathSeparator() + "FileUploadClient";
    wxString configPath = configDir + wxFileName::GetPathSeparator() + "server.conf";
    
    // 确保配置目录存在
    if (!wxFileName::DirExists(configDir)) {
        wxFileName::Mkdir(configDir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
    }
    
    // 读取现有配置
    wxArrayString lines;
    bool foundStoragePath = false;
    
    if (wxFileName::FileExists(configPath)) {
        wxTextFile file(configPath);
        if (file.Open()) {
            for (size_t i = 0; i < file.GetLineCount(); ++i) {
                wxString line = file.GetLine(i);
                if (line.StartsWith("storage_path=")) {
                    lines.Add("storage_path=" + path);
                    foundStoragePath = true;
                } else {
                    lines.Add(line);
                }
            }
            file.Close();
        }
    }
    
    // 如果没有找到storage_path行，添加它
    if (!foundStoragePath) {
        lines.Add("storage_path=" + path);
    }
    
    // 写回文件
    wxTextFile file;
    if (file.Create(configPath) || file.Open(configPath)) {
        file.Clear();
        for (const wxString& line : lines) {
            file.AddLine(line);
        }
        file.Write();
        file.Close();
    }
}

void DirectoryConfigDialog::PopulateDirectoryTree() {
    m_dirTree->DeleteAllItems();
    
    // 添加根节点
    
    // Unix系统：从根目录开始
    wxTreeItemId rootId = m_dirTree->AddRoot("/");
    m_dirTree->SetItemData(rootId, new DirTreeItemData("/"));
    m_dirTree->SetItemHasChildren(rootId, true);
    
    m_dirTree->Expand(rootId);
}

void DirectoryConfigDialog::AddDirectoryChildren(wxTreeItemId parent, const wxString& path) {
    wxDir dir(path);
    if (!dir.IsOpened()) return;
    
    // 清空所有子项
    m_dirTree->DeleteChildren(parent);
    
    wxArrayString dirNames;
    wxString filename;
    bool cont = dir.GetFirst(&filename, wxEmptyString, wxDIR_DIRS);
    
    while (cont) {
        if (!filename.StartsWith(".")) {
            dirNames.Add(filename);
        }
        cont = dir.GetNext(&filename);
    }
    
    dirNames.Sort();
    
    for (const wxString& dirname : dirNames) {
        wxString fullPath = path;
        if (!fullPath.EndsWith(wxFileName::GetPathSeparator())) {
            fullPath += wxFileName::GetPathSeparator();
        }
        fullPath += dirname;
        
        wxTreeItemId childId = m_dirTree->AppendItem(parent, dirname);
        m_dirTree->SetItemData(childId, new DirTreeItemData(fullPath));
        
        // 检查是否有子目录
        wxDir subDir(fullPath);
        if (subDir.IsOpened()) {
            wxString subFilename;
            if (subDir.GetFirst(&subFilename, wxEmptyString, wxDIR_DIRS)) {
                m_dirTree->SetItemHasChildren(childId, true);
            }
        }
    }
}

void DirectoryConfigDialog::UpdatePathDisplay() {
    m_savedPathLabel->SetLabel(_T("当前接收文件存储位置：") + m_savedPath);
}
void DirectoryConfigDialog::OnDirSelected(wxTreeEvent& event) {
    wxTreeItemId itemId = event.GetItem();
    if (!itemId.IsOk()) return;
    
    DirTreeItemData* data = dynamic_cast<DirTreeItemData*>(m_dirTree->GetItemData(itemId));
    if (data) {
        m_selectedPath = data->GetPath();
        UpdatePathDisplay();
    }
}

void DirectoryConfigDialog::OnDirExpanding(wxTreeEvent& event) {
    wxTreeItemId itemId = event.GetItem();
    if (!itemId.IsOk()) return;
    
    DirTreeItemData* data = dynamic_cast<DirTreeItemData*>(m_dirTree->GetItemData(itemId));
    if (data) {
        AddDirectoryChildren(itemId, data->GetPath());
    }
}

void DirectoryConfigDialog::OnConfirm(wxCommandEvent& event) {
    // 验证路径是否存在
    if (m_selectedPath.IsEmpty() || !wxFileName::DirExists(m_selectedPath)) {
        wxMessageBox(_T("请选择一个有效的目录"), _T("路径错误"), 
                     wxOK | wxICON_WARNING, this);
        return;
    }
    
    // 保存路径到配置文件
    SavePathToConfig(m_selectedPath);
    
    EndModal(wxID_OK);
}

void DirectoryConfigDialog::OnCancel(wxCommandEvent& event) {
    EndModal(wxID_CANCEL);
}
