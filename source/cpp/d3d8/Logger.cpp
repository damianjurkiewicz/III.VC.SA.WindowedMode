#include "Logger.h"
#include <chrono>
#include <iomanip>

SimpleLogger& SimpleLogger::GetInstance()
{
    static SimpleLogger instance;
    return instance;
}

SimpleLogger::SimpleLogger()
{
    logFile.open("COMP.WindowedMode.log", std::ios::out | std::ios::trunc);
    if (logFile.is_open())
    {
        Log("Logger initialized.");
    }
}

SimpleLogger::~SimpleLogger()
{
    if (logFile.is_open())
    {
        Log("Logger shutting down.");
        logFile.close();
    }
}

void SimpleLogger::Log(const std::string& message)
{
    if (!logFile.is_open()) return;

    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm bt = {};
    localtime_s(&bt, &in_time_t);

    std::stringstream ss;
    ss << std::put_time(&bt, "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();

    logFile << "[" << ss.str() << "] " << message << std::endl;
}