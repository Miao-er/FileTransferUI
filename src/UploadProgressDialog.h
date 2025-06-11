#ifndef UPLOADPROGRESSDIALOG_H
#define UPLOADPROGRESSDIALOG_H

#include <wx/wx.h>
#include <wx/gauge.h>
#include <wx/thread.h>
#include <wx/file.h>

// 前向声明
class UploadThread;

// 自定义事件声明
wxDECLARE_EVENT(wxEVT_UPLOAD_PROGRESS, wxCommandEvent);
wxDECLARE_EVENT(wxEVT_UPLOAD_COMPLETE, wxCommandEvent);

/**
 * @brief 上传进度对话框
 */
class UploadProgressDialog : public wxDialog {
public:
    UploadProgressDialog(wxWindow* parent, const wxString& filepath);
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
    
    // 数据成员
    wxString m_filepath;
    bool m_cancelled;
    UploadThread* m_uploadThread;
    
    // 私有方法 - 将 FormatFileSize 放在前面
    wxString FormatFileSize(wxULongLong size);
    void InitializeUI();
    void UpdateProgress(int percentage, const wxString& status);
    
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
    UploadThread(UploadProgressDialog* dialog, const wxString& filepath);
    ~UploadThread();
    
protected:
    virtual ExitCode Entry() override;
    
private:
    UploadProgressDialog* m_dialog;
    wxString m_filepath;
    wxString FormatFileSize(wxULongLong size);
};

#endif
