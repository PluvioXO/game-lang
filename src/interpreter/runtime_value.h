#ifndef GAMELANG_RUNTIME_VALUE_H
#define GAMELANG_RUNTIME_VALUE_H

#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace GameLang {

struct RuntimeValue;

using RuntimeList = std::vector<RuntimeValue>;
using RuntimeObject = std::map<std::string, RuntimeValue>;

struct RuntimeValue {
    using ListPtr = std::shared_ptr<RuntimeList>;
    using ObjectPtr = std::shared_ptr<RuntimeObject>;

    std::variant<std::nullptr_t, double, std::string, bool, ListPtr, ObjectPtr> data;

    RuntimeValue();
    RuntimeValue(std::nullptr_t);
    RuntimeValue(double value);
    RuntimeValue(int value);
    RuntimeValue(const std::string& value);
    RuntimeValue(const char* value);
    RuntimeValue(bool value);

    static RuntimeValue list(const RuntimeList& values);
    static RuntimeValue object(const RuntimeObject& values);

    bool isNil() const;
    bool isNumber() const;
    bool isString() const;
    bool isBool() const;
    bool isList() const;
    bool isObject() const;
};

struct CallArg {
    std::string name;
    RuntimeValue value;
};

bool isTruthy(const RuntimeValue& value);
std::string objectType(const RuntimeValue& value);
std::string valueToString(const RuntimeValue& value);
std::string valueToJson(const RuntimeValue& value, int indent = 0);

} // namespace GameLang

#endif // GAMELANG_RUNTIME_VALUE_H
