#ifndef UPLOADPROGRESSDIALOG_H
#define UPLOADPROGRESSDIALOG_H
class UploadThread;
#include <wx/wx.h>
#include <wx/gauge.h>
#include <wx/thread.h>
#include <wx/file.h>
#include <chrono>
#include "../net/StreamControl.h"
// 自定义事件声明
wxDECLARE_EVENT(wxEVT_UPLOAD_PROGRESS, wxCommandEvent);
wxDECLARE_EVENT(wxEVT_UPLOAD_COMPLETE, wxCommandEvent);

// 进度信息结构体
struct ProgressInfo {
    double percentage;
    wxULongLong bytesTransferred;
    wxULongLong totalBytes;
    double transferRate; // KB/s
    wxString status;

    ProgressInfo() : percentage(0), bytesTransferred(0), totalBytes(0), transferRate(0.0) {}
};

/**
 * @brief 上传进度对话框
 */
class UploadProgressDialog : public wxDialog {
public:
    UploadProgressDialog(wxWindow* parent, const wxString& filepath, StreamControl *streamControl);
    ~UploadProgressDialog();
    
    /**
     * @brief 开始上传
     * @return 是否成功完成上传
     */
    bool StartUpload();
    void cleanupThread();
private:
    // UI 组件
    wxGauge* m_progressGauge;
    wxStaticText* m_progressText;
    wxStaticText* m_statusText;
    wxStaticText* m_speedText;        // 新增：传输速率显示
    wxStaticText* m_bytesText;        // 新增：字节数显示
    wxStaticText* m_timeText;         // 新增：时间显示
    
    // 数据成员
    wxString m_filepath;
    bool m_cancelled;
    UploadThread* m_uploadThread;
    wxULongLong m_totalFileSize;      // 新增：总文件大小
    StreamControl *m_streamControl;
    // 私有方法
    wxString FormatFileSize(wxULongLong size);
    wxString FormatTransferRate(double rate);
    wxString FormatTime(int seconds);
    void InitializeUI();
    void UpdateProgress(const ProgressInfo& info);
    
    // 事件处理方法
    void OnCancel(wxCommandEvent& event);
    void OnClose(wxCloseEvent& event);
    void OnProgress(wxCommandEvent& event);
    void OnComplete(wxCommandEvent& event);
    friend class UploadThread;
    
    wxDECLARE_EVENT_TABLE();
};

/**
 * @brief 上传线程类
 */
class UploadThread : public wxThread {
public:
    UploadThread(UploadProgressDialog* dialog, const wxString& filepath, StreamControl *streamControl);
    ~UploadThread();
    int caculateTransferInfo(unsigned long bytesTransferred, double duration, unsigned long piece_size);
protected:
    virtual ExitCode Entry() override;
    
private:
    UploadProgressDialog* m_dialog;
    wxString m_filepath;
    wxULongLong m_totalSize;
    std::chrono::steady_clock::time_point m_startTime;
    StreamControl *m_streamControl;

    wxString FormatFileSize(wxULongLong size);
    void SendProgressUpdate(const ProgressInfo& info);
};

#endif
