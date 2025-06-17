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
               wxDefaultPosition, wxSize(280, 210),
               wxCAPTION | wxSYSTEM_MENU), // 固定大小样式
      m_filepath(filepath), m_uploadThread(nullptr), m_cancelled(false) {
    
    // 获取文件大小
    wxFileName fn(m_filepath);
    m_totalFileSize = fn.GetSize();
    
    InitializeUI();
    
    // 设置固定大小
    SetMinSize(wxSize(280, 210));
    SetMaxSize(wxSize(280, 210));
    
    // 居中显示
    CenterOnParent();
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
    
    wxStaticText* fileLabel = new wxStaticText(panel, wxID_ANY, "File:");
    
    // 限制文件名显示长度，避免对话框变宽
    wxStaticText* fileText = new wxStaticText(panel, wxID_ANY, filename);
    fileText->SetMaxSize(wxSize(260, -1)); // 限制最大宽度
    fileText->Wrap(260); // 自动换行

    wxStaticText* sizeLabel = new wxStaticText(panel, wxID_ANY, "Size:");
    wxStaticText* sizeText = new wxStaticText(panel, wxID_ANY, FormatFileSize(m_totalFileSize));
    
    // 进度条
    m_progressGauge = new wxGauge(panel, wxID_ANY, 100);
    m_progressGauge->SetMinSize(wxSize(260, 20)); // 固定进度条宽度
    m_progressGauge->SetMaxSize(wxSize(260, 20)); // 固定进度条高度
    m_progressText = new wxStaticText(panel, wxID_ANY, " 0%");
    
    // 状态信息和字节数信息
    m_statusText = new wxStaticText(panel, wxID_ANY, _T("准备上传..."));
    m_bytesText = new wxStaticText(panel, wxID_ANY, _T("0000.0 KB/") + FormatFileSize(m_totalFileSize), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
    m_speedText = new wxStaticText(panel, wxID_ANY, _T("    0.0 KB/s"));
    m_timeText = new wxStaticText(panel, wxID_ANY, _T("00:00"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
    
    // 取消按钮
    wxButton* cancelBtn = new wxButton(panel, wxID_CANCEL, "Cancel");

    // 布局
    wxFlexGridSizer* infoSizer = new wxFlexGridSizer(2, 2, 5, 10);
    infoSizer->AddGrowableCol(1);
    infoSizer->Add(fileLabel, 0, wxALIGN_CENTER_VERTICAL);
    infoSizer->Add(fileText, 1, wxEXPAND);
    infoSizer->Add(sizeLabel, 0, wxALIGN_CENTER_VERTICAL);
    infoSizer->Add(sizeText, 1, wxEXPAND);
    
    // 进度条布局
    wxBoxSizer* progressSizer = new wxBoxSizer(wxHORIZONTAL);
    progressSizer->Add(m_progressGauge, 1, wxALIGN_CENTER_VERTICAL);
    progressSizer->Add(m_progressText, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 10);
    
    // 修改：状态和字节数放在同一行，分别左对齐和右对齐
    wxBoxSizer* statusBytesSizer = new wxBoxSizer(wxHORIZONTAL);
    statusBytesSizer->Add(m_statusText, 0, wxEXPAND);
    statusBytesSizer->AddStretchSpacer();  // 添加弹性空间，将两个文本分开
    statusBytesSizer->Add(m_bytesText, 0, wxEXPAND | wxALIGN_RIGHT);
    // wxFlexGridSizer* statusBytesSizer = new wxFlexGridSizer(1, 2, 5, 10);
    // statusBytesSizer->AddGrowableCol(0, 1);  // 第一列可增长，比例为1
    // statusBytesSizer->AddGrowableCol(1, 1);  // 第二列可增长，比例为1
    // statusBytesSizer->Add(m_statusText, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);  // proportion设为1
    // statusBytesSizer->Add(m_bytesText, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);   // proportion设为1
    
    // 速率和剩余时间放在同一行
    wxBoxSizer* speedTimeSizer = new wxBoxSizer(wxHORIZONTAL);
    speedTimeSizer->Add(m_speedText, 0, wxEXPAND);
    speedTimeSizer->AddStretchSpacer();  // 添加弹性空间，将两个文本分开
    speedTimeSizer->Add(m_timeText, 0, wxEXPAND | wxALIGN_RIGHT);

    // wxFlexGridSizer* speedTimeSizer = new wxFlexGridSizer(1, 2, 5, 10);
    // speedTimeSizer->AddGrowableCol(0, 1);
    // speedTimeSizer->AddGrowableCol(1, 1);

    // speedTimeSizer->Add(m_speedText, 1,  wxALIGN_CENTER_VERTICAL);
    // speedTimeSizer->Add(m_timeText, 1,  wxALIGN_CENTER_VERTICAL);
    
    
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    mainSizer->Add(infoSizer, 0, wxEXPAND | wxALL, 10);
    mainSizer->Add(statusBytesSizer, 0,wxEXPAND | wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 5);  // 状态和字节数在同一行
    mainSizer->Add(progressSizer, 0, wxEXPAND | wxLEFT | wxRIGHT, 5);
    mainSizer->Add(speedTimeSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 5);
    mainSizer->Add(cancelBtn, 0, wxALIGN_CENTER | wxALL, 5);
    
    panel->SetSizer(mainSizer);

    // 让panel填满整个对话框，但不自动调整对话框大小
    wxBoxSizer* dlgSizer = new wxBoxSizer(wxVERTICAL);
    dlgSizer->Add(panel, 1, wxEXPAND);
    this->SetSizer(dlgSizer);
    // this->SetSizerAndFit(dlgSizer);
    this->Layout();
}
// 
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

wxString UploadProgressDialog::FormatTransferRate(double rate) {
    if (rate < 1024) {
        return wxString::Format("%.1f B/s", rate);
    } else if (rate < 1024 * 1024) {
        return wxString::Format("%.1f KB/s", rate / 1024);
    } else {
        return wxString::Format("%.1f MB/s", rate / (1024 * 1024));
    }
}

wxString UploadProgressDialog::FormatTime(int seconds) {
    if (seconds < 0) return "--:--";
    
    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    int secs = seconds % 60;
    
    if (hours > 0) {
        return wxString::Format("%d:%02d:%02d", hours, minutes, secs);
    } else {
        return wxString::Format("%02d:%02d", minutes, secs);
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
    
    return true;
}

void UploadProgressDialog::UpdateProgress(const ProgressInfo& info) {
    m_progressGauge->SetValue(info.percentage);
    m_progressText->SetLabel(wxString::Format("%d%%", info.percentage));
    m_statusText->SetLabel(info.status);
    
    // 更新字节数显示
    m_bytesText->SetLabel(wxString::Format("%s/%s", 
        FormatFileSize(info.bytesTransferred), 
        FormatFileSize(info.totalBytes)));
    
    // 更新传输速率
    m_speedText->SetLabel(FormatTransferRate(info.transferRate));

    // 计算剩余时间
    if (info.transferRate > 0 && info.percentage > 0) {
        wxULongLong remainingBytes = info.totalBytes - info.bytesTransferred;
        int remainingSeconds = static_cast<int>(remainingBytes.ToDouble() / info.transferRate);
        m_timeText->SetLabel(FormatTime(remainingSeconds));
    } else {
        m_timeText->SetLabel(FormatTime(-1));
    }
}

void UploadProgressDialog::OnProgress(wxCommandEvent& event) {
    // 从事件中获取进度信息
    ProgressInfo* info = reinterpret_cast<ProgressInfo*>(event.GetClientData());
    if (info) {
        UpdateProgress(*info);
        delete info; // 清理内存
    }
}

void UploadProgressDialog::OnComplete(wxCommandEvent& event) {
    bool success = event.GetInt() == 1;
    
    if (success) {
        ProgressInfo finalInfo;
        finalInfo.percentage = 100;
        finalInfo.bytesTransferred = m_totalFileSize;
        finalInfo.totalBytes = m_totalFileSize;
        finalInfo.status = _T("上传完成!");
        finalInfo.transferRate = 0;
        
        UpdateProgress(finalInfo);
        m_timeText->SetLabel(_T("00:00"));
        
        // 延迟一点时间让用户看到完成状态
        wxMilliSleep(500);
        printf("Upload completed successfully\n");
        EndModal(wxID_OK);
    } else {
        m_statusText->SetLabel(_T("上传失败!"));
        printf("Upload failed\n");
        //EndModal(wxID_CANCEL);
    }
}

void UploadProgressDialog::cleanupThread() {
    if (m_uploadThread && m_uploadThread->IsRunning()) {
        m_uploadThread->Delete();
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
    
    wxFileName fn(m_filepath);
    m_totalSize = fn.GetSize();
}

UploadThread::~UploadThread() {
    m_dialog->m_uploadThread = nullptr;
    printf("UploadThread destructor called\n");
}

void UploadThread::SendProgressUpdate(const ProgressInfo& info) {
    wxCommandEvent evt(wxEVT_UPLOAD_PROGRESS);
    ProgressInfo* infoPtr = new ProgressInfo(info);
    evt.SetClientData(infoPtr);
    wxQueueEvent(m_dialog, evt.Clone());
}

wxThread::ExitCode UploadThread::Entry() {
    printf("Starting upload for file: %s, size: %s\n", 
           wxFileName(m_filepath).GetFullName().ToStdString().c_str(), 
           FormatFileSize(m_totalSize).ToStdString().c_str());
    
    if (m_totalSize == wxInvalidSize) {
        wxCommandEvent evt(wxEVT_UPLOAD_COMPLETE);
        evt.SetInt(0); // 失败
        wxQueueEvent(m_dialog, evt.Clone());
        return (wxThread::ExitCode)0;
    }
    
    // 记录开始时间
    m_startTime = std::chrono::steady_clock::now();
    
    // 模拟文件传输过程
    const int totalSteps = 100;
    const int stepDelay = 50; // 毫秒
    int step;
    for (step = 1; step <= totalSteps; ++step) {
        if (TestDestroy() || m_dialog->m_cancelled) break;
        
        // 计算当前进度
        int percentage = step;
        wxULongLong bytesTransferred = (m_totalSize * step) / totalSteps;
        
        // 计算传输速率
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - m_startTime);
        double elapsedSeconds = elapsed.count() / 1000.0;
        
        double transferRate = 0.0;
        if (elapsedSeconds > 0) {
            transferRate = bytesTransferred.ToDouble() / elapsedSeconds; // bytes per second
        }
        
        // 创建进度信息
        ProgressInfo info;
        info.percentage = percentage;
        info.bytesTransferred = bytesTransferred;
        info.totalBytes = m_totalSize;
        info.transferRate = transferRate;
        info.status = wxString::Format(_T("正在上传..."));
        
        // 发送进度更新
        SendProgressUpdate(info);
        
        wxMilliSleep(stepDelay);
    }
    
    // 发送完成事件
    wxCommandEvent evt(wxEVT_UPLOAD_COMPLETE);
    evt.SetInt(step > totalSteps? 1:0); // 成功
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