#include "IronGang/Core/Log.hpp"

#include <array>
#include <cstddef>
#include <iostream>
#include <mutex>

namespace IronGang
{
    namespace
    {
        constexpr std::size_t kCategoryCount = static_cast<std::size_t>(LogCategory::Save) + 1;

        struct LogState
        {
            LogSeverity minimumSeverity{LogSeverity::Info};
            std::array<bool, kCategoryCount> categoryEnabled{};
            Log::Sink sink;
            std::mutex mutex;

            LogState() { categoryEnabled.fill(true); }
        };

        LogState& State()
        {
            static LogState state;
            return state;
        }
    }

    const char* LogSeverityName(LogSeverity severity) noexcept
    {
        switch (severity)
        {
            case LogSeverity::Debug: return "debug";
            case LogSeverity::Info: return "info";
            case LogSeverity::Warning: return "warning";
            case LogSeverity::Error: return "error";
        }
        return "info";
    }

    const char* LogCategoryName(LogCategory category) noexcept
    {
        switch (category)
        {
            case LogCategory::Application: return "app";
            case LogCategory::Assets: return "assets";
            case LogCategory::Audio: return "audio";
            case LogCategory::Config: return "config";
            case LogCategory::Cutscene: return "cutscene";
            case LogCategory::Dialogue: return "dialogue";
            case LogCategory::Mission: return "mission";
            case LogCategory::Rendering: return "rendering";
            case LogCategory::Save: return "save";
        }
        return "app";
    }

    bool ParseLogSeverity(const std::string& name, LogSeverity& out)
    {
        if (name == "debug") { out = LogSeverity::Debug; return true; }
        if (name == "info") { out = LogSeverity::Info; return true; }
        if (name == "warning") { out = LogSeverity::Warning; return true; }
        if (name == "error") { out = LogSeverity::Error; return true; }
        return false;
    }

    bool ParseLogCategory(const std::string& name, LogCategory& out)
    {
        for (std::size_t index = 0; index < kCategoryCount; ++index)
        {
            const LogCategory category = static_cast<LogCategory>(index);
            if (name == LogCategoryName(category))
            {
                out = category;
                return true;
            }
        }
        return false;
    }

    void Log::SetMinimumSeverity(LogSeverity severity) noexcept
    {
        LogState& state = State();
        const std::lock_guard<std::mutex> lock(state.mutex);
        state.minimumSeverity = severity;
    }

    LogSeverity Log::GetMinimumSeverity() noexcept
    {
        LogState& state = State();
        const std::lock_guard<std::mutex> lock(state.mutex);
        return state.minimumSeverity;
    }

    void Log::SetCategoryEnabled(LogCategory category, bool enabled) noexcept
    {
        LogState& state = State();
        const std::lock_guard<std::mutex> lock(state.mutex);
        state.categoryEnabled[static_cast<std::size_t>(category)] = enabled;
    }

    bool Log::IsCategoryEnabled(LogCategory category) noexcept
    {
        LogState& state = State();
        const std::lock_guard<std::mutex> lock(state.mutex);
        return state.categoryEnabled[static_cast<std::size_t>(category)];
    }

    bool Log::IsEnabled(LogCategory category, LogSeverity severity) noexcept
    {
        LogState& state = State();
        const std::lock_guard<std::mutex> lock(state.mutex);
        return state.categoryEnabled[static_cast<std::size_t>(category)] &&
               static_cast<int>(severity) >= static_cast<int>(state.minimumSeverity);
    }

    std::string Log::FormatLine(LogCategory category, LogSeverity severity, const std::string& message)
    {
        return std::string("[IronGang][") + LogCategoryName(category) + "][" + LogSeverityName(severity) +
               "] " + message;
    }

    void Log::Write(LogCategory category, LogSeverity severity, const std::string& message)
    {
        LogState& state = State();
        Sink sink;
        {
            const std::lock_guard<std::mutex> lock(state.mutex);
            if (!state.categoryEnabled[static_cast<std::size_t>(category)] ||
                static_cast<int>(severity) < static_cast<int>(state.minimumSeverity))
            {
                return;
            }
            sink = state.sink;
        }

        // The sink is called outside the lock: it is caller-supplied, may be slow, and must never
        // be able to deadlock the game by logging from inside itself.
        if (sink)
        {
            sink(category, severity, message);
            return;
        }
        std::cerr << FormatLine(category, severity, message) << "\n";
    }

    void Log::Debug(LogCategory category, const std::string& message)
    {
        Write(category, LogSeverity::Debug, message);
    }

    void Log::Info(LogCategory category, const std::string& message)
    {
        Write(category, LogSeverity::Info, message);
    }

    void Log::Warning(LogCategory category, const std::string& message)
    {
        Write(category, LogSeverity::Warning, message);
    }

    void Log::Error(LogCategory category, const std::string& message)
    {
        Write(category, LogSeverity::Error, message);
    }

    void Log::SetSink(Sink sink)
    {
        LogState& state = State();
        const std::lock_guard<std::mutex> lock(state.mutex);
        state.sink = std::move(sink);
    }

    void Log::Reset()
    {
        LogState& state = State();
        const std::lock_guard<std::mutex> lock(state.mutex);
        state.minimumSeverity = LogSeverity::Info;
        state.categoryEnabled.fill(true);
        state.sink = nullptr;
    }
}
