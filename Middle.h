#ifndef MIDDLE_H
#define MIDDLE_H

#include <windows.h>

#include <string>
#include <vector>

class Middle
{
public:
    Middle();
    ~Middle();

    bool init();

    void onTick();
    void onHotkey();
    void onEditConfig();

    const std::string & hotkey() { return hotkey_; }

private:
    bool parseConfig();

    bool persist_;
    bool runOnStartup_;
    std::string hotkey_;

    int defaultX_;
    int defaultY_;
    int defaultW_;
    int defaultH_;

    struct Automatic
    {
        int index;

        int x;
        int y;
        int w;
        int h;
        std::string titleString;
        std::string classString;
        bool exactTitle;
        bool exactClass;
    };

    std::vector<Automatic> automatics_;
};

#endif
