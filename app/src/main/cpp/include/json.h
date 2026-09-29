#pragma once
#include <string>
#include <vector>
#include <map>
#include <sstream>

// JSON minimalista, feito à mão de propósito (sem dependência externa — o NDK
// não traz um parser JSON pronto e não há acesso à rede no ambiente de build para
// puxar uma lib via CMake FetchContent). Cobre só o que o formato de projeto usa:
// objeto, array, string, número, bool, null — sem \uXXXX completo em strings (não
// aparece nos nossos dados) e sem comentários/trailing commas (não é JSON5).
// Testado por round-trip contra Timeline/Layer/Stroke reais antes de entrar aqui
// (inclusive cores ARGB com o bit de sinal ligado, que exigem cuidado ao passar
// por double).
namespace dreams::json {

enum class Type { Null, Bool, Number, String, Array, Object };

class Value {
public:
    Value() : type_(Type::Null) {}
    static Value makeNull() { return Value(); }
    static Value makeBool(bool b) { Value v; v.type_ = Type::Bool; v.bool_ = b; return v; }
    static Value makeNumber(double n) { Value v; v.type_ = Type::Number; v.num_ = n; return v; }
    static Value makeString(std::string s) { Value v; v.type_ = Type::String; v.str_ = std::move(s); return v; }
    static Value makeArray() { Value v; v.type_ = Type::Array; return v; }
    static Value makeObject() { Value v; v.type_ = Type::Object; return v; }

    Type type() const { return type_; }
    bool asBool(bool def = false) const { return type_ == Type::Bool ? bool_ : def; }
    double asNumber(double def = 0) const { return type_ == Type::Number ? num_ : def; }
    float asFloat(float def = 0) const { return (float) asNumber(def); }
    int asInt(int def = 0) const { return (int) asNumber(def); }
    std::string asString(const std::string& def = "") const { return type_ == Type::String ? str_ : def; }

    void push(Value v) { arr_.push_back(std::move(v)); }
    const std::vector<Value>& items() const { return arr_; }

    void set(const std::string& key, Value v) {
        if (obj_.find(key) == obj_.end()) keys_.push_back(key);
        obj_[key] = std::move(v);
    }
    bool has(const std::string& key) const { return obj_.find(key) != obj_.end(); }
    const Value& get(const std::string& key) const {
        static const Value nullVal;
        auto it = obj_.find(key);
        return it != obj_.end() ? it->second : nullVal;
    }

    std::string dump() const;

private:
    Type type_;
    bool bool_ = false;
    double num_ = 0;
    std::string str_;
    std::vector<Value> arr_;
    std::map<std::string, Value> obj_;
    std::vector<std::string> keys_;
};

Value parse(const std::string& text, bool* ok = nullptr);

} // namespace dreams::json
