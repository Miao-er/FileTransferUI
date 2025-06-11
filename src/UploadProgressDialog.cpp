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
    : wxDialog(parent, wxID_ANY, "Upload Progress", 
               wxDefaultPosition, wxSize(400, 200),
               wxDEFAULT_DIALOG_STYLE),
      m_filepath(filepath), m_uploadThread(nullptr), m_cancelled(false) {
    
    InitializeUI();
}

UploadProgressDialog::~UploadProgressDialog() {
    printf("UploadProgressDialog destructor called\n");
    cleanupThread();
}

void UploadProgressDialog::InitializeUI() {
    wxPanel* panel = new wxPanel(this);
    
    // 文件信息
    wxFileName fn(m_filepath);
    wxString filename = fn.GetFullName();
    wxULongLong fileSize = fn.GetSize();
    
    wxStaticText* fileLabel = new wxStaticText(panel, wxID_ANY, "upload file:");
    wxStaticText* fileText = new wxStaticText(panel, wxID_ANY, filename);

    wxStaticText* sizeLabel = new wxStaticText(panel, wxID_ANY, "file size:");
    wxStaticText* sizeText = new wxStaticText(panel, wxID_ANY, FormatFileSize(fileSize));
    
    // 进度条
    m_progressGauge = new wxGauge(panel, wxID_ANY, 100);
    m_progressText = new wxStaticText(panel, wxID_ANY, "0%");
    
    // 状态文本
    m_statusText = new wxStaticText(panel, wxID_ANY, "prepare to upload...");
    
    // 取消按钮
    wxButton* cancelBtn = new wxButton(panel, wxID_CANCEL, "Cancel");

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

    // 修正：让panel填满整个对话框，避免GTK负高度警告
    wxBoxSizer* dlgSizer = new wxBoxSizer(wxVERTICAL);
    dlgSizer->Add(panel, 1, wxEXPAND);
    this->SetSizerAndFit(dlgSizer);
    this->Layout();
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
        printf("Failed to create upload thread\n");
        delete m_uploadThread;
        m_uploadThread = nullptr;
        return false;
    }
    
    if (m_uploadThread->Run() != wxTHREAD_NO_ERROR) {
        printf("Failed to run upload thread\n");
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
        UpdateProgress(100, "Upload Completed!");
        // 延迟一点时间让用户看到完成状态
        wxMilliSleep(500);
        printf("Upload completed successfully\n");
        EndModal(wxID_OK);
    } else {
        m_statusText->SetLabel("Upload failed!");
        printf("Upload failed\n");
        EndModal(wxID_CANCEL);
    }
}

void UploadProgressDialog::cleanupThread() {
    if (m_uploadThread && m_uploadThread->IsRunning()) {
        m_uploadThread->Delete();
        // 等待线程结束
    }
    while (m_uploadThread) {
        wxMilliSleep(100);
    }
}
void UploadProgressDialog::OnCancel(wxCommandEvent& event) {
    m_cancelled = true;
    printf("Upload cancelled by user\n");
    EndModal(wxID_CANCEL);
}

void UploadProgressDialog::OnClose(wxCloseEvent& event) {
    m_cancelled = true;
    printf("Upload dialog closed by user\n"); 
    EndModal(wxID_CANCEL);
}

// 上传线程实现
UploadThread::UploadThread(UploadProgressDialog* dialog, const wxString& filepath)
    : wxThread(wxTHREAD_DETACHED), m_dialog(dialog), m_filepath(filepath) {
}

UploadThread::~UploadThread() {
    m_dialog->m_uploadThread = nullptr; // 清理对话框中的线程引用
    printf("UploadThread destructor called\n");
}
wxThread::ExitCode UploadThread::Entry() {
    wxFileName fn(m_filepath);
    wxULongLong totalSize = fn.GetSize();
    printf("Starting upload for file: %s, size: %s\n", 
           fn.GetFullName().ToStdString().c_str(), FormatFileSize(totalSize).ToStdString().c_str());
    if (totalSize == wxInvalidSize) {
        wxCommandEvent evt(wxEVT_UPLOAD_COMPLETE);
        evt.SetInt(0); // 失败
        wxQueueEvent(m_dialog, evt.Clone());
        return (wxThread::ExitCode)0;
    }
    int percent = 0;
    for (percent = 1; percent <= 100; ++percent) {
        if (TestDestroy() || m_dialog->m_cancelled) break;
        // printf("Uploading... %d%%\n", percent);
        wxMilliSleep(20);
        wxCommandEvent evt(wxEVT_UPLOAD_PROGRESS);
        evt.SetInt(percent);
        evt.SetString(wxString::Format("Uploading... %d%%", percent));
        wxQueueEvent(m_dialog, evt.Clone());
    }
    printf("Upload progress: %d%%\n", percent);
    wxCommandEvent evt(wxEVT_UPLOAD_COMPLETE);
    printf("Upload complete with status: %d%%\n", percent);
    evt.SetInt(percent == 101? 1 : 0); // 成功
    printf("Upload thread completed with status: %d\n", evt.GetInt());
    wxQueueEvent(m_dialog, evt.Clone());
    printf("Exiting upload thread\n");
    return (wxThread::ExitCode)0;
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