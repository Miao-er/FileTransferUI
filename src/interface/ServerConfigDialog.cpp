#include "ServerConfigDialog.h"
#include <wx/valtext.h>
#include <wx/msgdlg.h>

wxBEGIN_EVENT_TABLE(ServerConfigDialog, wxDialog)
    EVT_BUTTON(wxID_OK, ServerConfigDialog::OnOK)
    EVT_BUTTON(wxID_CANCEL, ServerConfigDialog::OnCancel)
wxEND_EVENT_TABLE()

ServerConfigDialog::ServerConfigDialog(wxWindow* parent, const wxString& title)
    : wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxSize(400, 300)) {
    InitializeUI();
}

ServerConfigDialog::ServerConfigDialog(wxWindow* parent, const ServerInfo& server, const wxString& title)
    : wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxSize(400, 300)) {
    InitializeUI();
    
    // 填充现有数据
    m_nameCtrl->SetValue(server.name);
    m_ipCtrl->SetValue(server.ip);
    m_portCtrl->SetValue(server.port);
}

void ServerConfigDialog::InitializeUI() {
    wxPanel* panel = new wxPanel(this);
    
    // 创建控件
    wxStaticText* nameLabel = new wxStaticText(panel, wxID_ANY, "name:");
    m_nameCtrl = new wxTextCtrl(panel, wxID_ANY);
    
    wxStaticText* ipLabel = new wxStaticText(panel, wxID_ANY, "IP:");
    m_ipCtrl = new wxTextCtrl(panel, wxID_ANY);
    
    wxStaticText* portLabel = new wxStaticText(panel, wxID_ANY, "port:");
    m_portCtrl = new wxSpinCtrl(panel, wxID_ANY, wxEmptyString, 
                                wxDefaultPosition, wxDefaultSize,
                                wxSP_ARROW_KEYS, 1, 65535, 8080);
    
    // 按钮
    wxButton* okBtn = new wxButton(panel, wxID_OK, "Enter");
    wxButton* cancelBtn = new wxButton(panel, wxID_CANCEL, "Cancel");
    
    // 布局
    wxFlexGridSizer* gridSizer = new wxFlexGridSizer(3, 2, 10, 10);
    gridSizer->AddGrowableCol(1);
    
    gridSizer->Add(nameLabel, 0, wxALIGN_CENTER_VERTICAL);
    gridSizer->Add(m_nameCtrl, 1, wxEXPAND);
    
    gridSizer->Add(ipLabel, 0, wxALIGN_CENTER_VERTICAL);
    gridSizer->Add(m_ipCtrl, 1, wxEXPAND);
    
    gridSizer->Add(portLabel, 0, wxALIGN_CENTER_VERTICAL);
    gridSizer->Add(m_portCtrl, 1, wxEXPAND);
    
    wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    btnSizer->AddStretchSpacer();
    btnSizer->Add(okBtn, 0, wxRIGHT, 5);
    btnSizer->Add(cancelBtn, 0);
    
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    mainSizer->Add(gridSizer, 1, wxEXPAND | wxALL, 20);
    mainSizer->Add(btnSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 20);
    
    panel->SetSizer(mainSizer);
    
    // 设置默认按钮
    okBtn->SetDefault();
    m_nameCtrl->SetFocus();
}

void ServerConfigDialog::OnOK(wxCommandEvent& event) {
    if (ValidateInput()) {
        EndModal(wxID_OK);
    }
}

void ServerConfigDialog::OnCancel(wxCommandEvent& event) {
    EndModal(wxID_CANCEL);
}

bool ServerConfigDialog::ValidateInput() {
    if (m_nameCtrl->GetValue().IsEmpty()) {
        wxMessageBox("Please enter server name", "Input Error", wxOK | wxICON_WARNING);
        m_nameCtrl->SetFocus();
        return false;
    }
    
    if (m_ipCtrl->GetValue().IsEmpty()) {
        wxMessageBox("Please enter IP address", "Input Error", wxOK | wxICON_WARNING);
        m_ipCtrl->SetFocus();
        return false;
    }
    
    // 简单的IP地址格式验证
    wxString ip = m_ipCtrl->GetValue();
    // 这里可以添加更严格的IP地址验证
    
    return true;
}

ServerInfo ServerConfigDialog::GetServerInfo() const {
    return ServerInfo(m_nameCtrl->GetValue(), 
                     m_ipCtrl->GetValue(), 
                     m_portCtrl->GetValue());
}
