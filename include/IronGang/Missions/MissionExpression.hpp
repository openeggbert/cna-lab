#pragma once

#include "IronGang/Missions/MissionContext.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace IronGang
{
    // plan_24 IG-24-013/014/031-033: the engine-evaluated condition/action expression language.
    //
    // Grammar (lowest to highest precedence):
    //     expression := or
    //     or         := and ( "||" and )*
    //     and        := comparison ( "&&" comparison )*
    //     comparison := sum ( ( "==" | "!=" | "<" | "<=" | ">" | ">=" ) sum )?
    //     sum        := product ( ( "+" | "-" ) product )*
    //     product    := unary ( ( "*" | "/" ) unary )*
    //     unary      := ( "!" | "-" ) unary | primary
    //     primary    := number | 'single-quoted string' | "true" | "false" | identifier
    //                 | "(" expression ")"
    //
    // An identifier is a fact or variable declared in the MissionContext the expression is
    // compiled against; unknown names are a compile error, never a silent false. Comparison does
    // not chain (a < b < c is rejected) because chained comparison reads as mathematics but would
    // not behave like it.
    //
    // This is explicitly NOT a general-purpose scripting language (IG-24-013): there are no
    // statements, calls, loops, assignments, or engine API surface inside an expression, so a
    // mission file cannot reach anything the game did not declare as a fact or variable. Every
    // expression is fully type-checked at compile time against the declared symbol types, so a
    // malformed mission file fails to load instead of failing mid-mission.
    //
    // The limits below bound both compilation and evaluation (IG-24-014): no recursion is possible
    // (the grammar has no calls), depth is capped while parsing, and evaluation counts steps.
    inline constexpr std::size_t kMissionExpressionMaxLength = 512;
    inline constexpr std::size_t kMissionExpressionMaxTokens = 128;
    inline constexpr std::size_t kMissionExpressionMaxNodes = 96;
    inline constexpr std::size_t kMissionExpressionMaxDepth = 16;
    inline constexpr std::size_t kMissionExpressionMaxSteps = 256;

    class MissionExpression final
    {
    public:
        // Compiles and type-checks @p source against @p context. On failure, errorMessage names
        // what is wrong and at which 1-based column, and @p out is left empty.
        [[nodiscard]] static bool Compile(const std::string& source,
                                          const MissionContext& context,
                                          MissionExpression& out,
                                          std::string& errorMessage);

        // True for a default-constructed expression: nothing to evaluate. A mission state uses
        // this for "terminal state, no automatic transition".
        [[nodiscard]] bool IsEmpty() const noexcept { return root_ < 0; }
        // Type of this expression's result, fixed at compile time. Only meaningful when !IsEmpty().
        [[nodiscard]] MissionValueType GetResultType() const noexcept { return resultType_; }
        [[nodiscard]] const std::string& GetSource() const noexcept { return source_; }

        // Evaluates against the current values in @p context. Fails only on a runtime-only fault
        // the type check cannot catch -- division by zero, a symbol missing from this context, or
        // exceeding kMissionExpressionMaxSteps -- never on a type error.
        [[nodiscard]] bool Evaluate(const MissionContext& context,
                                    MissionValue& out,
                                    std::string& errorMessage) const;
        // Convenience for conditions: fails if GetResultType() is not Bool. An empty expression
        // evaluates to false.
        [[nodiscard]] bool EvaluateBool(const MissionContext& context,
                                        bool& out,
                                        std::string& errorMessage) const;

    private:
        enum class NodeKind : std::uint8_t
        {
            Literal,
            Symbol,
            Not,
            Negate,
            Add,
            Subtract,
            Multiply,
            Divide,
            Less,
            LessOrEqual,
            Greater,
            GreaterOrEqual,
            Equal,
            NotEqual,
            And,
            Or,
        };

        struct Node
        {
            NodeKind kind{NodeKind::Literal};
            MissionValueType type{MissionValueType::Bool};
            MissionValue literal;
            std::string name;
            int left{-1};
            int right{-1};
        };

        [[nodiscard]] bool EvaluateNode(int index,
                                        const MissionContext& context,
                                        std::size_t& steps,
                                        MissionValue& out,
                                        std::string& errorMessage) const;

        friend class MissionExpressionParser;

        std::string source_;
        std::vector<Node> nodes_;
        int root_{-1};
        MissionValueType resultType_{MissionValueType::Bool};
    };
}
