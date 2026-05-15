#include "game_analysis.h"

#include "runtime_support.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace GameLang {
namespace {

std::vector<std::string> valueListToStrings(const RuntimeValue& value) {
    std::vector<std::string> result;
    if (!value.isList()) return result;
    for (const RuntimeValue& item : *std::get<RuntimeValue::ListPtr>(value.data)) {
        result.push_back(asString(item));
    }
    return result;
}

RuntimeObject gameStrategiesObject(const RuntimeValue& game) {
    const RuntimeValue* strategies = objectGet(game, "strategies");
    if (strategies && strategies->isObject()) return *std::get<RuntimeValue::ObjectPtr>(strategies->data);
    return {};
}

RuntimeValue validationResult(bool valid, const RuntimeList& errors) {
    return makeTaggedObject("validation", {
        {"valid", RuntimeValue(valid)},
        {"errors", RuntimeValue::list(errors)}
    });
}

RuntimeValue makeEquilibrium(
    const RuntimeValue& game,
    size_t row,
    size_t col,
    const std::string& conceptName = "nash") {
    RuntimeList players = gamePlayers(game);
    std::vector<std::string> rowStrategies = strategiesForPlayer(game, 0);
    std::vector<std::string> colStrategies = strategiesForPlayer(game, 1);
    PayoffPair payoff = payoffAt(game, row, col);

    RuntimeObject strategyProfile;
    strategyProfile[playerName(players[0], 0)] = RuntimeValue(rowStrategies[row]);
    strategyProfile[playerName(players[1], 1)] = RuntimeValue(colStrategies[col]);

    return makeTaggedObject("equilibrium", {
        {"concept", RuntimeValue(conceptName)},
        {"strategies", RuntimeValue::object(strategyProfile)},
        {"payoffs", RuntimeValue::list({RuntimeValue(payoff.first), RuntimeValue(payoff.second)})}
    });
}

double socialWelfareAt(const RuntimeValue& game, size_t row, size_t col) {
    PayoffPair payoff = payoffAt(game, row, col);
    return payoff.ok ? payoff.first + payoff.second : 0.0;
}

} // namespace

std::string playerName(const RuntimeValue& player, size_t fallbackIndex) {
    std::string fallback = "P" + std::to_string(fallbackIndex + 1);
    if (player.isObject()) return objectStringField(player, "name", fallback);
    if (player.isString()) return std::get<std::string>(player.data);
    return fallback;
}

RuntimeList gamePlayers(const RuntimeValue& game) {
    return objectListField(game, "players");
}

std::vector<std::string> strategiesForPlayer(const RuntimeValue& game, size_t playerIndex) {
    RuntimeList players = gamePlayers(game);
    if (playerIndex >= players.size()) return {};
    std::string name = playerName(players[playerIndex], playerIndex);

    RuntimeObject strategies = gameStrategiesObject(game);
    auto found = strategies.find(name);
    if (found != strategies.end()) return valueListToStrings(found->second);

    if (players[playerIndex].isObject()) {
        return valueListToStrings(objectGetOrNil(players[playerIndex], "strategies"));
    }
    return {};
}

PayoffPair payoffAt(const RuntimeValue& game, size_t row, size_t col) {
    RuntimeList rows = objectListField(game, "payoffs");
    if (row >= rows.size() || !rows[row].isList()) return {};
    RuntimeList rowValues = *std::get<RuntimeValue::ListPtr>(rows[row].data);
    if (rowValues.empty()) return {};

    if (col < rowValues.size() && rowValues[col].isList()) {
        RuntimeList pair = *std::get<RuntimeValue::ListPtr>(rowValues[col].data);
        if (pair.size() >= 2 && pair[0].isNumber() && pair[1].isNumber()) {
            return {std::get<double>(pair[0].data), std::get<double>(pair[1].data), true};
        }
    }

    size_t flatIndex = col * 2;
    if (flatIndex + 1 < rowValues.size() && rowValues[flatIndex].isNumber() && rowValues[flatIndex + 1].isNumber()) {
        return {std::get<double>(rowValues[flatIndex].data), std::get<double>(rowValues[flatIndex + 1].data), true};
    }
    return {};
}

RuntimeValue validateGameValue(const RuntimeValue& game) {
    RuntimeList errors;
    if (objectType(game) != "game" && objectType(game) != "population_game" && objectType(game) != "extensive_game") {
        errors.emplace_back("value is not a game");
        return validationResult(false, errors);
    }

    RuntimeList players = gamePlayers(game);
    if (players.empty()) errors.emplace_back("game must have at least one player");
    if (players.size() > 2) {
        errors.emplace_back("MVP solvers currently support one- and two-player normal-form games");
    }

    if (players.size() >= 2) {
        std::vector<std::string> rowStrategies = strategiesForPlayer(game, 0);
        std::vector<std::string> colStrategies = strategiesForPlayer(game, 1);
        RuntimeList rows = objectListField(game, "payoffs");
        if (rowStrategies.empty()) errors.emplace_back("first player has no strategies");
        if (colStrategies.empty()) errors.emplace_back("second player has no strategies");
        if (rows.size() != rowStrategies.size()) {
            errors.emplace_back("payoff row count does not match first player's strategy count");
        }
        for (size_t row = 0; row < std::min(rows.size(), rowStrategies.size()); ++row) {
            if (!rows[row].isList()) {
                errors.emplace_back("payoff row " + std::to_string(row) + " is not a list");
                continue;
            }
            RuntimeList rowValues = *std::get<RuntimeValue::ListPtr>(rows[row].data);
            bool nestedPairs = !rowValues.empty() && rowValues.front().isList();
            size_t expected = nestedPairs ? colStrategies.size() : colStrategies.size() * 2;
            if (rowValues.size() != expected) {
                errors.emplace_back("payoff row " + std::to_string(row) + " has wrong width");
            }
            for (size_t col = 0; col < colStrategies.size(); ++col) {
                if (!payoffAt(game, row, col).ok) {
                    errors.emplace_back("missing payoff pair at row " + std::to_string(row) + ", column " + std::to_string(col));
                }
            }
        }
    }

    return validationResult(errors.empty(), errors);
}

RuntimeValue solvePureNash(const RuntimeValue& game) {
    RuntimeValue validation = validateGameValue(game);
    if (!isTruthy(objectGetOrNil(validation, "valid"))) return RuntimeValue::list({});

    std::vector<std::string> rowStrategies = strategiesForPlayer(game, 0);
    std::vector<std::string> colStrategies = strategiesForPlayer(game, 1);
    RuntimeList equilibria;

    for (size_t row = 0; row < rowStrategies.size(); ++row) {
        for (size_t col = 0; col < colStrategies.size(); ++col) {
            PayoffPair current = payoffAt(game, row, col);
            bool rowCanImprove = false;
            bool colCanImprove = false;
            for (size_t altRow = 0; altRow < rowStrategies.size(); ++altRow) {
                PayoffPair alt = payoffAt(game, altRow, col);
                if (alt.ok && alt.first > current.first + EPSILON) rowCanImprove = true;
            }
            for (size_t altCol = 0; altCol < colStrategies.size(); ++altCol) {
                PayoffPair alt = payoffAt(game, row, altCol);
                if (alt.ok && alt.second > current.second + EPSILON) colCanImprove = true;
            }
            if (!rowCanImprove && !colCanImprove) {
                equilibria.push_back(makeEquilibrium(game, row, col));
            }
        }
    }

    return RuntimeValue::list(equilibria);
}

RuntimeValue dominatedStrategies(const RuntimeValue& game) {
    RuntimeList players = gamePlayers(game);
    std::vector<std::string> rowStrategies = strategiesForPlayer(game, 0);
    std::vector<std::string> colStrategies = strategiesForPlayer(game, 1);
    RuntimeObject result;

    RuntimeList rowDominated;
    for (size_t row = 0; row < rowStrategies.size(); ++row) {
        for (size_t altRow = 0; altRow < rowStrategies.size(); ++altRow) {
            if (row == altRow) continue;
            bool atLeastAsGood = true;
            bool strictlyBetterSomewhere = false;
            for (size_t col = 0; col < colStrategies.size(); ++col) {
                PayoffPair current = payoffAt(game, row, col);
                PayoffPair alternative = payoffAt(game, altRow, col);
                if (!current.ok || !alternative.ok || alternative.first + EPSILON < current.first) atLeastAsGood = false;
                if (alternative.first > current.first + EPSILON) strictlyBetterSomewhere = true;
            }
            if (atLeastAsGood && strictlyBetterSomewhere) {
                rowDominated.emplace_back(rowStrategies[row]);
                break;
            }
        }
    }

    RuntimeList colDominated;
    for (size_t col = 0; col < colStrategies.size(); ++col) {
        for (size_t altCol = 0; altCol < colStrategies.size(); ++altCol) {
            if (col == altCol) continue;
            bool atLeastAsGood = true;
            bool strictlyBetterSomewhere = false;
            for (size_t row = 0; row < rowStrategies.size(); ++row) {
                PayoffPair current = payoffAt(game, row, col);
                PayoffPair alternative = payoffAt(game, row, altCol);
                if (!current.ok || !alternative.ok || alternative.second + EPSILON < current.second) atLeastAsGood = false;
                if (alternative.second > current.second + EPSILON) strictlyBetterSomewhere = true;
            }
            if (atLeastAsGood && strictlyBetterSomewhere) {
                colDominated.emplace_back(colStrategies[col]);
                break;
            }
        }
    }

    if (players.size() >= 1) result[playerName(players[0], 0)] = RuntimeValue::list(rowDominated);
    if (players.size() >= 2) result[playerName(players[1], 1)] = RuntimeValue::list(colDominated);
    return RuntimeValue::object(result);
}

RuntimeValue dominantStrategies(const RuntimeValue& game) {
    RuntimeList players = gamePlayers(game);
    std::vector<std::string> rowStrategies = strategiesForPlayer(game, 0);
    std::vector<std::string> colStrategies = strategiesForPlayer(game, 1);
    RuntimeObject result;

    RuntimeList rowDominant;
    for (size_t row = 0; row < rowStrategies.size(); ++row) {
        bool dominatesAll = true;
        for (size_t altRow = 0; altRow < rowStrategies.size(); ++altRow) {
            if (row == altRow) continue;
            bool weaklyBetter = true;
            bool strictlyBetter = false;
            for (size_t col = 0; col < colStrategies.size(); ++col) {
                PayoffPair current = payoffAt(game, row, col);
                PayoffPair alternative = payoffAt(game, altRow, col);
                if (!current.ok || !alternative.ok || current.first + EPSILON < alternative.first) weaklyBetter = false;
                if (current.first > alternative.first + EPSILON) strictlyBetter = true;
            }
            if (!weaklyBetter || !strictlyBetter) dominatesAll = false;
        }
        if (dominatesAll && rowStrategies.size() > 1) rowDominant.emplace_back(rowStrategies[row]);
    }

    RuntimeList colDominant;
    for (size_t col = 0; col < colStrategies.size(); ++col) {
        bool dominatesAll = true;
        for (size_t altCol = 0; altCol < colStrategies.size(); ++altCol) {
            if (col == altCol) continue;
            bool weaklyBetter = true;
            bool strictlyBetter = false;
            for (size_t row = 0; row < rowStrategies.size(); ++row) {
                PayoffPair current = payoffAt(game, row, col);
                PayoffPair alternative = payoffAt(game, row, altCol);
                if (!current.ok || !alternative.ok || current.second + EPSILON < alternative.second) weaklyBetter = false;
                if (current.second > alternative.second + EPSILON) strictlyBetter = true;
            }
            if (!weaklyBetter || !strictlyBetter) dominatesAll = false;
        }
        if (dominatesAll && colStrategies.size() > 1) colDominant.emplace_back(colStrategies[col]);
    }

    if (players.size() >= 1) result[playerName(players[0], 0)] = RuntimeValue::list(rowDominant);
    if (players.size() >= 2) result[playerName(players[1], 1)] = RuntimeValue::list(colDominant);
    return RuntimeValue::object(result);
}

RuntimeValue priceOfAnarchy(const RuntimeValue& game) {
    std::vector<std::string> rowStrategies = strategiesForPlayer(game, 0);
    std::vector<std::string> colStrategies = strategiesForPlayer(game, 1);
    double maxWelfare = 0.0;
    for (size_t row = 0; row < rowStrategies.size(); ++row) {
        for (size_t col = 0; col < colStrategies.size(); ++col) {
            maxWelfare = std::max(maxWelfare, socialWelfareAt(game, row, col));
        }
    }
    RuntimeList equilibria = asList(solvePureNash(game), "price_of_anarchy");
    if (equilibria.empty()) return RuntimeValue();
    double worstEquilibriumWelfare = std::numeric_limits<double>::infinity();
    for (const RuntimeValue& equilibrium : equilibria) {
        RuntimeObject strategies = asObject(objectGetOrNil(equilibrium, "strategies"), "equilibrium strategies");
        std::vector<std::string> rows = strategiesForPlayer(game, 0);
        std::vector<std::string> cols = strategiesForPlayer(game, 1);
        std::string rowStrategy = objectStringField(RuntimeValue::object(strategies), playerName(gamePlayers(game)[0], 0));
        std::string colStrategy = objectStringField(RuntimeValue::object(strategies), playerName(gamePlayers(game)[1], 1));
        auto rowIt = std::find(rows.begin(), rows.end(), rowStrategy);
        auto colIt = std::find(cols.begin(), cols.end(), colStrategy);
        if (rowIt != rows.end() && colIt != cols.end()) {
            size_t row = static_cast<size_t>(std::distance(rows.begin(), rowIt));
            size_t col = static_cast<size_t>(std::distance(cols.begin(), colIt));
            worstEquilibriumWelfare = std::min(worstEquilibriumWelfare, socialWelfareAt(game, row, col));
        }
    }
    if (!std::isfinite(worstEquilibriumWelfare) || worstEquilibriumWelfare <= EPSILON) return RuntimeValue();
    return RuntimeValue(maxWelfare / worstEquilibriumWelfare);
}

} // namespace GameLang
