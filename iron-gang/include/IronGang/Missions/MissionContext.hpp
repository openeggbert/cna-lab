#pragma once

#include "IronGang/Missions/MissionValue.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace IronGang
{
    // Upper bound on how many variables one mission file may declare (IG-24-014: a malformed or
    // hostile mission file must not be able to grow engine state without limit). Missions in this
    // project track a handful of flags/counters; 64 is far above any real authored mission.
    inline constexpr std::size_t kMaxMissionVariables = 64;

    // plan_24 IG-24-005/031: the symbol table and current values a mission's condition/action
    // expressions are compiled and evaluated against.
    //
    // Two kinds of symbol share one namespace, because an expression reads them identically:
    //   * facts     -- read-only engine signals (is the player driving? how far is the sedan?)
    //                  declared once by the game and refreshed every frame.
    //   * variables -- mission-owned typed state declared by the mission file, written by its own
    //                  "set" actions, reset by a retry, and round-tripped through the save file.
    //
    // Both are declared with an initial value, which fixes their type: a later Set* with a
    // different type is rejected rather than silently reinterpreted, so an expression's compile-time
    // type check stays valid for the whole run.
    class MissionContext final
    {
    public:
        struct Symbol
        {
            std::string name;
            MissionValue initial;
            MissionValue current;
            bool isFact{false};
        };

        // Declares a symbol. Fails on a duplicate name (in either kind), an empty name, or -- for
        // variables -- on exceeding kMaxMissionVariables.
        [[nodiscard]] bool DeclareFact(std::string name, MissionValue initial, std::string& errorMessage);
        [[nodiscard]] bool DeclareVariable(std::string name, MissionValue initial, std::string& errorMessage);

        // Assigns a new value to an already-declared symbol of the same type. SetVariable refuses to
        // write a fact (facts are engine-owned) and vice versa.
        [[nodiscard]] bool SetFact(const std::string& name, const MissionValue& value, std::string& errorMessage);
        [[nodiscard]] bool SetVariable(const std::string& name, const MissionValue& value,
                                       std::string& errorMessage);

        // Restores every variable to its declared initial value; facts are left alone (the game
        // overwrites them next frame anyway). This is what a mission retry does (IG-24-009).
        void ResetVariables();

        [[nodiscard]] bool TryGetType(const std::string& name, MissionValueType& out) const;
        [[nodiscard]] bool TryGetValue(const std::string& name, MissionValue& out) const;
        [[nodiscard]] bool IsVariable(const std::string& name) const;
        [[nodiscard]] bool IsFact(const std::string& name) const;

        [[nodiscard]] const std::vector<Symbol>& GetSymbols() const noexcept { return symbols_; }
        // Declared variables in declaration order, for the save file (IG-24-029/039).
        [[nodiscard]] std::vector<MissionVariableSnapshot> CaptureVariables() const;

    private:
        [[nodiscard]] Symbol* Find(const std::string& name) noexcept;
        [[nodiscard]] const Symbol* Find(const std::string& name) const noexcept;

        std::vector<Symbol> symbols_;
    };
}
