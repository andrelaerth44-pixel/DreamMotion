#include "json.h"
#include <cctype>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <stdexcept>

namespace dreams::json {

static void escapeInto(std::ostringstream& os, const std::string& s) {
    os << '"';
    for (char c : s) {
        switch (c) {
            case '"': os << "\\\""; break;
            case '\\': os << "\\\\"; break;
            case '\n': os << "\\n"; break;
            case '\r': os << "\\r"; break;
            case '\t': os << "\\t"; break;
            default:
                if ((unsigned char) c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char) c);
                    os << buf;
                } else {
                    os << c;
                }
        }
    }
    os << '"';
}

std::string Value::dump() const {
    std::ostringstream os;
    switch (type_) {
        case Type::Null: os << "null"; break;
        case Type::Bool: os << (bool_ ? "true" : "false"); break;
        case Type::Number: {
            double n = num_;
            if (n == (double) (long long) n && std::fabs(n) < 1e15) {
                os << (long long) n;
            } else {
                os << n;
            }
            break;
        }
        case Type::String: escapeInto(os, str_); break;
        case Type::Array: {
            os << '[';
            for (size_t i = 0; i < arr_.size(); ++i) {
                if (i) os << ',';
                os << arr_[i].dump();
            }
            os << ']';
            break;
        }
        case Type::Object: {
            os << '{';
            bool first = true;
            for (const auto& k : keys_) {
                if (!first) os << ',';
                first = false;
                escapeInto(os, k);
                os << ':' << obj_.at(k).dump();
            }
            os << '}';
            break;
        }
    }
    return os.str();
}

namespace {

class Parser {
public:
    explicit Parser(const std::string& s) : s_(s) {}

    Value parseValue() {
        skipWs();
        if (pos_ >= s_.size()) throw std::runtime_error("fim inesperado");
        char c = s_[pos_];
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == '"') return Value::makeString(parseString());
        if (c == 't' || c == 'f') return parseBool();
        if (c == 'n') { expectLiteral("null"); return Value::makeNull(); }
        return parseNumber();
    }

private:
    const std::string& s_;
    size_t pos_ = 0;

    void skipWs() { while (pos_ < s_.size() && std::isspace((unsigned char) s_[pos_])) ++pos_; }

    void expect(char c) {
        skipWs();
        if (pos_ >= s_.size() || s_[pos_] != c) throw std::runtime_error(std::string("esperado '") + c + "'");
        ++pos_;
    }

    void expectLiteral(const char* lit) {
        size_t len = std::strlen(lit);
        if (s_.compare(pos_, len, lit) != 0) throw std::runtime_error("literal invalido");
        pos_ += len;
    }

    Value parseBool() {
        if (s_.compare(pos_, 4, "true") == 0) { pos_ += 4; return Value::makeBool(true); }
        expectLiteral("false");
        return Value::makeBool(false);
    }

    std::string parseString() {
        expect('"');
        std::string out;
        while (pos_ < s_.size() && s_[pos_] != '"') {
            char c = s_[pos_++];
            if (c == '\\' && pos_ < s_.size()) {
                char e = s_[pos_++];
                switch (e) {
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'u': pos_ += 4; out += '?'; break; // \uXXXX nao usado nos nossos dados, aproxima
                    default: out += e;
                }
            } else {
                out += c;
            }
        }
        expect('"');
        return out;
    }

    Value parseNumber() {
        skipWs();
        size_t start = pos_;
        if (pos_ < s_.size() && (s_[pos_] == '-' || s_[pos_] == '+')) ++pos_;
        while (pos_ < s_.size() &&
               (std::isdigit((unsigned char) s_[pos_]) || s_[pos_] == '.' || s_[pos_] == 'e' || s_[pos_] == 'E' ||
                s_[pos_] == '-' || s_[pos_] == '+')) {
            ++pos_;
        }
        std::string numStr = s_.substr(start, pos_ - start);
        if (numStr.empty()) throw std::runtime_error("numero invalido");
        return Value::makeNumber(std::stod(numStr));
    }

    Value parseArray() {
        expect('[');
        Value v = Value::makeArray();
        skipWs();
        if (pos_ < s_.size() && s_[pos_] == ']') { ++pos_; return v; }
        while (true) {
            v.push(parseValue());
            skipWs();
            if (pos_ < s_.size() && s_[pos_] == ',') { ++pos_; continue; }
            break;
        }
        expect(']');
        return v;
    }

    Value parseObject() {
        expect('{');
        Value v = Value::makeObject();
        skipWs();
        if (pos_ < s_.size() && s_[pos_] == '}') { ++pos_; return v; }
        while (true) {
            skipWs();
            std::string key = parseString();
            expect(':');
            Value val = parseValue();
            v.set(key, std::move(val));
            skipWs();
            if (pos_ < s_.size() && s_[pos_] == ',') { ++pos_; continue; }
            break;
        }
        expect('}');
        return v;
    }
};

} // namespace

Value parse(const std::string& text, bool* ok) {
    try {
        Parser p(text);
        Value v = p.parseValue();
        if (ok) *ok = true;
        return v;
    } catch (...) {
        if (ok) *ok = false;
        return Value::makeNull();
    }
}

} // namespace dreams::json
