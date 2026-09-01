#include "IronGang/Missions/MissionContext.hpp"

namespace IronGang
{
    namespace
    {
        std::string TypeMismatchMessage(const std::string& name,
                                        MissionValueType declared,
                                        MissionValueType assigned)
        {
            return "Mission symbol \"" + name + "\" is declared " + MissionValueTypeName(declared) +
                   " and cannot hold a " + MissionValueTypeName(assigned) + " value";
        }
    }

    MissionContext::Symbol* MissionContext::Find(const std::string& name) noexcept
    {
        for (Symbol& symbol : symbols_)
        {
            if (symbol.name == name)
            {
                return &symbol;
            }
        }
        return nullptr;
    }

    const MissionContext::Symbol* MissionContext::Find(const std::string& name) const noexcept
    {
        for (const Symbol& symbol : symbols_)
        {
            if (symbol.name == name)
            {
                return &symbol;
            }
        }
        return nullptr;
    }

    bool MissionContext::DeclareFact(std::string name, MissionValue initial, std::string& errorMessage)
    {
        if (name.empty())
        {
            errorMessage = "Mission fact name must not be empty";
            return false;
        }
        if (Find(name) != nullptr)
        {
            errorMessage = "Mission symbol \"" + name + "\" is already declared";
            return false;
        }
        Symbol symbol;
        symbol.name = std::move(name);
        symbol.initial = initial;
        symbol.current = std::move(initial);
        symbol.isFact = true;
        symbols_.push_back(std::move(symbol));
        return true;
    }

    bool MissionContext::DeclareVariable(std::string name, MissionValue initial, std::string& errorMessage)
    {
        if (name.empty())
        {
            errorMessage = "Mission variable name must not be empty";
            return false;
        }
        if (Find(name) != nullptr)
        {
            errorMessage = "Mission symbol \"" + name + "\" is already declared";
            return false;
        }
        std::size_t variableCount = 0;
        for (const Symbol& symbol : symbols_)
        {
            variableCount += symbol.isFact ? 0U : 1U;
        }
        if (variableCount >= kMaxMissionVariables)
        {
            errorMessage = "Mission declares more than " + std::to_string(kMaxMissionVariables) + " variables";
            return false;
        }
        Symbol symbol;
        symbol.name = std::move(name);
        symbol.initial = initial;
        symbol.current = std::move(initial);
        symbol.isFact = false;
        symbols_.push_back(std::move(symbol));
        return true;
    }

    bool MissionContext::SetFact(const std::string& name, const MissionValue& value, std::string& errorMessage)
    {
        Symbol* symbol = Find(name);
        if (symbol == nullptr || !symbol->isFact)
        {
            errorMessage = "Unknown mission fact \"" + name + "\"";
            return false;
        }
        if (symbol->current.GetType() != value.GetType())
        {
            errorMessage = TypeMismatchMessage(name, symbol->current.GetType(), value.GetType());
            return false;
        }
        symbol->current = value;
        return true;
    }

    bool MissionContext::SetVariable(const std::string& name, const MissionValue& value,
                                     std::string& errorMessage)
    {
        Symbol* symbol = Find(name);
        if (symbol == nullptr)
        {
            errorMessage = "Unknown mission variable \"" + name + "\"";
            return false;
        }
        if (symbol->isFact)
        {
            errorMessage = "Mission fact \"" + name + "\" is read-only and cannot be assigned";
            return false;
        }
        if (symbol->current.GetType() != value.GetType())
        {
            errorMessage = TypeMismatchMessage(name, symbol->current.GetType(), value.GetType());
            return false;
        }
        symbol->current = value;
        return true;
    }

    void MissionContext::ResetVariables()
    {
        for (Symbol& symbol : symbols_)
        {
            if (!symbol.isFact)
            {
                symbol.current = symbol.initial;
            }
        }
    }

    bool MissionContext::TryGetType(const std::string& name, MissionValueType& out) const
    {
        const Symbol* symbol = Find(name);
        if (symbol == nullptr)
        {
            return false;
        }
        out = symbol->current.GetType();
        return true;
    }

    bool MissionContext::TryGetValue(const std::string& name, MissionValue& out) const
    {
        const Symbol* symbol = Find(name);
        if (symbol == nullptr)
        {
            return false;
        }
        out = symbol->current;
        return true;
    }

    bool MissionContext::IsVariable(const std::string& name) const
    {
        const Symbol* symbol = Find(name);
        return symbol != nullptr && !symbol->isFact;
    }

    bool MissionContext::IsFact(const std::string& name) const
    {
        const Symbol* symbol = Find(name);
        return symbol != nullptr && symbol->isFact;
    }

    std::vector<MissionVariableSnapshot> MissionContext::CaptureVariables() const
    {
        std::vector<MissionVariableSnapshot> captured;
        for (const Symbol& symbol : symbols_)
        {
            if (!symbol.isFact)
            {
                captured.push_back(MissionVariableSnapshot{symbol.name, symbol.current});
            }
        }
        return captured;
    }
}
