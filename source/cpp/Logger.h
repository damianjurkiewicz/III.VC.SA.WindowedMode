#pragma once

#include <string>
#include <sstream>
#include <fstream>

class SimpleLogger
{
public:
    static SimpleLogger& GetInstance();
    void Log(const std::string& message);

    SimpleLogger(const SimpleLogger&) = delete;
    SimpleLogger& operator=(const SimpleLogger&) = delete;

private:
    SimpleLogger();
    ~SimpleLogger();

    std::ofstream logFile;
};

class LogStream
{
public:
    LogStream() {}
    ~LogStream()
    {
        SimpleLogger::GetInstance().Log(stream.str());
    }

    template<typename T>
    LogStream& operator<<(const T& val)
    {
        stream << val;
        return *this;
    }
private:
    std::stringstream stream;
};

#define LOG_STREAM LogStream()