#include "FileExplorerFrame.h"
#include "UploadProgressDialog.h"
#include <wx/msgdlg.h>
#include <wx/filename.h>
#include "MainFrame.h"

wxBEGIN_EVENT_TABLE(FileExplorerFrame, wxFrame)
    EVT_TREE_SEL_CHANGED(wxID_ANY, FileExplorerFrame::OnDirSelected)
    EVT_TREE_ITEM_EXPANDING(wxID_ANY, FileExplorerFrame::OnDirExpanding)
    EVT_LIST_ITEM_SELECTED(wxID_ANY, FileExplorerFrame::OnFileSelected)
    EVT_LIST_ITEM_ACTIVATED(wxID_ANY, FileExplorerFrame::OnFileDoubleClick)
    EVT_BUTTON(wxID_UP, FileExplorerFrame::OnUpDirectory)
    EVT_BUTTON(wxID_APPLY, FileExplorerFrame::OnUpload)
    EVT_BUTTON(wxID_BACKWARD, FileExplorerFrame::OnBack)
    EVT_CLOSE(FileExplorerFrame::OnClose)
wxEND_EVENT_TABLE()

FileExplorerFrame::FileExplorerFrame(wxWindow* parent, const ServerInfo& server, HwRdma *hwrdma, StreamControl *stream_control, LocalConf *localConf)
    : wxFrame(parent, wxID_ANY, 
              wxString::Format("File Explorer - %s", server.name),
              wxDefaultPosition, wxSize(900, 700)),
      m_serverInfo(server), m_progressDialog(nullptr), m_hwrdma(hwrdma), m_streamControl(stream_control), m_localConf(localConf) {
    InitializeUI();
    PopulateDirectoryTree();
    
    // 设置初始路径为根目录
    m_currentPath = wxFileName::GetPathSeparator();
    PopulateFileList(m_currentPath);
    
    UpdateStatus(wxString::Format("Connected to Server: %s:%d", server.ip, server.port));
}

FileExplorerFrame::~FileExplorerFrame() {
    if (m_progressDialog) {
        m_progressDialog->Destroy();
        m_progressDialog = nullptr;
    }
    // 清理文件列表中的内存
    for (long i = 0; i < m_fileList->GetItemCount(); ++i) {
        wxString* path = reinterpret_cast<wxString*>(m_fileList->GetItemData(i));
        delete path;
    }
    if(m_serverInfo.fd > 0)
    {
        printf("FileExplorerFrame destructor called, closing socket fd: %d\n", m_serverInfo.fd);
        close(m_serverInfo.fd);
        m_serverInfo.fd = -1;
    }

    if(m_localConf) {
        delete m_localConf;
        m_localConf = nullptr;
    }
    if (m_streamControl) {
        delete m_streamControl;
        m_streamControl = nullptr;
    }
    if (m_hwrdma) {
        delete m_hwrdma;
        m_hwrdma = nullptr;
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
    m_fileList->AppendColumn("name", wxLIST_FORMAT_LEFT, 300);
    m_fileList->AppendColumn("size", wxLIST_FORMAT_RIGHT, 100);
    m_fileList->AppendColumn("type", wxLIST_FORMAT_LEFT, 100);
    
    // 分割窗口设置
    m_splitter->SplitVertically(m_dirTree, m_fileList, 250);
    m_splitter->SetMinimumPaneSize(200);
    
    // 按钮 - 添加返回上级按钮
    m_upBtn = new wxButton(panel, wxID_UP, "Back");
    m_uploadBtn = new wxButton(panel, wxID_APPLY, "Upload");
    m_backBtn = new wxButton(panel, wxID_BACKWARD, "Return");
    m_uploadBtn->Enable(false);
    
    // 状态文本
    m_statusText = new wxStaticText(panel, wxID_ANY, "Ready");
    
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
    wxTreeItemId rootId = m_dirTree->AddRoot("/");
    
    // 获取所有驱动器（Windows）或根目录（Unix）
#ifdef __WXMSW__
    wxArrayString drives = wxFSFile::GetAvailableDrives();
    for (const wxString& drive : drives) {
        wxTreeItemId driveId = m_dirTree->AppendItem(rootId, drive);
        AddDirectoryChildren(driveId, drive);
}
#else
    // Unix系统，从根目录开始
    // wxTreeItemId rootDirId = m_dirTree->AppendItem(rootId, "/");
    m_dirTree->SetItemData(rootId, new TreeItemData("/"));
    m_dirTree->SetItemHasChildren(rootId, true);
    // AddDirectoryChildren(rootId, "/");
#endif
    m_dirTree->Expand(rootId);
    }
    
void FileExplorerFrame::AddDirectoryChildren(wxTreeItemId parent, const wxString& path) {
    wxDir dir(path);
    if (!dir.IsOpened()) return;

    // 先清空所有子项
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
    dirNames.Sort(); // 字典序排序

    if (dirNames.IsEmpty()) {
        // 空文件夹不添加任何子项
        return;
    }

    for (auto& dirname : dirNames) {
        wxString fullPath = path;
        if (!fullPath.EndsWith(wxFileName::GetPathSeparator())) {
            fullPath += wxFileName::GetPathSeparator();
        }
        fullPath += dirname;

        wxTreeItemId childId = m_dirTree->AppendItem(parent, dirname);
        m_dirTree->SetItemData(childId, new TreeItemData(fullPath));
        // 折叠时需要显示可展开符号，所以添加一个虚拟子项
        //m_dirTree->AppendItem(childId, "...");
        //确保显示折叠符号
        m_dirTree->SetItemHasChildren(childId, true);
    }
}
bool FileExplorerFrame::HasSubdirectories(const wxString& path) {
    wxDir dir(path);
    if (!dir.IsOpened()) return false;
    
    wxString filename;
    return dir.GetFirst(&filename, wxEmptyString, wxDIR_DIRS);
}

void FileExplorerFrame::PopulateFileList(const wxString& path) {
    //printf("PopulateFileList: Starting with path: %s\n", path.ToStdString().c_str());
    m_currentPath = path;
    if (!m_currentPath.EndsWith(wxFileName::GetPathSeparator())) {
            m_currentPath += wxFileName::GetPathSeparator();
        }
    // 清理旧的内存
    for (long i = 0; i < m_fileList->GetItemCount(); ++i) {
        wxString* oldPath = reinterpret_cast<wxString*>(m_fileList->GetItemData(i));
        delete oldPath;
    }
    m_fileList->DeleteAllItems();
    //m_currentPath = path;
    wxDir dir(m_currentPath);
    if (!dir.IsOpened()) {
        printf("PopulateFileList: Cannot open directory: %s\n", m_currentPath.ToStdString().c_str());
        UpdateStatus("Cannot access root dir: " + m_currentPath);
        return;
    }
    
    //printf("PopulateFileList: Directory opened successfully\n");
    
    long index = 0;
    
    // 添加返回上级目录项（除非已经在根目录）
    wxFileName currentDir(m_currentPath);
    //printf("PopulateFileList: Current directory: %s\n", currentDir.GetFullPath().ToStdString().c_str());
    //printf("PopulateFileList: Current directory count: %ld\n", currentDir.GetDirCount());
    if (currentDir.GetDirCount() > 0 || !currentDir.GetVolume().IsEmpty()) {
        long itemIndex = m_fileList->InsertItem(index, "..");
        m_fileList->SetItem(itemIndex, 1, "");
        m_fileList->SetItem(itemIndex, 2, "folder");
        
        // 计算父目录路径
        wxFileName parentDir(m_currentPath);
        parentDir.RemoveLastDir();
        wxString parentPath = parentDir.GetPath();
        if (parentPath.IsEmpty()) {
#ifdef __WXMSW__
            parentPath = parentDir.GetVolume() + ":";
#else
            parentPath = "/";
#endif
        }
        
        //printf("PopulateFileList: Added parent directory item: %s\n", parentPath.ToStdString().c_str());
        m_fileList->SetItemData(itemIndex, reinterpret_cast<wxUIntPtr>(new wxString(parentPath)));
        index++;
    }
    
    // // 先添加目录
    wxString filename;
    bool cont = dir.GetFirst(&filename, wxEmptyString, wxDIR_DIRS);
    int dirCount = 0;
    
    // 先收集所有目录
    wxArrayString dirNames;

    while (cont) {
        if (!filename.StartsWith(".")) {
            dirNames.Add(filename);
        }
        cont = dir.GetNext(&filename);
    }
    dirNames.Sort(); // 字典序排序

    for (auto& dirname : dirNames) {
        wxString fullPath = m_currentPath;
        if (!fullPath.EndsWith(wxFileName::GetPathSeparator())) {
            fullPath += wxFileName::GetPathSeparator();
        }
        fullPath += dirname;
        long itemIndex = m_fileList->InsertItem(index, dirname);
        m_fileList->SetItem(itemIndex, 1, "");
        m_fileList->SetItem(itemIndex, 2, "folder");
        m_fileList->SetItemData(itemIndex, reinterpret_cast<wxUIntPtr>(new wxString(fullPath)));
        index++;
        dirCount++;
    }
    
    // 再收集所有文件
    wxArrayString fileNames;
    cont = dir.GetFirst(&filename, wxEmptyString, wxDIR_FILES);
    int fileCount = 0;

    while (cont) {
        if (!filename.StartsWith(".")) {
            fileNames.Add(filename);
        }
        cont = dir.GetNext(&filename);
    }
    fileNames.Sort(); // 字典序排序

    for (auto& fname : fileNames) {
        wxString fullPath = m_currentPath;
        if (!fullPath.EndsWith(wxFileName::GetPathSeparator())) {
            fullPath += wxFileName::GetPathSeparator();
        }
        fullPath += fname;
        wxFileName fn(fullPath);
        long itemIndex = m_fileList->InsertItem(index, fname);
        wxULongLong size = fn.GetSize();
        wxString sizeStr = FormatFileSize(size);
        m_fileList->SetItem(itemIndex, 1, sizeStr);
        wxString ext = fn.GetExt().Lower();
        wxString type = GetFileType(ext);
        m_fileList->SetItem(itemIndex, 2, type);
        m_fileList->SetItemData(itemIndex, reinterpret_cast<wxUIntPtr>(new wxString(fullPath)));
        index++;
        fileCount++;
    }
    
    // 更新返回上级按钮状态
    wxFileName currentDirCheck(m_currentPath);
    m_upBtn->Enable(currentDirCheck.GetDirCount() > 0 || !currentDirCheck.GetVolume().IsEmpty());
    
    //printf("PopulateFileList: Added %d directories and %d files\n", dirCount, fileCount);
    UpdateStatus(wxString::Format("currect dir: %s (%ld items)", m_currentPath, index));
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
    if (ext.IsEmpty()) return "file";
    
    // 常见文件类型映射
    if (ext == "txt" || ext == "log") return "text";
    if (ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "gif") return "image";
    if (ext == "mp4" || ext == "avi" || ext == "mkv") return "video";
    if (ext == "mp3" || ext == "wav" || ext == "flac") return "audio";
    if (ext == "pdf") return "PDF";
    if (ext == "doc" || ext == "docx") return "Word";
    if (ext == "xls" || ext == "xlsx") return "Excel";
    if (ext == "zip" || ext == "rar" || ext == "7z") return "archive";

    return ext.Upper() + " file";
}

void FileExplorerFrame::OnDirExpanding(wxTreeEvent& event) {
    wxTreeItemId itemId = event.GetItem();
    if (!itemId.IsOk()) return;

    // 始终在展开时刷新子项
    //printf("OnDirExpanding: Expanding item: %s\n", m_dirTree->GetItemText(itemId).ToStdString().c_str());
    TreeItemData* data = dynamic_cast<TreeItemData*>(m_dirTree->GetItemData(itemId));
    if (data) {
        AddDirectoryChildren(itemId, data->GetPath());
        PopulateFileList(data->GetPath());
    }
}

void FileExplorerFrame::OnDirSelected(wxTreeEvent& event) {
    wxTreeItemId itemId = event.GetItem();
    if (!itemId.IsOk()) return;
    
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
    
    //printf("OnFileSelected: path = %s\n", fullPath->ToStdString().c_str());
    
    if (wxFileName::FileExists(*fullPath)) {
        m_selectedFile = *fullPath;
        m_uploadBtn->Enable(true);
        UpdateStatus("Selected file: " + wxFileName(*fullPath).GetFullName());
        //printf("OnFileSelected: Selected file: %s\n", fullPath->ToStdString().c_str());
    } else if (wxFileName::DirExists(*fullPath)) {
        m_uploadBtn->Enable(false);
        UpdateStatus("Selected directory: " + *fullPath + " (double-click to enter)");
        //printf("OnFileSelected: Selected directory: %s\n", fullPath->ToStdString().c_str());
    } else {
        m_uploadBtn->Enable(false);
        UpdateStatus("Please select a file to upload");
        printf("OnFileSelected: Invalid path: %s\n", fullPath->ToStdString().c_str());
    }
}

void FileExplorerFrame::OnFileDoubleClick(wxListEvent& event) {
    long index = event.GetIndex();
    wxString fullPath = *reinterpret_cast<wxString*>(m_fileList->GetItemData(index));

    if (fullPath.IsEmpty()) {
        printf("OnFileDoubleClick: fullPath is null\n");
        return;
    }

    //printf("OnFileDoubleClick: path = %s\n", fullPath.ToStdString().c_str());
    //printf("OnFileDoubleClick: DirExists = %d\n", wxFileName::DirExists(fullPath));
    //printf("OnFileDoubleClick: FileExists = %d\n", wxFileName::FileExists(fullPath));

    if (wxFileName::DirExists(fullPath)) {
        //printf("OnFileDoubleClick: Entering directory: %s\n", fullPath.ToStdString().c_str());

        // 双击目录，进入该目录
        PopulateFileList(fullPath);
        //printf("OnFileDoubleClick: Populated file list for directory: %s\n", fullPath.ToStdString().c_str());
        // 同步更新目录树选择（可选）
        SyncDirectoryTreeSelection(fullPath);

        // 更新状态
        UpdateStatus("Entering directory: " + fullPath);
    } else if (wxFileName::FileExists(fullPath)) {
        //printf("OnFileDoubleClick: Selecting file: %s\n", fullPath.ToStdString().c_str());

        // 双击文件，选中该文件（用于上传）
        m_selectedFile = fullPath;
        m_uploadBtn->Enable(true);
        UpdateStatus("Selected file: " + wxFileName(fullPath).GetFullName());
    } else {
        printf("OnFileDoubleClick: Path does not exist: %s\n", fullPath.ToStdString().c_str());
    }
}

void FileExplorerFrame::OnUpload(wxCommandEvent& event) {
    if (m_selectedFile.IsEmpty()) {
        wxMessageBox("Please select a file to upload", "Tip", wxOK | wxICON_INFORMATION);
        return;
    }
    
    // 创建上传进度对话框
    m_progressDialog = new UploadProgressDialog(this, m_selectedFile, m_streamControl);
    printf("progress dialog created\n");
    // 先启动上传线程
    if (!m_progressDialog->StartUpload()) {
        wxMessageBox("Unable to start upload task", "Error", wxOK | wxICON_ERROR);
        m_progressDialog->Destroy();
        m_progressDialog = nullptr;
        return;
    }
    printf("progress dialog started\n");
    // 显示进度对话框并等待结果
    int result = m_progressDialog->ShowModal();
    printf("progress dialog modal shown\n");
    // 安全地销毁进度对话框
    if (m_progressDialog) {
        printf("progress dialog cleanup thread\n");
        m_progressDialog->Destroy();
        m_progressDialog = nullptr;
}
    
    // 根据结果显示消息并处理后续操作
    if (result == wxID_OK) {
        wxMessageBox("Upload Success", "Complete", wxOK | wxICON_INFORMATION);
        // 直接返回主界面，不使用 CallAfter
        //ReturnToMainFrame();
        printf("complete\n");
    } else if (result == 0 || result == wxID_CANCEL){
        wxMessageBox("Upload Canceled", "Cancel", wxOK | wxICON_INFORMATION);
    }
    else if(result == -2){
        wxMessageBox("Server Disconnected", "Disconnect", wxOK | wxICON_ERROR);
        ReturnToMainFrame();
    }
    else  
        wxMessageBox("Upload Error", "Error", wxOK | wxICON_ERROR);
    printf("finish upload\n");
}

void FileExplorerFrame::OnBack(wxCommandEvent& event) {
    ReturnToMainFrame();
}

void FileExplorerFrame::ReturnToMainFrame() {
    Close(true);
}

void FileExplorerFrame::OnClose(wxCloseEvent& event) {
    // 显示主窗口
    printf("back to main frame\n");
    auto parent = GetParent();
    if (parent) {
        parent->Show(true);
        dynamic_cast<MainFrame*>(parent)->OnFileExplorerFrameClose();
    }
    // 允许窗口关闭
    event.Skip();
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
    //printf("targetPath: %s\n", targetPath.ToStdString().c_str());
    while (childId.IsOk()) {
        // 检查子节点的路径是否是目标路径的前缀
        //m_dirTree->Expand(childId);
        TreeItemData* childData = dynamic_cast<TreeItemData*>(m_dirTree->GetItemData(childId));
        if (childData) {
            wxString childPath = childData->GetPath();
            if (targetPath.StartsWith(childPath)) {
                // 如果需要展开子节点
                m_dirTree->Expand(childId);
                // 递归查找
                FindAndSelectTreeItem(childId, targetPath);
                return;
            }
        }
        
        childId = m_dirTree->GetNextChild(parentId, cookie);
    }
}