#include "runtime_value.h"

#include <cmath>
#include <iomanip>
#include <sstream>

namespace GameLang {
namespace {

std::string formatNumber(double value) {
    std::ostringstream out;
    out << std::setprecision(12) << value;
    std::string text = out.str();
    if (text.find('.') != std::string::npos) {
        while (!text.empty() && text.back() == '0') text.pop_back();
        if (!text.empty() && text.back() == '.') text.pop_back();
    }
    return text;
}

std::string escapeJsonString(const std::string& text) {
    std::ostringstream out;
    out << "\"";
    for (char c : text) {
        switch (c) {
            case '\\': out << "\\\\"; break;
            case '"': out << "\\\""; break;
            case '\n': out << "\\n"; break;
            case '\t': out << "\\t"; break;
            default: out << c; break;
        }
    }
    out << "\"";
    return out.str();
}

} // namespace

RuntimeValue::RuntimeValue() : data(nullptr) {}
RuntimeValue::RuntimeValue(std::nullptr_t) : data(nullptr) {}
RuntimeValue::RuntimeValue(double value) : data(value) {}
RuntimeValue::RuntimeValue(int value) : data(static_cast<double>(value)) {}
RuntimeValue::RuntimeValue(const std::string& value) : data(value) {}
RuntimeValue::RuntimeValue(const char* value) : data(std::string(value)) {}
RuntimeValue::RuntimeValue(bool value) : data(value) {}

RuntimeValue RuntimeValue::list(const RuntimeList& values) {
    RuntimeValue value;
    value.data = std::make_shared<RuntimeList>(values);
    return value;
}

RuntimeValue RuntimeValue::object(const RuntimeObject& values) {
    RuntimeValue value;
    value.data = std::make_shared<RuntimeObject>(values);
    return value;
}

RuntimeValue RuntimeValue::lambda(const std::vector<std::string>& parameters, const std::vector<Token>& body) {
    RuntimeValue value;
    value.data = std::make_shared<RuntimeLambda>(parameters, body);
    return value;
}

bool RuntimeValue::isNil() const { return std::holds_alternative<std::nullptr_t>(data); }
bool RuntimeValue::isNumber() const { return std::holds_alternative<double>(data); }
bool RuntimeValue::isString() const { return std::holds_alternative<std::string>(data); }
bool RuntimeValue::isBool() const { return std::holds_alternative<bool>(data); }
bool RuntimeValue::isList() const { return std::holds_alternative<ListPtr>(data); }
bool RuntimeValue::isObject() const { return std::holds_alternative<ObjectPtr>(data); }
bool RuntimeValue::isLambda() const { return std::holds_alternative<LambdaPtr>(data); }

bool isTruthy(const RuntimeValue& value) {
    if (value.isNil()) return false;
    if (value.isBool()) return std::get<bool>(value.data);
    if (value.isNumber()) return std::abs(std::get<double>(value.data)) > 1e-9;
    if (value.isString()) return !std::get<std::string>(value.data).empty();
    if (value.isList()) return !std::get<RuntimeValue::ListPtr>(value.data)->empty();
    if (value.isObject()) return !std::get<RuntimeValue::ObjectPtr>(value.data)->empty();
    if (value.isLambda()) return true;
    return false;
}

std::string objectType(const RuntimeValue& value) {
    if (!value.isObject()) return "";
    const auto& object = *std::get<RuntimeValue::ObjectPtr>(value.data);
    auto type = object.find("__type");
    if (type == object.end() || !type->second.isString()) return "";
    return std::get<std::string>(type->second.data);
}

std::string valueToString(const RuntimeValue& value) {
    if (value.isNil()) return "nil";
    if (value.isNumber()) return formatNumber(std::get<double>(value.data));
    if (value.isString()) return std::get<std::string>(value.data);
    if (value.isBool()) return std::get<bool>(value.data) ? "T" : "F";
    if (value.isList()) {
        std::ostringstream out;
        out << "[";
        const auto& list = *std::get<RuntimeValue::ListPtr>(value.data);
        for (size_t i = 0; i < list.size(); ++i) {
            if (i > 0) out << ", ";
            out << valueToString(list[i]);
        }
        out << "]";
        return out.str();
    }
    if (value.isObject()) {
        std::ostringstream out;
        const auto& object = *std::get<RuntimeValue::ObjectPtr>(value.data);
        auto type = object.find("__type");
        if (type != object.end()) out << valueToString(type->second) << " ";
        out << "{";
        bool first = true;
        for (const auto& [key, member] : object) {
            if (key == "__type") continue;
            if (!first) out << ", ";
            first = false;
            out << key << ": " << valueToString(member);
        }
        out << "}";
        return out.str();
    }
    if (value.isLambda()) {
        const auto& lambda = *std::get<RuntimeValue::LambdaPtr>(value.data);
        std::ostringstream out;
        out << "<lambda(";
        for (size_t i = 0; i < lambda.parameters.size(); ++i) {
            if (i > 0) out << ", ";
            out << lambda.parameters[i];
        }
        out << ")>";
        return out.str();
    }
    return "nil";
}

std::string valueToJson(const RuntimeValue& value, int indent) {
    std::string pad(static_cast<size_t>(indent), ' ');
    std::string childPad(static_cast<size_t>(indent + 2), ' ');
    if (value.isNil()) return "null";
    if (value.isNumber()) return formatNumber(std::get<double>(value.data));
    if (value.isBool()) return std::get<bool>(value.data) ? "true" : "false";
    if (value.isString()) return escapeJsonString(std::get<std::string>(value.data));
    if (value.isList()) {
        const auto& list = *std::get<RuntimeValue::ListPtr>(value.data);
        if (list.empty()) return "[]";
        std::ostringstream out;
        out << "[\n";
        for (size_t i = 0; i < list.size(); ++i) {
            if (i > 0) out << ",\n";
            out << childPad << valueToJson(list[i], indent + 2);
        }
        out << "\n" << pad << "]";
        return out.str();
    }
    if (value.isObject()) {
        const auto& object = *std::get<RuntimeValue::ObjectPtr>(value.data);
        if (object.empty()) return "{}";
        std::ostringstream out;
        out << "{\n";
        bool first = true;
        for (const auto& [key, member] : object) {
            if (!first) out << ",\n";
            first = false;
            out << childPad << escapeJsonString(key) << ": " << valueToJson(member, indent + 2);
        }
        out << "\n" << pad << "}";
        return out.str();
    }
    if (value.isLambda()) return escapeJsonString(valueToString(value));
    return "null";
}

} // namespace GameLang
