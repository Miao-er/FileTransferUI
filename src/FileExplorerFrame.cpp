#include "FileExplorerFrame.h"
#include "UploadProgressDialog.h"
#include <wx/msgdlg.h>
#include <wx/filename.h>
#include "MainFrame.h"

wxBEGIN_EVENT_TABLE(FileExplorerFrame, wxFrame)
    EVT_TREE_SEL_CHANGED(wxID_ANY, FileExplorerFrame::OnDirSelected)

    EVT_LIST_ITEM_SELECTED(wxID_ANY, FileExplorerFrame::OnFileSelected)
    EVT_LIST_ITEM_ACTIVATED(wxID_ANY, FileExplorerFrame::OnFileDoubleClick)
    EVT_BUTTON(wxID_UP, FileExplorerFrame::OnUpDirectory)
    EVT_BUTTON(wxID_APPLY, FileExplorerFrame::OnUpload)
    EVT_BUTTON(wxID_BACKWARD, FileExplorerFrame::OnBack)
    EVT_CLOSE(FileExplorerFrame::OnClose)
wxEND_EVENT_TABLE()

FileExplorerFrame::FileExplorerFrame(wxWindow* parent, const ServerInfo& server)
    : wxFrame(parent, wxID_ANY, 
              wxString::Format("文件浏览器 - %s", server.name),
              wxDefaultPosition, wxSize(900, 700)),
      m_serverInfo(server), m_progressDialog(nullptr) {
    
    InitializeUI();
    PopulateDirectoryTree();
    
    // 设置初始路径为根目录
    m_currentPath = wxFileName::GetPathSeparator();
    PopulateFileList(m_currentPath);
    
    UpdateStatus(wxString::Format("已连接到服务器: %s:%d", server.ip, server.port));
}

FileExplorerFrame::~FileExplorerFrame() {
    // 安全地清理进度对话框
    printf("FileExplorerFrame::~FileExplorerFrame() start\n");
    if (m_progressDialog) {
        if (m_progressDialog->IsModal()) {
            m_progressDialog->EndModal(wxID_CANCEL);
    }
        m_progressDialog->Destroy();
        m_progressDialog = nullptr;
    }
    printf("FileExplorerFrame::~FileExplorerFrame()\n");
    // 清理文件列表中的内存
    for (long i = 0; i < m_fileList->GetItemCount(); ++i) {
        wxString* path = reinterpret_cast<wxString*>(m_fileList->GetItemData(i));
        delete path;
    }
}

void FileExplorerFrame::InitializeUI() {
    wxPanel* panel = new wxPanel(this);
    
    // 创建分割窗口
    m_splitter = new wxSplitterWindow(panel, wxID_ANY);
    
    // 左侧目录树
    m_dirTree = new wxTreeCtrl(m_splitter, wxID_ANY, 
                               wxDefaultPosition, wxDefaultSize,
                               wxTR_DEFAULT_STYLE | wxTR_SINGLE);
    
    // 右侧文件列表
    m_fileList = new wxListCtrl(m_splitter, wxID_ANY,
                                wxDefaultPosition, wxDefaultSize,
                                wxLC_REPORT | wxLC_SINGLE_SEL);
    
    // 设置文件列表的列
    m_fileList->AppendColumn("名称", wxLIST_FORMAT_LEFT, 300);
    m_fileList->AppendColumn("大小", wxLIST_FORMAT_RIGHT, 100);
    m_fileList->AppendColumn("类型", wxLIST_FORMAT_LEFT, 100);
    
    // 分割窗口设置
    m_splitter->SplitVertically(m_dirTree, m_fileList, 250);
    m_splitter->SetMinimumPaneSize(200);
    
    // 按钮 - 添加返回上级按钮
    m_upBtn = new wxButton(panel, wxID_UP, "返回上级");
    m_uploadBtn = new wxButton(panel, wxID_APPLY, "上传选中文件");
    m_backBtn = new wxButton(panel, wxID_BACKWARD, "返回主界面");
    m_uploadBtn->Enable(false);
    
    // 状态文本
    m_statusText = new wxStaticText(panel, wxID_ANY, "就绪");
    
    // 布局
    wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    btnSizer->Add(m_upBtn, 0, wxRIGHT, 10);
    btnSizer->Add(m_uploadBtn, 0, wxRIGHT, 10);
    btnSizer->AddStretchSpacer();
    btnSizer->Add(m_backBtn, 0);
    
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    mainSizer->Add(m_splitter, 1, wxEXPAND | wxALL, 5);
    mainSizer->Add(btnSizer, 0, wxEXPAND | wxALL, 10);
    mainSizer->Add(m_statusText, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    
    panel->SetSizer(mainSizer);
    
    // 绑定事件
    m_upBtn->Bind(wxEVT_COMMAND_BUTTON_CLICKED, 
                  &FileExplorerFrame::OnUpDirectory, this, wxID_UP);
    m_uploadBtn->Bind(wxEVT_COMMAND_BUTTON_CLICKED, 
                      &FileExplorerFrame::OnUpload, this, wxID_APPLY);
    m_backBtn->Bind(wxEVT_COMMAND_BUTTON_CLICKED, 
                    &FileExplorerFrame::OnBack, this, wxID_BACKWARD);
}

void FileExplorerFrame::PopulateDirectoryTree() {
    m_dirTree->DeleteAllItems();
    // 添加根节点
    wxTreeItemId rootId = m_dirTree->AddRoot("文件系统");
    
    // 获取所有驱动器（Windows）或根目录（Unix）
#ifdef __WXMSW__
    wxArrayString drives = wxFSFile::GetAvailableDrives();
    for (const wxString& drive : drives) {
        wxTreeItemId driveId = m_dirTree->AppendItem(rootId, drive);
        AddDirectoryChildren(driveId, drive);
}
#else
    // Unix系统，从根目录开始
    wxTreeItemId rootDirId = m_dirTree->AppendItem(rootId, "/");
    AddDirectoryChildren(rootDirId, "/");
#endif

    m_dirTree->Expand(rootId);
    }
    
void FileExplorerFrame::AddDirectoryChildren(wxTreeItemId parent, const wxString& path) {
    wxDir dir(path);
    if (!dir.IsOpened()) return;
    
    wxString filename;
    bool cont = dir.GetFirst(&filename, wxEmptyString, wxDIR_DIRS);
    
    while (cont) {
        wxString fullPath = path;
        if (!fullPath.EndsWith(wxFileName::GetPathSeparator())) {
            fullPath += wxFileName::GetPathSeparator();
}
        fullPath += filename;

        wxTreeItemId childId = m_dirTree->AppendItem(parent, filename);
        // 使用自定义的 TreeItemData
        m_dirTree->SetItemData(childId, new TreeItemData(fullPath));
        
        // 添加一个虚拟子项，以便显示展开图标
        if (HasSubdirectories(fullPath)) {
            m_dirTree->AppendItem(childId, "...");
}

        cont = dir.GetNext(&filename);
    }
}

bool FileExplorerFrame::HasSubdirectories(const wxString& path) {
    wxDir dir(path);
    if (!dir.IsOpened()) return false;
    
    wxString filename;
    return dir.GetFirst(&filename, wxEmptyString, wxDIR_DIRS);
}

void FileExplorerFrame::PopulateFileList(const wxString& path) {
    printf("PopulateFileList: Starting with path: %s\n", path.ToStdString().c_str());
    
    // 清理旧的内存
    for (long i = 0; i < m_fileList->GetItemCount(); ++i) {
        wxString* oldPath = reinterpret_cast<wxString*>(m_fileList->GetItemData(i));
        delete oldPath;
    }
    
    m_fileList->DeleteAllItems();
    m_currentPath = path;
    
    wxDir dir(path);
    if (!dir.IsOpened()) {
        printf("PopulateFileList: Cannot open directory: %s\n", path.ToStdString().c_str());
        UpdateStatus("无法访问目录: " + path);
        return;
    }
    
    printf("PopulateFileList: Directory opened successfully\n");
    
    long index = 0;
    
    // 添加返回上级目录项（除非已经在根目录）
    wxFileName currentDir(path);
    if (currentDir.GetDirCount() > 0 || !currentDir.GetVolume().IsEmpty()) {
        long itemIndex = m_fileList->InsertItem(index, "..");
        m_fileList->SetItem(itemIndex, 1, "<上级目录>");
        m_fileList->SetItem(itemIndex, 2, "文件夹");
        
        // 计算父目录路径
        wxFileName parentDir(path);
        parentDir.RemoveLastDir();
        wxString parentPath = parentDir.GetPath();
        if (parentPath.IsEmpty()) {
#ifdef __WXMSW__
            parentPath = parentDir.GetVolume() + ":";
#else
            parentPath = "/";
#endif
        }
        
        printf("PopulateFileList: Added parent directory item: %s\n", parentPath.ToStdString().c_str());
        m_fileList->SetItemData(itemIndex, reinterpret_cast<wxUIntPtr>(new wxString(parentPath)));
        index++;
    }
    
    // 先添加目录
    wxString filename;
    bool cont = dir.GetFirst(&filename, wxEmptyString, wxDIR_DIRS);
    int dirCount = 0;
    
    while (cont) {
        // 跳过隐藏目录（以.开头的目录）
        if (!filename.StartsWith(".")) {
        wxString fullPath = path;
        if (!fullPath.EndsWith(wxFileName::GetPathSeparator())) {
            fullPath += wxFileName::GetPathSeparator();
        }
        fullPath += filename;
        
        long itemIndex = m_fileList->InsertItem(index, filename);
        m_fileList->SetItem(itemIndex, 1, "<目录>");
        m_fileList->SetItem(itemIndex, 2, "文件夹");
        m_fileList->SetItemData(itemIndex, reinterpret_cast<wxUIntPtr>(new wxString(fullPath)));
        
            printf("PopulateFileList: Added directory: %s -> %s\n", 
                   filename.ToStdString().c_str(), fullPath.ToStdString().c_str());
            
        index++;
            dirCount++;
    }
        cont = dir.GetNext(&filename);
    }
    
    // 再添加文件
    cont = dir.GetFirst(&filename, wxEmptyString, wxDIR_FILES);
    int fileCount = 0;
    
    while (cont) {
        // 跳过隐藏文件（以.开头的文件）
        if (!filename.StartsWith(".")) {
            wxString fullPath = path;
            if (!fullPath.EndsWith(wxFileName::GetPathSeparator())) {
                fullPath += wxFileName::GetPathSeparator();
            }
            fullPath += filename;
            
            wxFileName fn(fullPath);
            long itemIndex = m_fileList->InsertItem(index, filename);
            
            // 文件大小
            wxULongLong size = fn.GetSize();
            wxString sizeStr = FormatFileSize(size);
            m_fileList->SetItem(itemIndex, 1, sizeStr);
            
            // 文件类型
            wxString ext = fn.GetExt().Lower();
            wxString type = GetFileType(ext);
            m_fileList->SetItem(itemIndex, 2, type);
            
            m_fileList->SetItemData(itemIndex, reinterpret_cast<wxUIntPtr>(new wxString(fullPath)));
            
            index++;
            fileCount++;
        }
        cont = dir.GetNext(&filename);
    }
    
    // 更新返回上级按钮状态
    wxFileName currentDirCheck(path);
    m_upBtn->Enable(currentDirCheck.GetDirCount() > 0 || !currentDirCheck.GetVolume().IsEmpty());
    
    printf("PopulateFileList: Added %d directories and %d files\n", dirCount, fileCount);
    UpdateStatus(wxString::Format("当前目录: %s (%ld 项)", path, index));
}

wxString FileExplorerFrame::FormatFileSize(wxULongLong size) {
    if (size < 1024) {
        return wxString::Format("%llu B", size.GetValue());
    } else if (size < 1024 * 1024) {
        return wxString::Format("%.1f KB", size.ToDouble() / 1024);
    } else if (size < 1024 * 1024 * 1024) {
        return wxString::Format("%.1f MB", size.ToDouble() / (1024 * 1024));
    } else {
        return wxString::Format("%.1f GB", size.ToDouble() / (1024 * 1024 * 1024));
}
}

wxString FileExplorerFrame::GetFileType(const wxString& ext) {
    if (ext.IsEmpty()) return "文件";
    
    // 常见文件类型映射
    if (ext == "txt" || ext == "log") return "文本文件";
    if (ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "gif") return "图像文件";
    if (ext == "mp4" || ext == "avi" || ext == "mkv") return "视频文件";
    if (ext == "mp3" || ext == "wav" || ext == "flac") return "音频文件";
    if (ext == "pdf") return "PDF文档";
    if (ext == "doc" || ext == "docx") return "Word文档";
    if (ext == "xls" || ext == "xlsx") return "Excel文档";
    if (ext == "zip" || ext == "rar" || ext == "7z") return "压缩文件";
    
    return ext.Upper() + "文件";
}

void FileExplorerFrame::OnDirSelected(wxTreeEvent& event) {
    wxTreeItemId itemId = event.GetItem();
    if (!itemId.IsOk()) return;
    
    // 检查是否需要展开子目录
    if (m_dirTree->GetChildrenCount(itemId) == 1) {
        wxTreeItemIdValue cookie;
        wxTreeItemId childId = m_dirTree->GetFirstChild(itemId, cookie);
        if (m_dirTree->GetItemText(childId) == "...") {
            // 删除虚拟子项并添加真实子项
            m_dirTree->Delete(childId);
            
            // 使用自定义的 TreeItemData
            TreeItemData* data = dynamic_cast<TreeItemData*>(m_dirTree->GetItemData(itemId));
            if (data) {
                AddDirectoryChildren(itemId, data->GetPath());
    }
        }
    }
    
    // 获取选中目录的路径
    TreeItemData* data = dynamic_cast<TreeItemData*>(m_dirTree->GetItemData(itemId));
    if (data) {
        PopulateFileList(data->GetPath());
}
}

void FileExplorerFrame::OnFileSelected(wxListEvent& event) {
    long index = event.GetIndex();
    wxString* fullPath = reinterpret_cast<wxString*>(m_fileList->GetItemData(index));
    
    if (!fullPath) {
        printf("OnFileSelected: fullPath is null\n");
        return;
    }
    
    printf("OnFileSelected: path = %s\n", fullPath->ToStdString().c_str());
    
    if (wxFileName::FileExists(*fullPath)) {
        m_selectedFile = *fullPath;
        m_uploadBtn->Enable(true);
        UpdateStatus("已选择文件: " + wxFileName(*fullPath).GetFullName());
        printf("OnFileSelected: Selected file: %s\n", fullPath->ToStdString().c_str());
    } else if (wxFileName::DirExists(*fullPath)) {
        m_uploadBtn->Enable(false);
        UpdateStatus("已选择目录: " + *fullPath + " (双击进入)");
        printf("OnFileSelected: Selected directory: %s\n", fullPath->ToStdString().c_str());
    } else {
        m_uploadBtn->Enable(false);
        UpdateStatus("请选择一个文件进行上传");
        printf("OnFileSelected: Invalid path: %s\n", fullPath->ToStdString().c_str());
    }
}

void FileExplorerFrame::OnFileDoubleClick(wxListEvent& event) {
    long index = event.GetIndex();
    wxString* fullPath = reinterpret_cast<wxString*>(m_fileList->GetItemData(index));
    
    if (!fullPath) {
        printf("OnFileDoubleClick: fullPath is null\n");
        return;
    }
    
    printf("OnFileDoubleClick: path = %s\n", fullPath->ToStdString().c_str());
    printf("OnFileDoubleClick: DirExists = %d\n", wxFileName::DirExists(*fullPath));
    printf("OnFileDoubleClick: FileExists = %d\n", wxFileName::FileExists(*fullPath));
    
    if (wxFileName::DirExists(*fullPath)) {
        printf("OnFileDoubleClick: Entering directory: %s\n", fullPath->ToStdString().c_str());
        
        // 双击目录，进入该目录
        PopulateFileList(*fullPath);
        
        // 同步更新目录树选择（可选）
        SyncDirectoryTreeSelection(*fullPath);
        
        // 更新状态
        UpdateStatus("进入目录: " + *fullPath);
    } else if (wxFileName::FileExists(*fullPath)) {
        printf("OnFileDoubleClick: Selecting file: %s\n", fullPath->ToStdString().c_str());
        
        // 双击文件，选中该文件（用于上传）
        m_selectedFile = *fullPath;
        m_uploadBtn->Enable(true);
        UpdateStatus("已选择文件: " + wxFileName(*fullPath).GetFullName());
    } else {
        printf("OnFileDoubleClick: Path does not exist: %s\n", fullPath->ToStdString().c_str());
    }
}

void FileExplorerFrame::OnUpload(wxCommandEvent& event) {
    if (m_selectedFile.IsEmpty()) {
        wxMessageBox("请先选择要上传的文件", "提示", wxOK | wxICON_INFORMATION);
                return;
            }
    
    // 创建上传进度对话框
    m_progressDialog = new UploadProgressDialog(this, m_selectedFile);
    
    // 先启动上传线程
    if (!m_progressDialog->StartUpload()) {
        wxMessageBox("无法启动上传任务", "错误", wxOK | wxICON_ERROR);
        m_progressDialog->Destroy();
        return;
    }
    
    // 显示进度对话框并等待结果
    int result = m_progressDialog->ShowModal();
    
    // 安全地销毁进度对话框
    if (m_progressDialog) {
        m_progressDialog->Destroy();
        m_progressDialog = nullptr;
}
    
    // 根据结果显示消息并处理后续操作
    if (result == wxID_OK) {
        wxMessageBox("文件上传成功！", "完成", wxOK | wxICON_INFORMATION);
        // 直接返回主界面，不使用 CallAfter
        //ReturnToMainFrame();
        printf("complete\n");
    } else {
        wxMessageBox("文件上传失败！", "错误", wxOK | wxICON_ERROR);
    }
    printf("finish upload\n");
}

void FileExplorerFrame::OnBack(wxCommandEvent& event) {
    ReturnToMainFrame();
    }
    
void FileExplorerFrame::ReturnToMainFrame() {
    // 显示主窗口
    if (GetParent()) {
        GetParent()->Show(true);
        GetParent()->Raise(); // 确保窗口置于前台
}   
    printf("ReturnToMainFrame\n");
    // 直接关闭当前窗口，不使用 CallAfter
    Close(true);
}

void FileExplorerFrame::OnClose(wxCloseEvent& event) {
    // 显示主窗口
    printf("OnClose\n");
    auto parent = GetParent();
    if (parent) {
        parent->Show(true);

        printf("Show true\n");

        dynamic_cast<MainFrame*>(parent)->OnFileExplorerFrameClose();
        printf("OnFileExplorerFrameClose\n");
    }
    // 允许窗口关闭
    event.Skip();
    printf("Skip\n");
}

void FileExplorerFrame::UpdateStatus(const wxString& message) {
    m_statusText->SetLabel(message);
}

void FileExplorerFrame::OnUpDirectory(wxCommandEvent& event) {
    wxFileName currentDir(m_currentPath);
    
    // 获取父目录
    if (currentDir.GetDirCount() > 0) {
        currentDir.RemoveLastDir();
        wxString parentPath = currentDir.GetPath();
        
        if (parentPath.IsEmpty()) {
            // 到达根目录
#ifdef __WXMSW__
            parentPath = currentDir.GetVolume() + ":";
#else
            parentPath = "/";
#endif
    }
    
        PopulateFileList(parentPath);
        SyncDirectoryTreeSelection(parentPath);
    }
}

void FileExplorerFrame::SyncDirectoryTreeSelection(const wxString& path) {
    // 在目录树中查找并选中对应路径的节点
    // 这是一个递归查找过程
    
    wxTreeItemId rootId = m_dirTree->GetRootItem();
    if (!rootId.IsOk()) return;
    
    // 从根节点开始查找匹配的路径
    FindAndSelectTreeItem(rootId, path);
}

// 添加辅助方法来递归查找树节点
void FileExplorerFrame::FindAndSelectTreeItem(wxTreeItemId parentId, const wxString& targetPath) {
    // 检查当前节点是否匹配
    TreeItemData* data = dynamic_cast<TreeItemData*>(m_dirTree->GetItemData(parentId));
    if (data && data->GetPath() == targetPath) {
        m_dirTree->SelectItem(parentId);
        m_dirTree->EnsureVisible(parentId);
        return;
    }
    
    // 递归检查子节点
    wxTreeItemIdValue cookie;
    wxTreeItemId childId = m_dirTree->GetFirstChild(parentId, cookie);
    
    while (childId.IsOk()) {
        // 检查子节点的路径是否是目标路径的前缀
        TreeItemData* childData = dynamic_cast<TreeItemData*>(m_dirTree->GetItemData(childId));
        if (childData) {
            wxString childPath = childData->GetPath();
            if (targetPath.StartsWith(childPath)) {
                // 如果需要展开子节点
                if (m_dirTree->GetChildrenCount(childId) == 1) {
                    wxTreeItemIdValue subCookie;
                    wxTreeItemId subChild = m_dirTree->GetFirstChild(childId, subCookie);
                    if (subChild.IsOk() && m_dirTree->GetItemText(subChild) == "...") {
                        // 删除虚拟子项并添加真实子项
                        m_dirTree->Delete(subChild);
                        AddDirectoryChildren(childId, childPath);
                    }
                }
                
                // 递归查找
                FindAndSelectTreeItem(childId, targetPath);
                return;
            }
        }
        
        childId = m_dirTree->GetNextChild(parentId, cookie);
    }
}