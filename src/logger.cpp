#include "logger.hpp"
#include <ctime>
#include <iostream>
#include <sstream>

static char* levels[] =
{
    "DEBUG",
    "INFO",
    "ERROR"
};

char* LevelToString(LogLevel lvl) { return levels[lvl]; }

Logger::Logger(const std::string& fileName)
{
    logFile.open(fileName);
    if (!logFile.is_open()) 
    {
        std::cerr << "Failed to open log file " << fileName << std::endl;
    }
}

Logger::~Logger() { logFile.close(); }

void Logger::Log(LogLevel lvl, const std::string& msg)
{
    time_t now = time(0);
    tm* timeinfo = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%m/%d/%Y %H:%M:%S", timeinfo);

    std::ostringstream logEntry;
    logEntry << "[" << timestamp << "] " << LevelToString(lvl) << ": " << msg << std::endl;

    std::cout << logEntry.str();
    logFile << logEntry.str();
    logFile.flush();
}

