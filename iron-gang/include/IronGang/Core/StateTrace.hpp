#pragma once

#include "IronGang/Core/WorldTypes.hpp"

#include <string>
#include <vector>

namespace IronGang
{
    // plan_39 IG-39-020..024: what the game was doing at a given simulation update.
    //
    // The vertical-slice gates ask for controls, missions and save/load to be *verified*, and until
    // now the only observable outputs were log lines and screenshots. A log line says a mission
    // changed state; it does not say the player moved when a key was held. Screenshots say what a
    // frame looked like; they do not say the player stopped at a lamp post rather than beside one.
    //
    // So a run can record its own state. Deliberately a flat record of things that already exist --
    // no new game state, nothing the game reads back -- written as JSON Lines so a trace can be
    // appended during a run and read with one pass.
    struct StateTraceRecord
    {
        int update{0};
        Vector3 position{};
        float yaw{0.0F};
        bool driving{false};
        float speedKph{0.0F};
        std::string district;
        std::string missionId;
        std::string missionState;
        // Empty when no line is showing.
        std::string dialogueLineId;
    };

    // One JSON object on one line, with no trailing newline. Floats are written with enough
    // precision to compare positions a few centimetres apart, which is what a collision check needs.
    [[nodiscard]] std::string FormatStateTraceRecord(const StateTraceRecord& record);

    // Appends @p record to @p path, creating it if needed. Returns false with @p errorMessage set.
    // A trace is a diagnostic: a caller that cannot write one should say so and carry on, never
    // take the run down.
    [[nodiscard]] bool AppendStateTraceRecord(const std::string& path,
                                              const StateTraceRecord& record,
                                              std::string& errorMessage);
}
