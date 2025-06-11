#include <wx/wx.h>
#include "MainFrame.h"

/**
 * @brief 文件上传客户端应用程序
 */
class FileUploadApp : public wxApp {
public:
    virtual bool OnInit() override;
};

bool FileUploadApp::OnInit() {
    // 设置应用程序名称
    SetAppName("FileUploadClient");
    
    // 创建主窗口
    MainFrame* frame = new MainFrame();
    frame->Show(true);
    
    return true;
}

wxIMPLEMENT_APP(FileUploadApp);
