#define _CRT_SECURE_NO_WARNINGS

#include "helpers.h"

#include <windows.h>

#include <sstream>
#include <stdbool.h>
#include <stdio.h>

// --------------------------------------------------------------------------------------
// Generic helper functions

#ifdef _DEBUG
void logDebugHelper(const char * format, ...)
{
    char errorText[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(errorText, sizeof(errorText), format, args);
    OutputDebugStringA(errorText);
}
#endif

bool fatalError(const char * format, ...)
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
void split(const std::string & s, char delim, std::vector<std::string> & elems)
{
    std::stringstream ss;
    ss.str(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
}

// From: https://stackoverflow.com/questions/216823/how-to-trim-a-stdstring
inline void ltrim(std::string & s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
}
inline void rtrim(std::string & s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
}
void trim(std::string & s)
{
    rtrim(s);
    ltrim(s);
}

std::string getExeAdjacentFile(const std::string & filename)
{
    char buffer[MAX_PATH];
    if (GetModuleFileName(NULL, (char *)buffer, MAX_PATH) == ERROR_INSUFFICIENT_BUFFER) {
        return "";
    }

    std::string modulePath = buffer;
    size_t pos = modulePath.find_last_of('\\');
    if (pos == std::string::npos) {
        return "";
    }

    return modulePath.substr(0, pos) + "\\" + filename;
}

// --------------------------------------------------------------------------------------
// Registry nonsense

bool runOnStartup(bool enabled)
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
// File manipulation

bool readEntireFile(const std::string & filename, std::string & contents)
{
    FILE * f = fopen(filename.c_str(), "rb");
    if (!f) {
        return fatalError("Cannot read: %s", filename.c_str());
    }
    fseek(f, 0, SEEK_END);
    int len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len < 1) {
        return fatalError("empty file: %s", filename.c_str());
    }

    std::vector<char> raw(len + 1);
    size_t bytesRead = fread(&raw[0], 1, len, f);
    fclose(f);
    if (bytesRead != len) {
        return fatalError("failed to read all %d bytes of middle.ini", len);
    }
    contents = raw.data();
    return true;
}

bool writeEntireFile(const std::string & filename, const std::string & contents)
{
    FILE * f = fopen(filename.c_str(), "wb");
    if (!f) {
        return fatalError("Cannot write: %s", filename.c_str());
    }
    fwrite(contents.data(), 1, contents.size(), f);
    fclose(f);
    return true;
}
