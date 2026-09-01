#include "IronGang/Core/StateTrace.hpp"

#include <array>
#include <charconv>
#include <fstream>
#include <system_error>

namespace IronGang
{
    namespace
    {
        // Shortest round-tripping form, the same convention MissionValue uses -- a trace that
        // rounds positions cannot answer "did the player stop 5 cm short or 50?".
        [[nodiscard]] std::string Number(float value)
        {
            std::array<char, 48> buffer{};
            const std::to_chars_result result =
                std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
            if (result.ec != std::errc())
            {
                return "0";
            }
            return std::string(buffer.data(), result.ptr);
        }

        [[nodiscard]] std::string Quoted(const std::string& text)
        {
            std::string out = "\"";
            for (const char character : text)
            {
                if (character == '"' || character == '\\')
                {
                    out += '\\';
                }
                out += character;
            }
            out += '"';
            return out;
        }
    }

    std::string FormatStateTraceRecord(const StateTraceRecord& record)
    {
        std::string line = "{";
        line += "\"update\":" + std::to_string(record.update);
        line += ",\"x\":" + Number(record.position.X);
        line += ",\"y\":" + Number(record.position.Y);
        line += ",\"z\":" + Number(record.position.Z);
        line += ",\"yaw\":" + Number(record.yaw);
        line += ",\"driving\":" + std::string(record.driving ? "true" : "false");
        line += ",\"speedKph\":" + Number(record.speedKph);
        line += ",\"district\":" + Quoted(record.district);
        line += ",\"mission\":" + Quoted(record.missionId);
        line += ",\"missionState\":" + Quoted(record.missionState);
        line += ",\"dialogueLine\":" + Quoted(record.dialogueLineId);
        line += "}";
        return line;
    }

    bool AppendStateTraceRecord(const std::string& path,
                                const StateTraceRecord& record,
                                std::string& errorMessage)
    {
        std::ofstream stream(path, std::ios::app);
        if (!stream)
        {
            errorMessage = "could not open state trace for writing: " + path;
            return false;
        }
        stream << FormatStateTraceRecord(record) << '\n';
        if (!stream)
        {
            errorMessage = "could not write to state trace: " + path;
            return false;
        }
        return true;
    }
}
