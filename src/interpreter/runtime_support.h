#ifndef GAMELANG_RUNTIME_SUPPORT_H
#define GAMELANG_RUNTIME_SUPPORT_H

#include "runtime_value.h"

#include <initializer_list>
#include <string>
#include <vector>

namespace GameLang {

inline constexpr double EPSILON = 1e-9;

RuntimeList asList(const RuntimeValue& value, const std::string& context);
RuntimeObject asObject(const RuntimeValue& value, const std::string& context);
double asNumber(const RuntimeValue& value, const std::string& context);
std::string asString(const RuntimeValue& value);

const RuntimeValue* objectGet(const RuntimeValue& value, const std::string& key);
RuntimeValue objectGetOrNil(const RuntimeValue& value, const std::string& key);
std::string objectStringField(
    const RuntimeValue& value,
    const std::string& key,
    const std::string& fallback = "");
RuntimeList objectListField(const RuntimeValue& value, const std::string& key);

bool valuesEqual(const RuntimeValue& left, const RuntimeValue& right);

RuntimeValue makeTaggedObject(const std::string& type, RuntimeObject object = {});
RuntimeList stringListToValues(const std::vector<std::string>& values);

std::vector<CallArg> positionalArgs(std::initializer_list<RuntimeValue> values);
const RuntimeValue* findNamedArg(const std::vector<CallArg>& args, const std::string& name);
RuntimeValue argAt(
    const std::vector<CallArg>& args,
    size_t index,
    const RuntimeValue& fallback = RuntimeValue());
RuntimeValue argNamedOrAt(
    const std::vector<CallArg>& args,
    const std::string& name,
    size_t index,
    const RuntimeValue& fallback = RuntimeValue());

} // namespace GameLang

#endif // GAMELANG_RUNTIME_SUPPORT_H
