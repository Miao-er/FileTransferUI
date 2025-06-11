#include "UploadProgressDialog.h"
#include <wx/filename.h>
#include <wx/msgdlg.h>

// 定义自定义事件
wxDEFINE_EVENT(wxEVT_UPLOAD_PROGRESS, wxCommandEvent);
wxDEFINE_EVENT(wxEVT_UPLOAD_COMPLETE, wxCommandEvent);

wxBEGIN_EVENT_TABLE(UploadProgressDialog, wxDialog)
    EVT_BUTTON(wxID_CANCEL, UploadProgressDialog::OnCancel)
    EVT_COMMAND(wxID_ANY, wxEVT_UPLOAD_PROGRESS, UploadProgressDialog::OnProgress)
    EVT_COMMAND(wxID_ANY, wxEVT_UPLOAD_COMPLETE, UploadProgressDialog::OnComplete)
    EVT_CLOSE(UploadProgressDialog::OnClose)
wxEND_EVENT_TABLE()

UploadProgressDialog::UploadProgressDialog(wxWindow* parent, const wxString& filepath)
    : wxDialog(parent, wxID_ANY, "文件上传进度", 
               wxDefaultPosition, wxSize(400, 200),
               wxDEFAULT_DIALOG_STYLE),
      m_filepath(filepath), m_uploadThread(nullptr), m_cancelled(false) {
    
    InitializeUI();
}

UploadProgressDialog::~UploadProgressDialog() {
    if (m_uploadThread && m_uploadThread->IsRunning()) {
        m_uploadThread->Delete();
    }
}

void UploadProgressDialog::InitializeUI() {
    wxPanel* panel = new wxPanel(this);
    
    // 文件信息
    wxFileName fn(m_filepath);
    wxString filename = fn.GetFullName();
    wxULongLong fileSize = fn.GetSize();
    
    wxStaticText* fileLabel = new wxStaticText(panel, wxID_ANY, "上传文件:");
    wxStaticText* fileText = new wxStaticText(panel, wxID_ANY, filename);
    
    wxStaticText* sizeLabel = new wxStaticText(panel, wxID_ANY, "文件大小:");
    wxStaticText* sizeText = new wxStaticText(panel, wxID_ANY, FormatFileSize(fileSize));
    
    // 进度条
    m_progressGauge = new wxGauge(panel, wxID_ANY, 100);
    m_progressText = new wxStaticText(panel, wxID_ANY, "0%");
    
    // 状态文本
    m_statusText = new wxStaticText(panel, wxID_ANY, "准备上传...");
    
    // 取消按钮
    wxButton* cancelBtn = new wxButton(panel, wxID_CANCEL, "取消");
    
    // 布局
    wxFlexGridSizer* infoSizer = new wxFlexGridSizer(2, 2, 5, 10);
    infoSizer->AddGrowableCol(1);
    infoSizer->Add(fileLabel, 0, wxALIGN_CENTER_VERTICAL);
    infoSizer->Add(fileText, 1, wxEXPAND);
    infoSizer->Add(sizeLabel, 0, wxALIGN_CENTER_VERTICAL);
    infoSizer->Add(sizeText, 1, wxEXPAND);
    
    wxBoxSizer* progressSizer = new wxBoxSizer(wxHORIZONTAL);
    progressSizer->Add(m_progressGauge, 1, wxALIGN_CENTER_VERTICAL);
    progressSizer->Add(m_progressText, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 10);
    
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    mainSizer->Add(infoSizer, 0, wxEXPAND | wxALL, 15);
    mainSizer->Add(progressSizer, 0, wxEXPAND | wxLEFT | wxRIGHT, 15);
    mainSizer->Add(m_statusText, 0, wxEXPAND | wxALL, 15);
    mainSizer->Add(cancelBtn, 0, wxALIGN_CENTER | wxALL, 15);
    
    panel->SetSizer(mainSizer);
}

wxString UploadProgressDialog::FormatFileSize(wxULongLong size) {
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

bool UploadProgressDialog::StartUpload() {
    // 创建上传线程
    m_uploadThread = new UploadThread(this, m_filepath);
    
    if (m_uploadThread->Create() != wxTHREAD_NO_ERROR) {
        delete m_uploadThread;
        m_uploadThread = nullptr;
        return false;
    }
    
    if (m_uploadThread->Run() != wxTHREAD_NO_ERROR) {
        delete m_uploadThread;
        m_uploadThread = nullptr;
        return false;
    }
    
    // 不在这里调用 ShowModal，让调用者控制
    return true;
}

void UploadProgressDialog::UpdateProgress(int percentage, const wxString& status) {
    m_progressGauge->SetValue(percentage);
    m_progressText->SetLabel(wxString::Format("%d%%", percentage));
    m_statusText->SetLabel(status);
}

void UploadProgressDialog::OnProgress(wxCommandEvent& event) {
    int percentage = event.GetInt();
    wxString status = event.GetString();
    UpdateProgress(percentage, status);
}

void UploadProgressDialog::OnComplete(wxCommandEvent& event) {
    bool success = event.GetInt() == 1;
    
    if (success) {
        UpdateProgress(100, "上传完成！");
        // 延迟一点时间让用户看到完成状态
        wxMilliSleep(500);
        EndModal(wxID_OK);
    } else {
        m_statusText->SetLabel("上传失败！");
        EndModal(wxID_CANCEL);
    }
}

void UploadProgressDialog::OnCancel(wxCommandEvent& event) {
    m_cancelled = true;
    
    if (m_uploadThread && m_uploadThread->IsRunning()) {
        m_uploadThread->Delete();
        // 等待线程结束
        wxMilliSleep(100);
    }
    
    EndModal(wxID_CANCEL);
}

void UploadProgressDialog::OnClose(wxCloseEvent& event) {
    m_cancelled = true;
    
    if (m_uploadThread && m_uploadThread->IsRunning()) {
        m_uploadThread->Delete();
    }

    EndModal(wxID_CANCEL);
}

// 上传线程实现
UploadThread::UploadThread(UploadProgressDialog* dialog, const wxString& filepath)
    : wxThread(wxTHREAD_DETACHED), m_dialog(dialog), m_filepath(filepath) {
}

wxThread::ExitCode UploadThread::Entry() {
    // 获取文件大小
    wxFileName fn(m_filepath);
    wxULongLong totalSize = fn.GetSize();
    
    if (totalSize == wxInvalidSize) {
        // 发送失败事件
        wxCommandEvent event(wxEVT_UPLOAD_COMPLETE);
        event.SetInt(0); // 失败
        wxPostEvent(m_dialog, event);
        return 0;
    }
    
    // 模拟上传过程
    const int chunkSize = 8192; // 8KB chunks
    wxULongLong uploadedSize = 0;
    
    // 这里应该调用实际的 transfer_file 函数
    // 为了演示，我们模拟上传过程
    
    wxFile file(m_filepath, wxFile::read);
    if (!file.IsOpened()) {
        wxCommandEvent event(wxEVT_UPLOAD_COMPLETE);
        event.SetInt(0); // 失败
        wxPostEvent(m_dialog, event);
        return 0;
    }
    
    char buffer[chunkSize];
    
    while (uploadedSize < totalSize && !TestDestroy()) {
        size_t bytesToRead = wxMin(chunkSize, (totalSize - uploadedSize).GetLo());
        size_t bytesRead = file.Read(buffer, bytesToRead);
        
        if (bytesRead == 0) break;
        
        // 这里应该调用 transfer_file 函数
        // transfer_file(buffer, bytesRead);
        
        // 模拟网络传输延迟
        wxMilliSleep(50); // 减少延迟时间，从500改为50
        
        uploadedSize += bytesRead;
        
        // 计算进度百分比
        int percentage = (uploadedSize * 100 / totalSize).GetLo();
        
        // 发送进度更新事件
        wxCommandEvent progressEvent(wxEVT_UPLOAD_PROGRESS);
        progressEvent.SetInt(percentage);
        progressEvent.SetString(wxString::Format("已上传: %s / %s", 
                                               FormatFileSize(uploadedSize),
                                               FormatFileSize(totalSize)));
        wxPostEvent(m_dialog, progressEvent);
    }
    
    file.Close();
    
    // 发送完成事件
    wxCommandEvent completeEvent(wxEVT_UPLOAD_COMPLETE);
    completeEvent.SetInt(TestDestroy() ? 0 : 1); // 成功或被取消
    wxPostEvent(m_dialog, completeEvent);
    
    return 0;
}

wxString UploadThread::FormatFileSize(wxULongLong size) {
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