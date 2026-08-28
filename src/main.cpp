#include <wx/wx.h>

#include "ui/main_frame.h"

class MainApp final : public wxApp
{
public:
    bool OnInit() override
    {
        auto* frame = new MainFrame();
        frame->Show();
        return true;
    }
};

wxIMPLEMENT_APP(MainApp);
