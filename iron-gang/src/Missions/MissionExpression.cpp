#include "IronGang/Missions/MissionExpression.hpp"

#include <charconv>
#include <cmath>
#include <system_error>

namespace IronGang
{
    namespace
    {
        struct Token
        {
            enum class Kind
            {
                End,
                Number,
                String,
                Identifier,
                Operator,
                LeftParen,
                RightParen,
            };

            Kind kind{Kind::End};
            std::string text;
            MissionValue value;
            std::size_t column{1};
        };

        std::string At(std::size_t column)
        {
            return " at column " + std::to_string(column);
        }

        bool IsIdentifierStart(char character)
        {
            return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
                   character == '_';
        }

        bool IsDigit(char character) { return character >= '0' && character <= '9'; }

        bool IsIdentifierPart(char character) { return IsIdentifierStart(character) || IsDigit(character); }

        bool TokenizeNumber(const std::string& source, std::size_t& index, Token& token, std::string& errorMessage)
        {
            const std::size_t start = index;
            while (index < source.size() && IsDigit(source[index]))
            {
                ++index;
            }
            bool isFloat = false;
            if (index < source.size() && source[index] == '.')
            {
                isFloat = true;
                ++index;
                if (index >= source.size() || !IsDigit(source[index]))
                {
                    errorMessage = "number must have at least one digit after the decimal point" + At(index + 1);
                    return false;
                }
                while (index < source.size() && IsDigit(source[index]))
                {
                    ++index;
                }
            }
            if (index < source.size() && IsIdentifierStart(source[index]))
            {
                errorMessage = std::string("unexpected character '") + source[index] + "' directly after a number" +
                               At(index + 1);
                return false;
            }

            const char* begin = source.data() + start;
            const char* end = source.data() + index;
            if (isFloat)
            {
                float value = 0.0F;
                const std::from_chars_result result = std::from_chars(begin, end, value);
                if (result.ec != std::errc() || result.ptr != end)
                {
                    errorMessage = "malformed float literal" + At(start + 1);
                    return false;
                }
                token.value = MissionValue::Float(value);
            }
            else
            {
                int value = 0;
                const std::from_chars_result result = std::from_chars(begin, end, value);
                if (result.ec != std::errc() || result.ptr != end)
                {
                    errorMessage = "int literal is out of range" + At(start + 1);
                    return false;
                }
                token.value = MissionValue::Int(value);
            }
            token.kind = Token::Kind::Number;
            token.column = start + 1;
            return true;
        }

        // Single-quoted so a mission-file author never has to escape a quote inside JSON's own
        // double-quoted string. There are no escape sequences: a string literal is exactly the
        // characters between the quotes.
        bool TokenizeString(const std::string& source, std::size_t& index, Token& token, std::string& errorMessage)
        {
            const std::size_t start = index;
            ++index;
            std::string text;
            while (index < source.size() && source[index] != '\'')
            {
                text.push_back(source[index]);
                ++index;
            }
            if (index >= source.size())
            {
                errorMessage = "unterminated string literal" + At(start + 1);
                return false;
            }
            ++index; // closing quote
            token.kind = Token::Kind::String;
            token.value = MissionValue::String(std::move(text));
            token.column = start + 1;
            return true;
        }

        bool Tokenize(const std::string& source, std::vector<Token>& tokens, std::string& errorMessage)
        {
            if (source.size() > kMissionExpressionMaxLength)
            {
                errorMessage = "expression is longer than the " + std::to_string(kMissionExpressionMaxLength) +
                               "-character limit";
                return false;
            }

            std::size_t index = 0;
            while (index < source.size())
            {
                const char character = source[index];
                if (character == ' ' || character == '\t' || character == '\n' || character == '\r')
                {
                    ++index;
                    continue;
                }
                if (tokens.size() >= kMissionExpressionMaxTokens)
                {
                    errorMessage = "expression has more than " + std::to_string(kMissionExpressionMaxTokens) +
                                   " tokens";
                    return false;
                }

                Token token;
                token.column = index + 1;
                if (IsDigit(character))
                {
                    if (!TokenizeNumber(source, index, token, errorMessage))
                    {
                        return false;
                    }
                }
                else if (character == '\'')
                {
                    if (!TokenizeString(source, index, token, errorMessage))
                    {
                        return false;
                    }
                }
                else if (IsIdentifierStart(character))
                {
                    const std::size_t start = index;
                    while (index < source.size() && IsIdentifierPart(source[index]))
                    {
                        ++index;
                    }
                    token.kind = Token::Kind::Identifier;
                    token.text = source.substr(start, index - start);
                }
                else if (character == '(')
                {
                    token.kind = Token::Kind::LeftParen;
                    ++index;
                }
                else if (character == ')')
                {
                    token.kind = Token::Kind::RightParen;
                    ++index;
                }
                else
                {
                    static constexpr const char* kTwoCharacterOperators[] = {"&&", "||", "==", "!=", "<=", ">="};
                    bool matched = false;
                    for (const char* candidate : kTwoCharacterOperators)
                    {
                        if (source.compare(index, 2, candidate) == 0)
                        {
                            token.kind = Token::Kind::Operator;
                            token.text = candidate;
                            index += 2;
                            matched = true;
                            break;
                        }
                    }
                    if (!matched)
                    {
                        if (character == '&' || character == '|')
                        {
                            errorMessage = std::string("single '") + character +
                                           "' is not an operator; use '&&' or '||'" + At(index + 1);
                            return false;
                        }
                        static constexpr const char* kSingleCharacterOperators = "!<>+-*/";
                        bool isSingle = false;
                        for (const char* scan = kSingleCharacterOperators; *scan != '\0'; ++scan)
                        {
                            isSingle = isSingle || *scan == character;
                        }
                        if (!isSingle)
                        {
                            errorMessage = std::string("unexpected character '") + character + "'" + At(index + 1);
                            return false;
                        }
                        token.kind = Token::Kind::Operator;
                        token.text = std::string(1, character);
                        ++index;
                    }
                }
                tokens.push_back(std::move(token));
            }

            if (tokens.size() >= kMissionExpressionMaxTokens)
            {
                errorMessage = "expression has more than " + std::to_string(kMissionExpressionMaxTokens) + " tokens";
                return false;
            }
            Token end;
            end.kind = Token::Kind::End;
            end.column = source.size() + 1;
            tokens.push_back(std::move(end));
            return true;
        }

        bool IsNumericType(MissionValueType type)
        {
            return type == MissionValueType::Int || type == MissionValueType::Float;
        }
    }

    // Recursive-descent parser over the token stream. Depth is passed down and capped, so a
    // deeply nested mission expression fails to compile instead of exhausting the stack.
    class MissionExpressionParser final
    {
    public:
        MissionExpressionParser(const std::vector<Token>& tokens,
                                const MissionContext& context,
                                MissionExpression& target)
            : tokens_(tokens), context_(context), target_(target)
        {
        }

        [[nodiscard]] bool Parse(std::string& errorMessage)
        {
            const int root = ParseOr(0, errorMessage);
            if (root < 0)
            {
                return false;
            }
            if (Current().kind != Token::Kind::End)
            {
                errorMessage = "unexpected trailing input" + At(Current().column);
                return false;
            }
            target_.root_ = root;
            target_.resultType_ = target_.nodes_[static_cast<std::size_t>(root)].type;
            return true;
        }

    private:
        using Node = MissionExpression::Node;
        using NodeKind = MissionExpression::NodeKind;

        [[nodiscard]] const Token& Current() const { return tokens_[position_]; }

        bool MatchOperator(const char* text)
        {
            if (Current().kind == Token::Kind::Operator && Current().text == text)
            {
                ++position_;
                return true;
            }
            return false;
        }

        [[nodiscard]] int AddNode(Node node, std::string& errorMessage)
        {
            if (target_.nodes_.size() >= kMissionExpressionMaxNodes)
            {
                errorMessage = "expression has more than " + std::to_string(kMissionExpressionMaxNodes) +
                               " operations";
                return -1;
            }
            target_.nodes_.push_back(std::move(node));
            return static_cast<int>(target_.nodes_.size()) - 1;
        }

        [[nodiscard]] MissionValueType TypeOf(int index) const
        {
            return target_.nodes_[static_cast<std::size_t>(index)].type;
        }

        [[nodiscard]] bool CheckDepth(std::size_t depth, std::string& errorMessage) const
        {
            if (depth > kMissionExpressionMaxDepth)
            {
                errorMessage = "expression nests deeper than the limit of " +
                               std::to_string(kMissionExpressionMaxDepth);
                return false;
            }
            return true;
        }

        [[nodiscard]] int ParseOr(std::size_t depth, std::string& errorMessage)
        {
            if (!CheckDepth(depth, errorMessage))
            {
                return -1;
            }
            int left = ParseAnd(depth + 1, errorMessage);
            while (left >= 0)
            {
                const std::size_t column = Current().column;
                if (!MatchOperator("||"))
                {
                    break;
                }
                const int right = ParseAnd(depth + 1, errorMessage);
                if (right < 0)
                {
                    return -1;
                }
                left = MakeLogical(NodeKind::Or, "||", left, right, column, errorMessage);
            }
            return left;
        }

        [[nodiscard]] int ParseAnd(std::size_t depth, std::string& errorMessage)
        {
            if (!CheckDepth(depth, errorMessage))
            {
                return -1;
            }
            int left = ParseComparison(depth + 1, errorMessage);
            while (left >= 0)
            {
                const std::size_t column = Current().column;
                if (!MatchOperator("&&"))
                {
                    break;
                }
                const int right = ParseComparison(depth + 1, errorMessage);
                if (right < 0)
                {
                    return -1;
                }
                left = MakeLogical(NodeKind::And, "&&", left, right, column, errorMessage);
            }
            return left;
        }

        [[nodiscard]] int MakeLogical(NodeKind kind,
                                      const char* text,
                                      int left,
                                      int right,
                                      std::size_t column,
                                      std::string& errorMessage)
        {
            if (TypeOf(left) != MissionValueType::Bool || TypeOf(right) != MissionValueType::Bool)
            {
                errorMessage = std::string("'") + text + "' needs bool operands, got " +
                               MissionValueTypeName(TypeOf(left)) + " and " +
                               MissionValueTypeName(TypeOf(right)) + At(column);
                return -1;
            }
            Node node;
            node.kind = kind;
            node.type = MissionValueType::Bool;
            node.left = left;
            node.right = right;
            return AddNode(std::move(node), errorMessage);
        }

        [[nodiscard]] int ParseComparison(std::size_t depth, std::string& errorMessage)
        {
            if (!CheckDepth(depth, errorMessage))
            {
                return -1;
            }
            const int left = ParseSum(depth + 1, errorMessage);
            if (left < 0)
            {
                return -1;
            }
            if (Current().kind != Token::Kind::Operator)
            {
                return left;
            }

            NodeKind kind = NodeKind::Equal;
            const std::string& text = Current().text;
            if (text == "==") { kind = NodeKind::Equal; }
            else if (text == "!=") { kind = NodeKind::NotEqual; }
            else if (text == "<") { kind = NodeKind::Less; }
            else if (text == "<=") { kind = NodeKind::LessOrEqual; }
            else if (text == ">") { kind = NodeKind::Greater; }
            else if (text == ">=") { kind = NodeKind::GreaterOrEqual; }
            else { return left; }

            const std::string operatorText = text;
            const std::size_t column = Current().column;
            ++position_;
            const int right = ParseSum(depth + 1, errorMessage);
            if (right < 0)
            {
                return -1;
            }
            if (Current().kind == Token::Kind::Operator &&
                (Current().text == "==" || Current().text == "!=" || Current().text == "<" ||
                 Current().text == "<=" || Current().text == ">" || Current().text == ">="))
            {
                errorMessage = "chained comparison is not supported; use '&&' between two comparisons" +
                               At(Current().column);
                return -1;
            }

            const MissionValueType leftType = TypeOf(left);
            const MissionValueType rightType = TypeOf(right);
            const bool ordering = kind != NodeKind::Equal && kind != NodeKind::NotEqual;
            const bool numeric = IsNumericType(leftType) && IsNumericType(rightType);
            const bool sameNonNumeric = !ordering && leftType == rightType && !IsNumericType(leftType);
            if (!(numeric || sameNonNumeric))
            {
                errorMessage = "'" + operatorText + "' cannot compare " + MissionValueTypeName(leftType) +
                               " with " + MissionValueTypeName(rightType) + At(column);
                return -1;
            }

            Node node;
            node.kind = kind;
            node.type = MissionValueType::Bool;
            node.left = left;
            node.right = right;
            return AddNode(std::move(node), errorMessage);
        }

        [[nodiscard]] int ParseSum(std::size_t depth, std::string& errorMessage)
        {
            if (!CheckDepth(depth, errorMessage))
            {
                return -1;
            }
            int left = ParseProduct(depth + 1, errorMessage);
            while (left >= 0)
            {
                const std::size_t column = Current().column;
                NodeKind kind = NodeKind::Add;
                const char* text = "+";
                if (MatchOperator("+")) { kind = NodeKind::Add; text = "+"; }
                else if (MatchOperator("-")) { kind = NodeKind::Subtract; text = "-"; }
                else { break; }

                const int right = ParseProduct(depth + 1, errorMessage);
                if (right < 0)
                {
                    return -1;
                }
                left = MakeArithmetic(kind, text, left, right, column, errorMessage);
            }
            return left;
        }

        [[nodiscard]] int ParseProduct(std::size_t depth, std::string& errorMessage)
        {
            if (!CheckDepth(depth, errorMessage))
            {
                return -1;
            }
            int left = ParseUnary(depth + 1, errorMessage);
            while (left >= 0)
            {
                const std::size_t column = Current().column;
                NodeKind kind = NodeKind::Multiply;
                const char* text = "*";
                if (MatchOperator("*")) { kind = NodeKind::Multiply; text = "*"; }
                else if (MatchOperator("/")) { kind = NodeKind::Divide; text = "/"; }
                else { break; }

                const int right = ParseUnary(depth + 1, errorMessage);
                if (right < 0)
                {
                    return -1;
                }
                left = MakeArithmetic(kind, text, left, right, column, errorMessage);
            }
            return left;
        }

        [[nodiscard]] int MakeArithmetic(NodeKind kind,
                                         const char* text,
                                         int left,
                                         int right,
                                         std::size_t column,
                                         std::string& errorMessage)
        {
            const MissionValueType leftType = TypeOf(left);
            const MissionValueType rightType = TypeOf(right);
            if (!IsNumericType(leftType) || !IsNumericType(rightType))
            {
                errorMessage = std::string("'") + text + "' needs int/float operands, got " +
                               MissionValueTypeName(leftType) + " and " + MissionValueTypeName(rightType) +
                               At(column);
                return -1;
            }
            Node node;
            node.kind = kind;
            node.type = (leftType == MissionValueType::Float || rightType == MissionValueType::Float)
                            ? MissionValueType::Float
                            : MissionValueType::Int;
            node.left = left;
            node.right = right;
            return AddNode(std::move(node), errorMessage);
        }

        [[nodiscard]] int ParseUnary(std::size_t depth, std::string& errorMessage)
        {
            if (!CheckDepth(depth, errorMessage))
            {
                return -1;
            }
            const std::size_t column = Current().column;
            if (MatchOperator("!"))
            {
                const int operand = ParseUnary(depth + 1, errorMessage);
                if (operand < 0)
                {
                    return -1;
                }
                if (TypeOf(operand) != MissionValueType::Bool)
                {
                    errorMessage = std::string("'!' needs a bool operand, got ") +
                                   MissionValueTypeName(TypeOf(operand)) + At(column);
                    return -1;
                }
                Node node;
                node.kind = NodeKind::Not;
                node.type = MissionValueType::Bool;
                node.left = operand;
                return AddNode(std::move(node), errorMessage);
            }
            if (MatchOperator("-"))
            {
                const int operand = ParseUnary(depth + 1, errorMessage);
                if (operand < 0)
                {
                    return -1;
                }
                if (!IsNumericType(TypeOf(operand)))
                {
                    errorMessage = std::string("unary '-' needs an int/float operand, got ") +
                                   MissionValueTypeName(TypeOf(operand)) + At(column);
                    return -1;
                }
                Node node;
                node.kind = NodeKind::Negate;
                node.type = TypeOf(operand);
                node.left = operand;
                return AddNode(std::move(node), errorMessage);
            }
            return ParsePrimary(depth + 1, errorMessage);
        }

        [[nodiscard]] int ParsePrimary(std::size_t depth, std::string& errorMessage)
        {
            if (!CheckDepth(depth, errorMessage))
            {
                return -1;
            }
            const Token& token = Current();
            switch (token.kind)
            {
                case Token::Kind::Number:
                case Token::Kind::String:
                {
                    Node node;
                    node.kind = NodeKind::Literal;
                    node.literal = token.value;
                    node.type = token.value.GetType();
                    ++position_;
                    return AddNode(std::move(node), errorMessage);
                }
                case Token::Kind::Identifier:
                {
                    Node node;
                    if (token.text == "true" || token.text == "false")
                    {
                        node.kind = NodeKind::Literal;
                        node.literal = MissionValue::Bool(token.text == "true");
                        node.type = MissionValueType::Bool;
                        ++position_;
                        return AddNode(std::move(node), errorMessage);
                    }
                    MissionValueType type{};
                    if (!context_.TryGetType(token.text, type))
                    {
                        errorMessage = "unknown identifier \"" + token.text +
                                       "\" (not a declared mission fact or variable)" + At(token.column);
                        return -1;
                    }
                    node.kind = NodeKind::Symbol;
                    node.name = token.text;
                    node.type = type;
                    ++position_;
                    return AddNode(std::move(node), errorMessage);
                }
                case Token::Kind::LeftParen:
                {
                    ++position_;
                    const int inner = ParseOr(depth + 1, errorMessage);
                    if (inner < 0)
                    {
                        return -1;
                    }
                    if (Current().kind != Token::Kind::RightParen)
                    {
                        errorMessage = "expected ')'" + At(Current().column);
                        return -1;
                    }
                    ++position_;
                    return inner;
                }
                case Token::Kind::RightParen:
                    errorMessage = "unexpected ')'" + At(token.column);
                    return -1;
                case Token::Kind::Operator:
                    errorMessage = "expected a value before '" + token.text + "'" + At(token.column);
                    return -1;
                case Token::Kind::End:
                    errorMessage = "expression ends unexpectedly" + At(token.column);
                    return -1;
            }
            errorMessage = "malformed expression" + At(token.column);
            return -1;
        }

        const std::vector<Token>& tokens_;
        const MissionContext& context_;
        MissionExpression& target_;
        std::size_t position_{0};
    };

    bool MissionExpression::Compile(const std::string& source,
                                    const MissionContext& context,
                                    MissionExpression& out,
                                    std::string& errorMessage)
    {
        MissionExpression compiled;
        compiled.source_ = source;

        std::vector<Token> tokens;
        if (!Tokenize(source, tokens, errorMessage))
        {
            return false;
        }
        if (tokens.size() == 1) // only the End token
        {
            errorMessage = "expression is empty";
            return false;
        }

        MissionExpressionParser parser(tokens, context, compiled);
        if (!parser.Parse(errorMessage))
        {
            return false;
        }

        out = std::move(compiled);
        return true;
    }

    bool MissionExpression::Evaluate(const MissionContext& context,
                                     MissionValue& out,
                                     std::string& errorMessage) const
    {
        if (root_ < 0)
        {
            errorMessage = "expression is empty";
            return false;
        }
        std::size_t steps = 0;
        return EvaluateNode(root_, context, steps, out, errorMessage);
    }

    bool MissionExpression::EvaluateBool(const MissionContext& context,
                                         bool& out,
                                         std::string& errorMessage) const
    {
        if (root_ < 0)
        {
            out = false;
            return true;
        }
        if (resultType_ != MissionValueType::Bool)
        {
            errorMessage = std::string("expression \"") + source_ + "\" evaluates to " +
                           MissionValueTypeName(resultType_) + ", not bool";
            return false;
        }
        MissionValue value;
        if (!Evaluate(context, value, errorMessage))
        {
            return false;
        }
        out = value.AsBool();
        return true;
    }

    bool MissionExpression::EvaluateNode(int index,
                                         const MissionContext& context,
                                         std::size_t& steps,
                                         MissionValue& out,
                                         std::string& errorMessage) const
    {
        ++steps;
        if (steps > kMissionExpressionMaxSteps)
        {
            errorMessage = "expression \"" + source_ + "\" exceeded " +
                           std::to_string(kMissionExpressionMaxSteps) + " evaluation steps";
            return false;
        }

        const Node& node = nodes_[static_cast<std::size_t>(index)];
        switch (node.kind)
        {
            case NodeKind::Literal:
                out = node.literal;
                return true;
            case NodeKind::Symbol:
            {
                MissionValue value;
                if (!context.TryGetValue(node.name, value))
                {
                    errorMessage = "mission symbol \"" + node.name +
                                   "\" is not declared in the context this expression is evaluated against";
                    return false;
                }
                if (value.GetType() != node.type)
                {
                    errorMessage = "mission symbol \"" + node.name + "\" changed type from " +
                                   MissionValueTypeName(node.type) + " to " +
                                   MissionValueTypeName(value.GetType()) + " after compilation";
                    return false;
                }
                out = std::move(value);
                return true;
            }
            case NodeKind::Not:
            {
                MissionValue operand;
                if (!EvaluateNode(node.left, context, steps, operand, errorMessage))
                {
                    return false;
                }
                out = MissionValue::Bool(!operand.AsBool());
                return true;
            }
            case NodeKind::Negate:
            {
                MissionValue operand;
                if (!EvaluateNode(node.left, context, steps, operand, errorMessage))
                {
                    return false;
                }
                out = node.type == MissionValueType::Int ? MissionValue::Int(-operand.AsInt())
                                                         : MissionValue::Float(-operand.AsFloat());
                return true;
            }
            case NodeKind::And:
            case NodeKind::Or:
            {
                MissionValue left;
                if (!EvaluateNode(node.left, context, steps, left, errorMessage))
                {
                    return false;
                }
                // Short-circuit: the right operand is not evaluated (and does not consume steps)
                // when the left one already decides the result.
                if (node.kind == NodeKind::And && !left.AsBool())
                {
                    out = MissionValue::Bool(false);
                    return true;
                }
                if (node.kind == NodeKind::Or && left.AsBool())
                {
                    out = MissionValue::Bool(true);
                    return true;
                }
                MissionValue right;
                if (!EvaluateNode(node.right, context, steps, right, errorMessage))
                {
                    return false;
                }
                out = MissionValue::Bool(right.AsBool());
                return true;
            }
            default:
                break;
        }

        MissionValue left;
        MissionValue right;
        if (!EvaluateNode(node.left, context, steps, left, errorMessage) ||
            !EvaluateNode(node.right, context, steps, right, errorMessage))
        {
            return false;
        }

        switch (node.kind)
        {
            case NodeKind::Add:
            case NodeKind::Subtract:
            case NodeKind::Multiply:
            case NodeKind::Divide:
            {
                if (node.kind == NodeKind::Divide)
                {
                    const bool dividesByZero = node.type == MissionValueType::Int
                                                   ? right.AsInt() == 0
                                                   : right.AsFloat() == 0.0F;
                    if (dividesByZero)
                    {
                        errorMessage = "expression \"" + source_ + "\" divides by zero";
                        return false;
                    }
                }
                if (node.type == MissionValueType::Int)
                {
                    const int a = left.AsInt();
                    const int b = right.AsInt();
                    switch (node.kind)
                    {
                        case NodeKind::Add: out = MissionValue::Int(a + b); break;
                        case NodeKind::Subtract: out = MissionValue::Int(a - b); break;
                        case NodeKind::Multiply: out = MissionValue::Int(a * b); break;
                        default: out = MissionValue::Int(a / b); break;
                    }
                    return true;
                }
                const float a = left.AsFloat();
                const float b = right.AsFloat();
                switch (node.kind)
                {
                    case NodeKind::Add: out = MissionValue::Float(a + b); break;
                    case NodeKind::Subtract: out = MissionValue::Float(a - b); break;
                    case NodeKind::Multiply: out = MissionValue::Float(a * b); break;
                    default: out = MissionValue::Float(a / b); break;
                }
                return true;
            }
            case NodeKind::Less:
                out = MissionValue::Bool(left.AsFloat() < right.AsFloat());
                return true;
            case NodeKind::LessOrEqual:
                out = MissionValue::Bool(left.AsFloat() <= right.AsFloat());
                return true;
            case NodeKind::Greater:
                out = MissionValue::Bool(left.AsFloat() > right.AsFloat());
                return true;
            case NodeKind::GreaterOrEqual:
                out = MissionValue::Bool(left.AsFloat() >= right.AsFloat());
                return true;
            case NodeKind::Equal:
            case NodeKind::NotEqual:
            {
                bool equal = false;
                switch (left.GetType())
                {
                    case MissionValueType::Bool:
                        equal = left.AsBool() == right.AsBool();
                        break;
                    case MissionValueType::String:
                        equal = left.AsString() == right.AsString();
                        break;
                    case MissionValueType::Int:
                    case MissionValueType::Float:
                        equal = left.AsFloat() == right.AsFloat();
                        break;
                }
                out = MissionValue::Bool(node.kind == NodeKind::Equal ? equal : !equal);
                return true;
            }
            default:
                break;
        }

        errorMessage = "expression \"" + source_ + "\" contains an unsupported operation";
        return false;
    }
}
