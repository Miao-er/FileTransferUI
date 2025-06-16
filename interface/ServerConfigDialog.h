#ifndef SERVERCONFIGDIALOG_H
#define SERVERCONFIGDIALOG_H

#include <wx/wx.h>
#include <wx/spinctrl.h>
#include "ServerConfig.h"

/**
 * @brief 服务器配置对话框
 */
class ServerConfigDialog : public wxDialog {
public:
    ServerConfigDialog(wxWindow* parent, const wxString& title = "Server Config");
    ServerConfigDialog(wxWindow* parent, const ServerInfo& server, const wxString& title = "Edit Server Config");
    
    /**
     * @brief 获取配置的服务器信息
     * @return 服务器信息
     */
    ServerInfo GetServerInfo() const;

private:
    wxTextCtrl* m_nameCtrl;
    wxTextCtrl* m_ipCtrl;
    wxSpinCtrl* m_portCtrl;
    
    void InitializeUI();
    void OnOK(wxCommandEvent& event);
    void OnCancel(wxCommandEvent& event);
    bool ValidateInput();
    
    wxDECLARE_EVENT_TABLE();
};

#endif
