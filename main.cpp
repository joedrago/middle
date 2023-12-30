#include "targetver.h"

#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <shellapi.h>
// #include <sstream>
// #include <stdbool.h>
// #include <stdio.h>
#include <string>
#include <vector>

#include "Middle.h"
#include "helpers.h"
#include "resource.h"

#define WM_SHELLICONCLICKED (WM_USER + 1)

// --------------------------------------------------------------------------------------
// Globals! Yuck!

HMENU contextMenu_;

Middle * middle_ = nullptr;

// Determined in loadConfig() (including defaults)
static int offsetX_;
static int offsetY_;
static int width_;
static int height_;

// --------------------------------------------------------------------------------------
// Hotkey nonsense

static bool addHotKey(HWND hWnd, int id, const std::string & keyText)
{
    unsigned int mods = 0;

    unsigned int key = 0;
    std::vector<std::string> pieces;
    split(keyText, '+', pieces);
    if (pieces.empty()) {
        return fatalError("action has an empty key string");
    }

    for (std::vector<std::string>::iterator it = pieces.begin(); it != pieces.end(); ++it) {
        if (*it == "win") {
            mods |= MOD_WIN;
        } else if (*it == "alt") {
            mods |= MOD_ALT;
        } else if ((*it == "ctrl") || (*it == "ctl") || (*it == "control")) {
            mods |= MOD_CONTROL;
        } else if (*it == "shift") {
            mods |= MOD_SHIFT;
        } else if (*it == "up") {
            key = VK_UP;
        } else if (*it == "down") {
            key = VK_DOWN;
        } else if (*it == "left") {
            key = VK_LEFT;
        } else if (*it == "space") {
            key = VK_SPACE;
        } else if (*it == "right") {
            key = VK_RIGHT;
        } else {
            if (it->size() != 1) {
                return fatalError("Unknown key element: %s", it->c_str());
            }
            key = (unsigned int)((*it)[0]);
        }
    }

    if (key == 0) {
        return fatalError("Did not provide an actual key to press: ", keyText.c_str());
    }

    if (!RegisterHotKey(hWnd, id, mods | MOD_NOREPEAT, key)) {
        return fatalError("Failed to register hotkey: %s", keyText.c_str());
    }
    return true;
}

static bool registerHotKeys(HWND hWnd)
{
    if (!addHotKey(hWnd, 1, middle_->hotkey().c_str())) {
        return false;
    }
    return true;
}

// --------------------------------------------------------------------------------------
// Shell icon nonsense

static void createShellIcon(HWND hDlg)
{
    NOTIFYICONDATA nid;

    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hDlg;
    nid.uID = 100;
    nid.uVersion = NOTIFYICON_VERSION;
    nid.uCallbackMessage = WM_SHELLICONCLICKED;
    nid.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MIDDLE));
    strcpy(nid.szTip, "Middle");
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;

    Shell_NotifyIcon(NIM_ADD, &nid);
}

static void destroyShellIcon(HWND hDlg)
{
    NOTIFYICONDATA nid;
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hDlg;
    nid.uID = 100;
    Shell_NotifyIcon(NIM_DELETE, &nid);
}

// --------------------------------------------------------------------------------------
// Windows nonsense

static INT_PTR CALLBACK Proc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
        case WM_INITDIALOG:
            SetTimer(hDlg, 5, 5000, NULL);
            createShellIcon(hDlg);
            registerHotKeys(hDlg);
            return (INT_PTR)TRUE;

        case WM_SHELLICONCLICKED:
            switch (lParam) {
                case WM_LBUTTONDOWN:
                case WM_RBUTTONDOWN:
                    POINT p;
                    GetCursorPos(&p);
                    TrackPopupMenu(GetSubMenu(contextMenu_, 0), TPM_TOPALIGN | TPM_LEFTALIGN, p.x, p.y, 0, hDlg, nullptr);
                    break;
            }
            return (INT_PTR)TRUE;

        case WM_TIMER:
            if (middle_) {
                middle_->onTick();
            }
            return (INT_PTR)TRUE;

        case WM_HOTKEY:
            if (middle_) {
                middle_->onHotkey();
            }
            return (INT_PTR)TRUE;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_QUIT) {
                destroyShellIcon(hDlg);
                PostQuitMessage(0);
                return (INT_PTR)TRUE;
            } else if (LOWORD(wParam) == ID_MIDDLE_EDITCONFIG) {
                if (middle_) {
                    middle_->onEditConfig();
                }
            } else if (LOWORD(wParam) == IDCANCEL) {
                return (INT_PTR)TRUE;
            }
            break;
    }
    return (INT_PTR)FALSE;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    middle_ = new Middle();
    if (!middle_->init()) {
        return 0;
    }

    contextMenu_ = LoadMenu(hInstance, MAKEINTRESOURCE(IDR_MIDDLE_CONTEXT_MENU));

    HWND hDlg = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_MIDDLE), NULL, Proc);
    ShowWindow(hDlg, SW_HIDE);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
