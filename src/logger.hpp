#pragma once

#include <fstream>

enum LogLevel { DEBUG, INFO, ERROR };

class Logger
{
    public:
        Logger(const std::string& fileName);
        ~Logger();

        void Log(LogLevel lvl, const std::string& msg);

        bool verbose;

    private:
        std::ofstream logFile;
};
