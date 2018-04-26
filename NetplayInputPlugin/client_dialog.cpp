#include <cstdint>
#include <boost/lexical_cast.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <boost/tokenizer.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <windows.h>
#include <Windowsx.h>
#include <Richedit.h>
#include <Uxtheme.h>

#include "util.h"
#include "client_dialog.h"
#include "resource.h"

#define WM_TASK 0x8000

using namespace std;

client_dialog::client_dialog(HMODULE hmod, HWND main_window)
    : hmod(hmod), main_window(main_window), h_rich(LoadLibrary(L"Riched20.dll")), hwndDlg(NULL), thread([=] { gui_thread(); }) {
    initialized.get_future().get();
}

client_dialog::~client_dialog() {
    SendMessage(hwndDlg, WM_COMMAND, IDC_DESTROY_BUTTON, 0);

    thread.join();

    if (h_rich != NULL) {
        FreeLibrary(h_rich);
    }
}

void client_dialog::set_command_handler(function<void(string)> command_handler) {
    this->command_handler = command_handler;
}

void client_dialog::status(const string& text) {
    PostMessage(hwndDlg, WM_TASK, (WPARAM) new function<void(void)>([=] {
        HWND output_box = GetDlgItem(hwndDlg, IDC_OUTPUT_RICHEDIT);
        SendMessage(output_box, WM_SETREDRAW, FALSE, NULL);

        bool at_bottom = scroll_at_bottom();

        append_timestamp();

        CHARFORMAT2 format;
        format.cbSize = sizeof(format);
        format.dwMask = CFM_COLOR | CFM_BOLD | CFM_SIZE;
        format.crTextColor = RGB(0, 0, 255);
        format.yHeight = 10 * 20;
        format.dwEffects = CFE_BOLD;
        SendMessage(output_box, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&format);
        insert_text(text);

        if (at_bottom) {
            scroll_to_bottom();
        }

        alert_user(false);

        SendMessage(output_box, WM_SETREDRAW, TRUE, NULL);
        RedrawWindow(output_box, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
    }), NULL);
}

void client_dialog::error(const string& text) {
    PostMessage(hwndDlg, WM_TASK, (WPARAM) new function<void(void)>([=] {
        HWND output_box = GetDlgItem(hwndDlg, IDC_OUTPUT_RICHEDIT);
        SendMessage(output_box, WM_SETREDRAW, FALSE, NULL);

        bool at_bottom = scroll_at_bottom();

        append_timestamp();

        CHARFORMAT2 format;
        format.cbSize = sizeof(format);
        format.dwMask = CFM_COLOR | CFM_BOLD | CFM_SIZE;
        format.crTextColor = RGB(255, 0, 0);
        format.yHeight = 10*20;
        format.dwEffects = CFE_BOLD;
        SendMessage(output_box, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM) &format);
        insert_text(text);

        if (at_bottom) {
            scroll_to_bottom();
        }

        alert_user(true);

        SendMessage(output_box, WM_SETREDRAW, TRUE, NULL);
        RedrawWindow(output_box, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
    }), NULL);
}

void client_dialog::chat(const string& name, const string& message) {
    PostMessage(hwndDlg, WM_TASK, (WPARAM) new function<void(void)>([=] {
        HWND output_box = GetDlgItem(hwndDlg, IDC_OUTPUT_RICHEDIT);
        SendMessage(output_box, WM_SETREDRAW, FALSE, NULL);

        bool at_bottom = scroll_at_bottom();

        append_timestamp();

        CHARFORMAT2 format;
        format.cbSize = sizeof(format);
        format.dwMask = CFM_COLOR | CFM_BOLD | CFM_SIZE;
        format.crTextColor = RGB(0, 0, 0);
        format.yHeight = 10 * 20;
        format.dwEffects = CFE_BOLD;
        SendMessage(output_box, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&format);
        insert_text(name + ":");

        format.cbSize = sizeof(format);
        format.dwMask = CFM_COLOR | CFM_BOLD | CFM_SIZE;
        format.crTextColor = RGB(0, 0, 0);
        format.yHeight = 10 * 20;
        format.dwEffects = !CFE_BOLD;
        SendMessage(output_box, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&format);
        insert_text(" " + message);

        if (at_bottom) {
            scroll_to_bottom();
        }

        alert_user(false);

        SendMessage(output_box, WM_SETREDRAW, TRUE, NULL);
        RedrawWindow(output_box, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
    }), NULL);
}

void client_dialog::update_user_list(const map<uint32_t, string>& names, const map<uint32_t, uint32_t>& pings) {
    PostMessage(hwndDlg, WM_TASK, (WPARAM) new function<void(void)>([=] {
        HWND list_box = GetDlgItem(hwndDlg, IDC_USER_LIST);

        SendMessage(list_box, WM_SETREDRAW, FALSE, NULL);

        int selection = ListBox_GetCurSel(list_box);

        ListBox_ResetContent(list_box);
        for (auto it = names.begin(); it != names.end(); ++it) {
            string entry = it->second;
            auto ping = pings.find(it->first);
            if (ping != pings.end()) {
                entry += " (" + boost::lexical_cast<string>(ping->second) + " ms)";
            }
            ListBox_InsertString(list_box, -1, utf8_to_wstring(entry).c_str());
        }

        ListBox_SetCurSel(list_box, selection);

        SendMessage(list_box, WM_SETREDRAW, TRUE, NULL);
        RedrawWindow(list_box, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
    }), NULL);
}

void client_dialog::gui_thread() {
    HINSTANCE user32 = GetModuleHandle(L"User32.dll");
    if (user32) {
        auto SetThreadDpiAwarenessContext = (int(__stdcall *)(int)) GetProcAddress(user32, "SetThreadDpiAwarenessContext");
        if (SetThreadDpiAwarenessContext) {
            SetThreadDpiAwarenessContext(-2);
        }
    }

    hwndDlg = CreateDialogParam(hmod, MAKEINTRESOURCE(IDD_NETPLAY_DIALOG), NULL, &DialogProc, (LPARAM) this);

    RECT main_rect, my_rect;
    GetWindowRect(main_window, &main_rect);
    GetWindowRect(hwndDlg, &my_rect);
    SetWindowPos(
        hwndDlg,
        HWND_TOP,
        (main_rect.left + main_rect.right) / 2 - (my_rect.right - my_rect.left) / 2,
        (main_rect.top + main_rect.bottom) / 2 - (my_rect.bottom - my_rect.top) / 2,
        my_rect.right - my_rect.left,
        my_rect.bottom - my_rect.top,
        0
    );

    initialized.set_value(true);

    MSG message;
    while (GetMessage(&message, NULL, 0, 0) > 0) {
        if (!IsDialogMessage(hwndDlg, &message)) {
            TranslateMessage(&message);
            DispatchMessage(&message);
        }
    }
}

bool client_dialog::scroll_at_bottom() {
    SCROLLINFO psi;
    psi.cbSize = sizeof(psi);
    psi.fMask = SIF_ALL;
    if (GetScrollInfo(GetDlgItem(hwndDlg, IDC_OUTPUT_RICHEDIT), SB_VERT, &psi) && psi.nMax - (int)psi.nPage > psi.nTrackPos) {
        return false;
    }

    return true;
}

void client_dialog::scroll_to_bottom() {
    SendMessage(GetDlgItem(hwndDlg, IDC_OUTPUT_RICHEDIT), WM_VSCROLL, MAKELONG(SB_BOTTOM, 0), 0);
}

void client_dialog::select_end() {
    CHARRANGE cr;
    cr.cpMin = -1;
    cr.cpMax = -1;
    SendMessage(GetDlgItem(hwndDlg, IDC_OUTPUT_RICHEDIT), EM_EXSETSEL, 0, (LPARAM) &cr);
}

void client_dialog::insert_text(const string& text) {
    SendMessage(GetDlgItem(hwndDlg, IDC_OUTPUT_RICHEDIT), EM_REPLACESEL, 0, (LPARAM)utf8_to_wstring(text).c_str());
}

void client_dialog::append_timestamp() {
    using namespace boost::gregorian;
    using namespace boost::posix_time;

    string time = to_simple_string(second_clock::local_time());
    time = time.substr(12);
    string wtime;
    wtime.assign(time.begin(), time.end());

    select_end();

    CHARFORMAT2 format;
    format.cbSize = sizeof(format);
    format.dwMask = CFM_COLOR | CFM_BOLD | CFM_SIZE;
    format.crTextColor = RGB(128, 128, 128);
    format.yHeight = 10*20;
    format.dwEffects = !CFE_BOLD;
    SendMessage(GetDlgItem(hwndDlg, IDC_OUTPUT_RICHEDIT), EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM) &format);

    if (SendMessage(GetDlgItem(hwndDlg, IDC_OUTPUT_RICHEDIT), WM_GETTEXTLENGTH, 0, 0) == 0) {
        insert_text("(" + wtime + ") ");
    } else {
        insert_text("\n(" + wtime + ") ");
    }
}

void client_dialog::alert_user(bool force) {
    WINDOWPLACEMENT wp;
    if (!force) {
        wp.length = sizeof(wp);
        GetWindowPlacement(hwndDlg, &wp);
    }

    if (force || wp.showCmd == SW_SHOWMINIMIZED) {
        FLASHWINFO fwi;
        fwi.cbSize = sizeof(fwi);
        fwi.hwnd = hwndDlg;
        fwi.dwFlags = FLASHW_TIMERNOFG | FLASHW_TRAY;
        fwi.uCount = (UINT) -1;
        fwi.dwTimeout = 0;
        FlashWindowEx(&fwi);
    }
}

INT_PTR CALLBACK client_dialog::DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_INITDIALOG:
            SetProp(hwndDlg, L"client_dialog", (void*) lParam);
            SetWindowTheme(GetDlgItem(hwndDlg, IDC_USER_LIST), L"", L"");
            return TRUE;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        case WM_COMMAND:
            switch (wParam) {
                case IDC_DESTROY_BUTTON:
                    DestroyWindow(hwndDlg);
                    return TRUE;

                case IDCANCEL:
                    ShowWindow(hwndDlg, SW_MINIMIZE);
                    return TRUE;

                case IDOK:
                    if (GetFocus() == GetDlgItem(hwndDlg, IDC_INPUT_RICHEDIT)) {
                        wchar_t buffer[1024];
                        GETTEXTEX gte;
                        gte.cb = sizeof(buffer);
                        gte.flags = GT_DEFAULT;
                        gte.codepage = 1200; // Unicode
                        gte.lpDefaultChar = NULL;
                        gte.lpUsedDefChar = NULL;

                        if (SendMessage(GetDlgItem(hwndDlg, IDC_INPUT_RICHEDIT), EM_GETTEXTEX, (WPARAM) &gte, (LPARAM) buffer)) {
                            auto dialog = (client_dialog*) GetProp(hwndDlg, L"client_dialog");

                            if (dialog && dialog->command_handler) {
                                dialog->command_handler(wstring_to_utf8(buffer));

                                SETTEXTEX ste;
                                ste.flags = ST_DEFAULT;
                                ste.codepage = 1200; // Unicode
                                SendMessage(GetDlgItem(hwndDlg, IDC_INPUT_RICHEDIT), EM_SETTEXTEX, (WPARAM)&ste, (LPARAM)L"");
                            }
                        }
                    }
                    return TRUE;
            }
            break;

        case WM_TASK:
            auto task = (function <void(void)>*) wParam;
            (*task)();
            delete task;
            return TRUE;
    }

    return FALSE;
}
