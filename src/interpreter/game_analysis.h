#ifndef GAMELANG_GAME_ANALYSIS_H
#define GAMELANG_GAME_ANALYSIS_H

#include "runtime_value.h"

#include <cstddef>
#include <string>
#include <vector>

namespace GameLang {

struct PayoffPair {
    double first = 0.0;
    double second = 0.0;
    bool ok = false;
};

std::string playerName(const RuntimeValue& player, size_t fallbackIndex);
RuntimeList gamePlayers(const RuntimeValue& game);
std::vector<std::string> strategiesForPlayer(const RuntimeValue& game, size_t playerIndex);
PayoffPair payoffAt(const RuntimeValue& game, size_t row, size_t col);

RuntimeValue validateGameValue(const RuntimeValue& game);
RuntimeValue solvePureNash(const RuntimeValue& game);
RuntimeValue dominatedStrategies(const RuntimeValue& game);
RuntimeValue dominantStrategies(const RuntimeValue& game);
RuntimeValue priceOfAnarchy(const RuntimeValue& game);

} // namespace GameLang

#endif // GAMELANG_GAME_ANALYSIS_H
