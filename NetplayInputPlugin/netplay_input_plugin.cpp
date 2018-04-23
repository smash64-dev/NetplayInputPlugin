#include "Controller 1.0.h"
#include "id_variable.h"
#include "plugin_dialog.h"
#include "settings.h"
#include "input_plugin.h"
#include "client.h"

#include <string>
#include <cassert>

using namespace std;

#if defined(__cplusplus)
extern "C" {
#endif

static bool loaded = false;
static bool rom_open = false;
static HMODULE this_dll = NULL;
static HWND main_window = NULL;
static CONTROL* netplay_controllers;
static shared_ptr<settings> my_settings;
static shared_ptr<input_plugin> my_plugin;
static shared_ptr<client> my_client;
static wstring my_location;
static bool control_visited[MAX_PLAYERS];

void set_visited(bool b) {
    for (int i = 0; i < 4; i++) {
        control_visited[i] = b;
    }
}

BOOL WINAPI DllMain(HMODULE hinstDLL, DWORD dwReason, LPVOID lpvReserved) {
    switch (dwReason) {
        case DLL_PROCESS_ATTACH:
            this_dll = hinstDLL;
            break;

        case DLL_PROCESS_DETACH:
            break;
        
        case DLL_THREAD_ATTACH:
            break;
        
        case DLL_THREAD_DETACH:
            break;
    }

    return TRUE;
}

void load() {
    if (loaded) {
        return;
    }

    SetErrorMode(SEM_FAILCRITICALERRORS);

    wchar_t my_location_array[MAX_PATH];
    GetModuleFileName(this_dll, my_location_array, MAX_PATH);
    wcsrchr(my_location_array, L'\\')[1] = 0;
    my_location = my_location_array;

    my_settings = shared_ptr<settings>(new settings(my_location + L"netplay_input_plugin.ini"));

    try {
        my_plugin = shared_ptr<input_plugin>(new input_plugin(my_location + my_settings->get_plugin_dll()));
    } catch(const exception&) {
        my_plugin = shared_ptr<input_plugin>();
    }

    loaded = true;
}

EXPORT int _IDENTIFYING_VARIABLE = 0;

EXPORT void CALL CloseDLL (void) {
    if (my_client != NULL) {
        my_settings->set_name(my_client->get_name());
    }

    my_settings->save();

    my_client.reset();
    my_settings.reset();
    my_plugin.reset();

    loaded = false;
}

EXPORT void CALL ControllerCommand( int Control, BYTE * Command) {
    load();
}

EXPORT void CALL DllAbout ( HWND hParent ) {
    load();

    MessageBox(hParent, L"Netplay Input Plugin\n\nVersion: 0.23\n\nAuthor: @CoderTimZ (aka AQZ)\n\nWebsite: www.play64.com", L"About", MB_OK | MB_ICONINFORMATION);
}

EXPORT void CALL DllConfig ( HWND hParent ) {
    load();

    if (rom_open) {
        if (my_plugin != NULL) {
            my_plugin->DllConfig(hParent);

            if (my_client != NULL) {
                my_client->set_local_controllers(my_plugin->controls);
            }
        }
    } else {
        my_plugin = shared_ptr<input_plugin>();

        assert(main_window != NULL);

        plugin_dialog dialog(this_dll, hParent, my_location, my_settings->get_plugin_dll(), main_window);

        if (dialog.ok_clicked()) {
            my_settings->set_plugin_dll(dialog.get_plugin_dll());
        }

        try {
            my_plugin = shared_ptr<input_plugin>(new input_plugin(my_location + my_settings->get_plugin_dll()));
            my_plugin->InitiateControllers0100(main_window, my_plugin->controls);
        }
        catch(const exception&){}
    }
}

EXPORT void CALL DllTest ( HWND hParent ) {
    load();

    if (my_plugin == NULL) {
        MessageBox(NULL, L"No input plugin has been selected.", L"Warning", MB_OK | MB_ICONWARNING);
    }
}

EXPORT void CALL GetDllInfo ( PLUGIN_INFO * PluginInfo ) {
    load();

    PluginInfo->Version = 0x0100;
    PluginInfo->Type = PLUGIN_TYPE_CONTROLLER;

    strncpy_s(PluginInfo->Name, PLUGIN_NAME_LENGTH, "AQZ Netplay v0.23", PLUGIN_NAME_LENGTH);
}

EXPORT void CALL GetKeys(int Control, BUTTONS * Keys ) {
    static vector<BUTTONS> input(4);

    Keys->Value = 0;

    load();

    if (control_visited[Control]) {
        set_visited(false);

        if (my_plugin == NULL || my_client == NULL) {
            for (int i = 0; i < MAX_PLAYERS; i++) {
                input[i].Value = 0;
            }
        } else {
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (my_client->plugged_in(i)) {
                    my_plugin->GetKeys(i, &input[i]);
                } else {
                    input[i].Value = 0;
                }
            }

            my_client->process_input(input);
        }
    }

    *Keys = input[Control];
    control_visited[Control] = true;
}

EXPORT void CALL InitiateControllers (HWND hMainWindow, CONTROL Controls[4]) {
    load();

    netplay_controllers = Controls;

    if (my_plugin != NULL) {
        my_plugin->InitiateControllers0100(hMainWindow, my_plugin->controls);
    }

    main_window = hMainWindow;
}

EXPORT void CALL ReadController ( int Control, BYTE * Command ) {
    load();
}

EXPORT void CALL RomClosed (void) {
    load();

    if (my_client != NULL) {
        my_settings->set_name(my_client->get_name());
    }

    my_client.reset();

    if (my_plugin != NULL) {
        my_plugin->RomClosed();
    }

    rom_open = false;
}

EXPORT void CALL RomOpen (void) {
    load();

    assert(main_window != NULL);

    rom_open = true;

    if (my_plugin == NULL) {
        DllConfig(main_window);
    }

    if (my_plugin != NULL) {
        my_client = make_shared<client>(new client_dialog(this_dll, main_window));
        my_client->set_name(my_settings->get_name());
        my_client->set_netplay_controllers(netplay_controllers);

        my_plugin->RomOpen();
        my_client->set_local_controllers(my_plugin->controls);

        my_client->wait_for_game_to_start();
    }

    set_visited(true);
}

EXPORT void CALL WM_KeyDown( WPARAM wParam, LPARAM lParam ) {
    load();

    if (my_plugin != NULL) {
        my_plugin->WM_KeyDown(wParam, lParam);
    }
}

EXPORT void CALL WM_KeyUp( WPARAM wParam, LPARAM lParam ) {
    load();

    if (my_plugin != NULL) {
        my_plugin->WM_KeyUp(wParam, lParam);
    }
}

#if defined(__cplusplus)
}
#endif
