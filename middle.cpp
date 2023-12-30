#include "Middle.h"

#include "helpers.h"

#define ARBITRARY_BUFFER_SIZE 4096

#define TOML_EXCEPTIONS 0
#include "toml.h"

static const char * exactString(bool exact)
{
    return exact ? "exact" : "substr";
}

static const std::string getConfigFilename()
{
    return getExeAdjacentFile("middle.toml");
}

static const std::string makeToml(const toml::table & tbl)
{
    auto formatFlags = toml::toml_formatter::default_flags & ~toml::format_flags::allow_literal_strings;
    std::ostringstream ss;
    ss << toml::toml_formatter(tbl, formatFlags);
    return ss.str() + "\n";
}

Middle::Middle()
{
}

Middle::~Middle()
{
}

bool Middle::parseConfig()
{
    logDebug("Middle::init()\n");

    auto result = toml::parse_file(getConfigFilename());
    if (!result) {
        const std::string error = std::string(result.error().description());
        return fatalError("Parsing failed: %s", error.c_str());
    }
    auto config = std::move(result).table();

    // Basic global settings
    persist_ = config["persist"].value_or(false);
    runOnStartup_ = config["startup"].value_or(false);
    hotkey_ = config["hotkey"].value<std::string>().value_or("win+N");
    logDebug("hotkey: %s\n", hotkey_.c_str());
    defaultX_ = config["x"].value<int>().value_or(0);
    defaultY_ = config["y"].value<int>().value_or(0);
    defaultW_ = config["w"].value<int>().value_or(0);
    defaultH_ = config["h"].value<int>().value_or(0);

    // Per-automatic setting
    auto automatic = config["automatic"];
    if (!!automatic) {
        toml::array * arr = automatic.as_array();
        for (auto it = arr->begin(); it != arr->end(); ++it) {
            Automatic a;
            a.index = (int)automatics_.size();
            a.x = it->at_path("x").value<int>().value_or(0);
            a.y = it->at_path("y").value<int>().value_or(0);
            a.w = it->at_path("w").value<int>().value_or(0);
            a.h = it->at_path("h").value<int>().value_or(0);
            a.titleString = it->at_path("title").value<std::string>().value_or("");
            a.classString = it->at_path("class").value<std::string>().value_or("");
            a.exactTitle = it->at_path("exactTitle").value<bool>().value_or(true);
            a.exactClass = it->at_path("exactClass").value<bool>().value_or(true);

            logDebug("automatic[%d] [%d/%d/%d/%d] title[%s]:\"%s\" class[%s]:\"%s\"\n",
                     a.index,
                     a.x,
                     a.y,
                     a.w,
                     a.h,
                     exactString(a.exactTitle),
                     a.titleString.c_str(),
                     exactString(a.exactClass),
                     a.classString.c_str());

            automatics_.push_back(a);
        }
    }

    return true;
}

bool Middle::init()
{
    logDebug("Middle::init()\n");

    if (!parseConfig()) {
        return false;
    }

    runOnStartup(runOnStartup_);
    return true;
}

void Middle::onTick()
{
    HWND foregroundWindow = GetForegroundWindow();
    if (!foregroundWindow) {
        return;
    }

    char windowTitleBuffer[ARBITRARY_BUFFER_SIZE];
    if (!GetWindowText(foregroundWindow, windowTitleBuffer, ARBITRARY_BUFFER_SIZE)) {
        return;
    }
    const std::string & windowTitle = windowTitleBuffer;

    char windowClassBuffer[ARBITRARY_BUFFER_SIZE];
    if (!GetClassName(foregroundWindow, windowClassBuffer, ARBITRARY_BUFFER_SIZE)) {
    }
    const std::string & windowClass = windowClassBuffer;

    RECT windowRect;
    GetWindowRect(foregroundWindow, &windowRect);
    const int currentX = windowRect.left;
    const int currentY = windowRect.top;
    const int currentW = windowRect.right - windowRect.left;
    const int currentH = windowRect.bottom - windowRect.top;

    const LONG currentStyle = GetWindowLong(foregroundWindow, GWL_STYLE);
    const LONG requestedStyle = currentStyle & ~(WS_BORDER | WS_CAPTION | WS_DLGFRAME | WS_OVERLAPPEDWINDOW | WS_POPUPWINDOW);

    logDebug("Foreground[%d/%d/%d/%d] \"%s\" \"%s\"\n",
             windowRect.left,
             windowRect.top,
             windowRect.right,
             windowRect.bottom,
             windowTitle.c_str(),
             windowClass.c_str());

    // See if this window matches one of our automatics
    Automatic * found = nullptr;
    for (auto it = automatics_.begin(); it != automatics_.end(); ++it) {
        if (it->exactTitle) {
            if (windowTitle != it->titleString) {
                continue;
            }
        } else {
            if (windowTitle.find(it->titleString) == std::string::npos) {
                continue;
            }
        }

        if (it->exactClass) {
            if (windowClass != it->classString) {
                continue;
            }
        } else {
            if (windowClass.find(it->classString) == std::string::npos) {
                continue;
            }
        }

        found = &(*it);
        break;
    }
    if (!found) {
        logDebug(" * Foreground not a current automatic window\n");
        return;
    }
    logDebug(" * Found automatic index %d\n", found->index);

    int x = defaultX_;
    int y = defaultY_;
    int w = defaultW_;
    int h = defaultH_;
    if (found->x || found->y || found->w || found->h) {
        x = found->x;
        y = found->y;
        w = found->w;
        h = found->h;
    }

    if ((x == currentX) && (y == currentY) && (w == currentW) && (h == currentH) && (requestedStyle == currentStyle)) {
        logDebug(" * Ignoring, window already happy\n");
        return;
    }

    logDebug(" * Move -> [%d/%d/%d/%d] [%u]=>[%u]\n", x, y, w, h, currentStyle, requestedStyle);
    SetWindowLong(foregroundWindow, GWL_STYLE, requestedStyle); // Nuke various style flags to make it borderless
    MoveWindow(foregroundWindow, x, y, w, h, TRUE);             // Move it where it belongs
}

void Middle::onHotkey()
{
    logDebug("onHotkey()\n");

    HWND foregroundWindow = GetForegroundWindow();
    if (!foregroundWindow) {
        return;
    }

    char windowTitleBuffer[ARBITRARY_BUFFER_SIZE];
    if (!GetWindowText(foregroundWindow, windowTitleBuffer, ARBITRARY_BUFFER_SIZE)) {
        return;
    }
    const std::string & windowTitle = windowTitleBuffer;

    char windowClassBuffer[ARBITRARY_BUFFER_SIZE];
    if (!GetClassName(foregroundWindow, windowClassBuffer, ARBITRARY_BUFFER_SIZE)) {
    }
    const std::string & windowClass = windowClassBuffer;

    if (persist_) {
        // clang-format off
        std::string toAppend =
            "\n[[automatic]]\n\n" +
            makeToml(toml::table { { "title", windowTitle } }) +
            makeToml(toml::table { { "exactTitle", true } }) +
            "\n" +
            makeToml(toml::table { { "class", windowClass } }) +
            makeToml(toml::table { { "exactClass", true } });
        // clang-format on

        logDebug("Appending new entry and activating:\n%s\n", toAppend.c_str());
        std::string contents;
        if (!readEntireFile(getConfigFilename(), contents)) {
            return;
        }
        contents += toAppend;
        writeEntireFile(getConfigFilename(), contents);
    }

    // Add to live config and activate immediately
    Automatic a;
    a.index = (int)automatics_.size();
    a.x = 0;
    a.y = 0;
    a.w = 0;
    a.h = 0;
    a.titleString = windowTitle;
    a.classString = windowClass;
    a.exactTitle = true;
    a.exactClass = true;
    automatics_.push_back(a);
    onTick();
}

void Middle::onEditConfig()
{
    logDebug("onEditConfig()\n");

    ShellExecute(nullptr, nullptr, getConfigFilename().c_str(), nullptr, nullptr, SW_SHOW);
}
