#pragma once

#include <string>
#include <sstream>
#include <fstream>

class SimpleLogger
{
public:
    static SimpleLogger& GetInstance();
    void Log(const std::string& message);

    // --- TEGO BRAKUJE W TWOIM PLIKU ---
    /**
     * @brief Włącza lub wyłącza logowanie.
     */
    void SetEnabled(bool state);

    /**
     * @brief Sprawdza, czy logowanie jest włączone.
     */
    bool IsEnabled() const;
    // --- KONIEC ---

    SimpleLogger(const SimpleLogger&) = delete;
    SimpleLogger& operator=(const SimpleLogger&) = delete;

private:
    SimpleLogger();
    ~SimpleLogger();

    std::ofstream logFile;

    // --- TEGO TEŻ BRAKUJE ---
    bool bEnabled; // Przełącznik loggera
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

// --- I TA ZMIANA JEST KRYTYCZNA ---
#define LOG_STREAM if (!SimpleLogger::GetInstance().IsEnabled()) {} else LogStream()