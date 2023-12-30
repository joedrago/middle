#include "targetver.h"

#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <shellapi.h>
#include <sstream>
#include <stdbool.h>
#include <stdio.h>
#include <string>
#include <vector>

#include "resource.h"

#define WM_SHELLICONCLICKED (WM_USER + 1)

// --------------------------------------------------------------------------------------
// Globals! Yuck!

HMENU contextMenu_;

// Determined in loadConfig() (including defaults)
static bool runOnStartup_;
static std::string hotKey_;
static int offsetX_;
static int offsetY_;
static int width_;
static int height_;

// --------------------------------------------------------------------------------------
// Generic helper functions

static bool fatalError(const char * format, ...)
{
    char errorText[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(errorText, sizeof(errorText), format, args);
    MessageBox(NULL, errorText, "Middle: Fatal Error", MB_OK);
    PostQuitMessage(0);
    return false;
}

// taken shamelessly from http://stackoverflow.com/questions/236129/split-a-string-in-c
static void split(const std::string & s, char delim, std::vector<std::string> & elems)
{
    std::stringstream ss;
    ss.str(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
}

// From: https://stackoverflow.com/questions/216823/how-to-trim-a-stdstring
static inline void ltrim(std::string & s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
}
static inline void rtrim(std::string & s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
}
static inline void trim(std::string & s)
{
    rtrim(s);
    ltrim(s);
}

// --------------------------------------------------------------------------------------
// Registry nonsense

static bool runOnStartup(bool enabled)
{
    char buffer[MAX_PATH];
    if (GetModuleFileName(NULL, (char *)buffer, MAX_PATH) == ERROR_INSUFFICIENT_BUFFER) {
        return fatalError("Not enough room to get module filename?");
    }

    HKEY hkey;
    LONG RegOpenResult;
    RegOpenResult = RegOpenKeyEx(HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hkey);
    if (enabled) {
        auto wut = RegSetValueEx(hkey, "Middle", 0, REG_SZ, (unsigned char *)buffer, (DWORD)strlen(buffer));
        printf("lol");
    } else {
        RegDeleteValueA(hkey, "Middle");
    }
    RegCloseKey(hkey);
    return true;
}

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
    if (!addHotKey(hWnd, 1, hotKey_)) {
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
// The "middle" of Middle

static void moveForegroundWindow()
{
    HWND foregroundWindow = GetForegroundWindow();
    RECT destinationRect = { offsetX_, offsetY_, width_, height_ };

    // Nuke various style flags to make it borderless
    LONG style = GetWindowLong(foregroundWindow, GWL_STYLE);
    SetWindowLong(foregroundWindow, GWL_STYLE, style & ~(WS_BORDER | WS_CAPTION | WS_DLGFRAME | WS_OVERLAPPEDWINDOW | WS_POPUPWINDOW));

    MoveWindow(foregroundWindow, destinationRect.left, destinationRect.top, destinationRect.right, destinationRect.bottom, TRUE);
}

// --------------------------------------------------------------------------------------
// Configuration

static void loadDefaults()
{
    runOnStartup_ = true;
    hotKey_ = "win+N";

    offsetX_ = 0;
    offsetY_ = 0;
    width_ = GetSystemMetrics(SM_CXSCREEN);
    height_ = GetSystemMetrics(SM_CYSCREEN);
}

static bool loadConfig()
{
    loadDefaults();

    char buffer[MAX_PATH];
    if (GetModuleFileName(NULL, (char *)buffer, MAX_PATH) == ERROR_INSUFFICIENT_BUFFER) {
        return fatalError("Not enough room to get module filename?");
    }

    std::string modulePath = buffer;
    size_t pos = modulePath.find_last_of('\\');
    if (pos == std::string::npos) {
        return fatalError("Module filename does not contain a backslash?");
    }

    std::string iniPath = modulePath.substr(0, pos) + "\\middle.ini";
    FILE * f = fopen(iniPath.c_str(), "rb");
    if (!f) {
        return fatalError("middle.ini does not exist next to middle.exe");
    }

    fseek(f, 0, SEEK_END);
    int len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len < 1) {
        return fatalError("middle.ini is empty");
    }

    std::vector<char> raw(len + 1);
    size_t bytesRead = fread(&raw[0], 1, len, f);
    fclose(f);
    if (bytesRead != len) {
        return fatalError("failed to read all %d bytes of middle.ini", len);
    }

    std::string contents = raw.data();
    std::vector<std::string> lines;
    split(contents, '\n', lines);

    for (auto it = lines.begin(); it != lines.end(); ++it) {
        std::string line = *it;
        trim(line);
        if ((line.size() > 0) && (line[0] == '#')) {
            // Comment!
            continue;
        }

        std::vector<std::string> pieces;
        split(line, '=', pieces);
        if (pieces.size() == 2) {
            const std::string & key = pieces[0];
            const std::string & val = pieces[1];
            if (key == "hotkey") {
                hotKey_ = val;
            } else if (key == "startup") {
                runOnStartup_ = ((val == "true") || (val == "1") || (val == "on") || (val == "yes"));
            } else if (key == "x") {
                offsetX_ = atoi(val.c_str());
            } else if (key == "y") {
                offsetY_ = atoi(val.c_str());
            } else if (key == "w") {
                width_ = atoi(val.c_str());
            } else if (key == "h") {
                height_ = atoi(val.c_str());
            }
        }
    }
    return true;
}

// --------------------------------------------------------------------------------------
// Windows nonsense

static INT_PTR CALLBACK Proc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
        case WM_INITDIALOG:
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

        case WM_HOTKEY:
            moveForegroundWindow();
            return (INT_PTR)TRUE;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_QUIT) {
                destroyShellIcon(hDlg);
                PostQuitMessage(0);
                return (INT_PTR)TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                return (INT_PTR)TRUE;
            }
            break;
    }
    return (INT_PTR)FALSE;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    if (!loadConfig()) {
        return 0;
    }

    runOnStartup(runOnStartup_);

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
