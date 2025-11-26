#include "jxwm.hpp"
#include <X11/X.h>
#include <fstream>
#include <sstream>
#include <string.h>
#include <X11/XF86keysym.h>

unsigned int super = Mod4Mask;
unsigned int NOMASK = 0;

unsigned int stringToMod(const std::string& mod)
{
    if (mod == "super") { return super; }
    if (mod == "None") { return NOMASK; }
    if (mod == "windows") { return Mod4Mask; }
    if (mod == "shift") { return ShiftMask; }
    if (mod == "lock") { return LockMask; }
    if (mod == "ctrl") { return ControlMask; }

    return Mod4Mask;
}

void JXWM::ReadConfigFile(const std::string& configFile)
{
    logger.Log(INFO, "Reading file " + configFile + "...");
    std::ifstream file(configFile);
    if (!file.is_open())
    {
        logger.Log(ERROR, "Could not open config file " + configFile);
        return;
    }

    std::string line;
    std::vector<std::string> argsToSpawn;
    while (std::getline(file, line))
    {
        std::stringstream ss(line);
        std::string item;
        char delimiter = ',';
        std::vector<std::string> split;
        while (std::getline(ss, item, delimiter)) { split.push_back(item); }
        if (split.empty()) { continue; }
        if (split[0] == "run")
        {
            argsToSpawn.push_back(split[1]);
        }
        else if(split[0] == "kb")
        {
            if (split[3] == "spawn")
            {
                keybindings.push_back({stringToMod(split[1]), XStringToKeysym(split[2].c_str()), &JXWM::Spawn, {.spawn = strdup(split[4].c_str())}});
            }
            else if (split[3] == "killwindow")
            {
                keybindings.push_back({stringToMod(split[1]), XStringToKeysym(split[2].c_str()), &JXWM::KillWindow, nullptr});
            }
            else if (split[3] == "quit")
            {
                keybindings.push_back({stringToMod(split[1]), XStringToKeysym(split[2].c_str()), &JXWM::Quit, nullptr});
            }
            else if (split[3] == "logout")
            {
                keybindings.push_back({stringToMod(split[1]), XStringToKeysym(split[2].c_str()), &JXWM::Logout, nullptr});
            }
            else if(split[3] == "reload")
            {
                keybindings.push_back({stringToMod(split[1]), XStringToKeysym(split[2].c_str()), &JXWM::ReloadConfig, nullptr});
            }
            else if (split[3] == "move_left")
            {
                keybindings.push_back({stringToMod(split[1]), XStringToKeysym(split[2].c_str()), &JXWM::MoveClientLeft, nullptr});
            }
            else if (split[3] == "move_right")
            {
                keybindings.push_back({stringToMod(split[1]), XStringToKeysym(split[2].c_str()), &JXWM::MoveClientRight, nullptr});
            }
        }
        else if ( split[0] == "set")
        {
            if (split[1] == "bw") { settings.borderWidth = std::stoi(split[2]); }

            else if (split[1] == "switchonopen")
            {
                if (split[2] == "true") { settings.switchOnOpen = true; }
                else { settings.switchOnOpen = false; }
            }

            else if (split[1] == "switchonmove")
            {
                if (split[2] == "true") { settings.switchOnMove = true; }
                else { settings.switchOnMove = false; }
            }

            else if (split[1] == "verbose")
            {
                if (split[2] == "on") { logger.verbose = true; }
                else { logger.verbose = false; }
            }

            else if (split[1] == "fbordercolor") { settings.focusedBorderColor = std::stoul(split[2], nullptr, 16); }
            else if (split[1] == "ubordercolor") { settings.unfocusedBorderColor = std::stoul(split[2], nullptr, 16); }

            else if (split[1] == "super") { super = stringToMod(split[2]); }
        }
    }

    for (auto cmd : argsToSpawn) { Spawn(cmd.c_str()); }
    Arrange();
}
