#include "runtime_support.h"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace GameLang {

RuntimeList asList(const RuntimeValue& value, const std::string& context) {
    if (!value.isList()) {
        throw std::runtime_error(context + " expected a list");
    }
    return *std::get<RuntimeValue::ListPtr>(value.data);
}

RuntimeObject asObject(const RuntimeValue& value, const std::string& context) {
    if (!value.isObject()) {
        throw std::runtime_error(context + " expected an object");
    }
    return *std::get<RuntimeValue::ObjectPtr>(value.data);
}

double asNumber(const RuntimeValue& value, const std::string& context) {
    if (!value.isNumber()) {
        throw std::runtime_error(context + " expected a number");
    }
    return std::get<double>(value.data);
}

std::string asString(const RuntimeValue& value) {
    if (value.isString()) return std::get<std::string>(value.data);
    if (value.isNumber()) {
        std::ostringstream out;
        out << std::setprecision(12) << std::get<double>(value.data);
        std::string text = out.str();
        if (text.find('.') != std::string::npos) {
            while (!text.empty() && text.back() == '0') text.pop_back();
            if (!text.empty() && text.back() == '.') text.pop_back();
        }
        return text;
    }
    if (value.isBool()) return std::get<bool>(value.data) ? "T" : "F";
    if (value.isNil()) return "nil";
    return valueToString(value);
}

const RuntimeValue* objectGet(const RuntimeValue& value, const std::string& key) {
    if (!value.isObject()) return nullptr;
    const auto& object = *std::get<RuntimeValue::ObjectPtr>(value.data);
    auto found = object.find(key);
    if (found == object.end()) return nullptr;
    return &found->second;
}

RuntimeValue objectGetOrNil(const RuntimeValue& value, const std::string& key) {
    const RuntimeValue* found = objectGet(value, key);
    return found ? *found : RuntimeValue();
}

std::string objectStringField(const RuntimeValue& value, const std::string& key, const std::string& fallback) {
    const RuntimeValue* found = objectGet(value, key);
    if (!found) return fallback;
    return asString(*found);
}

RuntimeList objectListField(const RuntimeValue& value, const std::string& key) {
    const RuntimeValue* found = objectGet(value, key);
    if (!found || !found->isList()) return {};
    return *std::get<RuntimeValue::ListPtr>(found->data);
}

bool valuesEqual(const RuntimeValue& left, const RuntimeValue& right) {
    if (left.isNumber() && right.isNumber()) {
        return std::abs(std::get<double>(left.data) - std::get<double>(right.data)) < EPSILON;
    }
    if (left.data.index() != right.data.index()) return false;
    if (left.isNil()) return true;
    if (left.isString()) return std::get<std::string>(left.data) == std::get<std::string>(right.data);
    if (left.isBool()) return std::get<bool>(left.data) == std::get<bool>(right.data);
    if (left.isList()) {
        const auto& leftList = *std::get<RuntimeValue::ListPtr>(left.data);
        const auto& rightList = *std::get<RuntimeValue::ListPtr>(right.data);
        if (leftList.size() != rightList.size()) return false;
        for (size_t i = 0; i < leftList.size(); ++i) {
            if (!valuesEqual(leftList[i], rightList[i])) return false;
        }
        return true;
    }
    if (left.isObject()) {
        const auto& leftObject = *std::get<RuntimeValue::ObjectPtr>(left.data);
        const auto& rightObject = *std::get<RuntimeValue::ObjectPtr>(right.data);
        if (leftObject.size() != rightObject.size()) return false;
        for (const auto& [key, value] : leftObject) {
            auto found = rightObject.find(key);
            if (found == rightObject.end() || !valuesEqual(value, found->second)) return false;
        }
        return true;
    }
    return false;
}

RuntimeValue makeTaggedObject(const std::string& type, RuntimeObject object) {
    object["__type"] = RuntimeValue(type);
    return RuntimeValue::object(object);
}

RuntimeList stringListToValues(const std::vector<std::string>& values) {
    RuntimeList result;
    for (const std::string& value : values) result.emplace_back(value);
    return result;
}

std::vector<CallArg> positionalArgs(std::initializer_list<RuntimeValue> values) {
    std::vector<CallArg> args;
    for (const RuntimeValue& value : values) {
        args.push_back({"", value});
    }
    return args;
}

const RuntimeValue* findNamedArg(const std::vector<CallArg>& args, const std::string& name) {
    for (const CallArg& arg : args) {
        if (arg.name == name) return &arg.value;
    }
    return nullptr;
}

RuntimeValue argAt(const std::vector<CallArg>& args, size_t index, const RuntimeValue& fallback) {
    size_t seen = 0;
    for (const CallArg& arg : args) {
        if (!arg.name.empty()) continue;
        if (seen == index) return arg.value;
        ++seen;
    }
    return fallback;
}

RuntimeValue argNamedOrAt(
    const std::vector<CallArg>& args,
    const std::string& name,
    size_t index,
    const RuntimeValue& fallback) {
    const RuntimeValue* named = findNamedArg(args, name);
    return named ? *named : argAt(args, index, fallback);
}

} // namespace GameLang
