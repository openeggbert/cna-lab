#pragma once

#include <functional>
#include <string>

namespace IronGang
{
    // plan_04 IG-04-002. Four levels, ordered: filtering keeps everything at or above the minimum.
    //   Debug   -- detail useful while working on a subsystem; off by default.
    //   Info    -- something happened that a player-visible run should mention (an asset loaded,
    //              a mission advanced, a save was written).
    //   Warning -- the game carried on, but not the way the data asked for (a fallback was used,
    //              a value was ignored). Every warning should name what was ignored and what
    //              happened instead.
    //   Error   -- something the caller asked for did not happen at all.
    enum class LogSeverity
    {
        Debug,
        Info,
        Warning,
        Error,
    };

    // What the message is about. Categories exist to be filtered, so a new one is only worth
    // adding when someone would plausibly want to watch it alone -- see docs/logging.md.
    enum class LogCategory
    {
        Application,
        Assets,
        Audio,
        Config,
        Cutscene,
        Dialogue,
        Mission,
        Save,
    };

    [[nodiscard]] const char* LogSeverityName(LogSeverity severity) noexcept;
    [[nodiscard]] const char* LogCategoryName(LogCategory category) noexcept;
    [[nodiscard]] bool ParseLogSeverity(const std::string& name, LogSeverity& out);
    [[nodiscard]] bool ParseLogCategory(const std::string& name, LogCategory& out);

    // The game's one logging path. Deliberately static: there is a single log for a single game,
    // and threading a logger reference through every subsystem would buy nothing here.
    //
    // Output goes to stderr, so a run's diagnostics can be redirected without swallowing the
    // gameplay text the prototype prints to stdout (dialogue lines).
    class Log final
    {
    public:
        using Sink = std::function<void(LogCategory, LogSeverity, const std::string&)>;

        // Messages below this are dropped. Default: Info.
        static void SetMinimumSeverity(LogSeverity severity) noexcept;
        [[nodiscard]] static LogSeverity GetMinimumSeverity() noexcept;

        // A disabled category is silent at every severity, errors included -- turning a category
        // off means "I do not want to hear from this subsystem", and a half-off category would be
        // more confusing than either state.
        static void SetCategoryEnabled(LogCategory category, bool enabled) noexcept;
        [[nodiscard]] static bool IsCategoryEnabled(LogCategory category) noexcept;
        [[nodiscard]] static bool IsEnabled(LogCategory category, LogSeverity severity) noexcept;

        static void Write(LogCategory category, LogSeverity severity, const std::string& message);
        static void Debug(LogCategory category, const std::string& message);
        static void Info(LogCategory category, const std::string& message);
        static void Warning(LogCategory category, const std::string& message);
        static void Error(LogCategory category, const std::string& message);

        // "[IronGang][mission][warning] ..." -- the exact line the default sink writes.
        [[nodiscard]] static std::string FormatLine(LogCategory category,
                                                    LogSeverity severity,
                                                    const std::string& message);

        // Replaces the destination; an empty sink restores stderr. Tests capture with this, and a
        // file or in-game console log would attach the same way.
        static void SetSink(Sink sink);
        // Back to the defaults: stderr, minimum severity Info, every category enabled.
        static void Reset();
    };
}
