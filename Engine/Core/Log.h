#pragma once

#include <iostream>
#include <mutex>
#include <string_view>

namespace NoJob
{
    class Log
    {
    public:
        static void Info(std::string_view message) { Write("INFO", message); }
        static void Warn(std::string_view message) { Write("WARN", message); }
        static void Error(std::string_view message) { Write("ERROR", message); }

    private:
        static void Write(std::string_view level, std::string_view message)
        {
            static std::mutex mutex;
            std::lock_guard<std::mutex> lock(mutex);
            std::cerr << "[NoJob][" << level << "] " << message << '\n';
        }
    };
}
