#include <wx/wx.h>
#include <wx/socket.h>

#include "ui/main_frame.h"

class MainApp final : public wxApp
{
public:
    bool OnInit() override
    {
        // wxSocketBase::Initialize() must run on the main thread. The API
        // service creates blocking sockets from its worker thread, but those
        // sockets still use the socket manager initialized here.
        if (!wxSocketBase::Initialize())
            return false;
        socketsInitialized_ = true;

        auto* frame = new MainFrame();
        frame->Show();
        return true;
    }

    int OnExit() override
    {
        if (socketsInitialized_)
            wxSocketBase::Shutdown();
        return 0;
    }

private:
    bool socketsInitialized_ = false;
};

wxIMPLEMENT_APP(MainApp);
