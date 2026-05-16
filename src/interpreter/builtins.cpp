#include "interpreter.h"

#include "game_analysis.h"
#include "runtime_support.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>

namespace GameLang {
namespace {

RuntimeValue createPlayer(const std::vector<CallArg>& args) {
    std::string name = asString(argAt(args, 0, RuntimeValue("Player")));
    RuntimeObject object = {
        {"name", RuntimeValue(name)},
        {"strategies", argNamedOrAt(args, "strategies", 1, RuntimeValue::list({}))}
    };
    for (const CallArg& arg : args) {
        if (!arg.name.empty()) object[arg.name] = arg.value;
    }
    return makeTaggedObject("player", object);
}

RuntimeValue createBelief(const std::vector<CallArg>& args) {
    RuntimeObject distribution;
    if (args.size() == 1 && args[0].name.empty() && args[0].value.isObject()) {
        distribution = *std::get<RuntimeValue::ObjectPtr>(args[0].value.data);
    } else {
        for (const CallArg& arg : args) {
            if (!arg.name.empty()) distribution[arg.name] = arg.value;
        }
    }
    return makeTaggedObject("belief", {{"distribution", RuntimeValue::object(distribution)}});
}

RuntimeValue createStrategy(const std::vector<CallArg>& args) {
    RuntimeObject probabilities;
    for (const CallArg& arg : args) {
        if (!arg.name.empty()) probabilities[arg.name] = arg.value;
    }
    if (!probabilities.empty()) {
        return makeTaggedObject("mixed_strategy", {{"probabilities", RuntimeValue::object(probabilities)}});
    }
    RuntimeValue name = argAt(args, 0, RuntimeValue("strategy"));
    return makeTaggedObject("strategy", {{"name", RuntimeValue(asString(name))}});
}

RuntimeValue createGame(const std::vector<CallArg>& args, const std::string& requestedType = "game") {
    RuntimeValue playersArg = argNamedOrAt(args, "players", 0, RuntimeValue::list({}));
    RuntimeList players;
    RuntimeValue strategiesArg = argNamedOrAt(args, "strategies", 1, RuntimeValue());

    if (playersArg.isNumber()) {
        int count = static_cast<int>(std::max(0.0, std::get<double>(playersArg.data)));
        for (int i = 0; i < count; ++i) {
            players.push_back(makeTaggedObject("player", {
                {"name", RuntimeValue("P" + std::to_string(i + 1))},
                {"strategies", RuntimeValue::list({})}
            }));
        }
    } else if (playersArg.isList()) {
        players = *std::get<RuntimeValue::ListPtr>(playersArg.data);
    }

    RuntimeObject strategiesObject;
    if (strategiesArg.isObject()) {
        strategiesObject = *std::get<RuntimeValue::ObjectPtr>(strategiesArg.data);
    }

    if (players.empty() && requestedType == "population" && strategiesArg.isList()) {
        players.push_back(makeTaggedObject("player", {
            {"name", RuntimeValue("Population")},
            {"strategies", strategiesArg}
        }));
        players.push_back(makeTaggedObject("player", {
            {"name", RuntimeValue("Opponent")},
            {"strategies", strategiesArg}
        }));
    }

    if (players.empty() && !strategiesObject.empty()) {
        for (const auto& [name, strategies] : strategiesObject) {
            players.push_back(makeTaggedObject("player", {
                {"name", RuntimeValue(name)},
                {"strategies", strategies}
            }));
        }
    }

    for (size_t i = 0; i < players.size(); ++i) {
        std::string name = playerName(players[i], i);
        if (strategiesObject.find(name) == strategiesObject.end()) {
            RuntimeValue strategyList = objectGetOrNil(players[i], "strategies");
            if (strategyList.isList()) strategiesObject[name] = strategyList;
        }
    }

    RuntimeValue payoffs = argNamedOrAt(args, "payoffs", 2, RuntimeValue());
    if (payoffs.isNil()) payoffs = argNamedOrAt(args, "matrix", 2, RuntimeValue::list({}));

    bool missingInferredStrategies = strategiesObject.empty();
    if (!missingInferredStrategies && players.size() >= 2) {
        missingInferredStrategies = true;
        for (const auto& [ignoredName, strategies] : strategiesObject) {
            (void)ignoredName;
            if (strategies.isList() && !std::get<RuntimeValue::ListPtr>(strategies.data)->empty()) {
                missingInferredStrategies = false;
            }
        }
    }

    if (missingInferredStrategies && players.size() >= 2 && payoffs.isList()) {
        RuntimeList payoffRows = *std::get<RuntimeValue::ListPtr>(payoffs.data);
        RuntimeList rowStrategies;
        for (size_t row = 0; row < payoffRows.size(); ++row) {
            rowStrategies.emplace_back("S" + std::to_string(row + 1));
        }
        size_t columnCount = 0;
        if (!payoffRows.empty() && payoffRows.front().isList()) {
            RuntimeList firstRow = *std::get<RuntimeValue::ListPtr>(payoffRows.front().data);
            columnCount = (!firstRow.empty() && firstRow.front().isList()) ? firstRow.size() : firstRow.size() / 2;
        }
        RuntimeList columnStrategies;
        for (size_t col = 0; col < columnCount; ++col) {
            columnStrategies.emplace_back("S" + std::to_string(col + 1));
        }
        strategiesObject[playerName(players[0], 0)] = RuntimeValue::list(rowStrategies);
        strategiesObject[playerName(players[1], 1)] = RuntimeValue::list(columnStrategies);
    }

    RuntimeObject game = {
        {"players", RuntimeValue::list(players)},
        {"strategies", RuntimeValue::object(strategiesObject)},
        {"payoffs", payoffs},
        {"game_type", RuntimeValue(requestedType == "game" ? "normal" : requestedType)}
    };

    for (const CallArg& arg : args) {
        if (!arg.name.empty() &&
            arg.name != "players" &&
            arg.name != "strategies" &&
            arg.name != "payoffs" &&
            arg.name != "matrix") {
            game[arg.name] = arg.value;
        }
    }

    std::string tag = requestedType == "population" ? "population_game" :
                      requestedType == "extensive" ? "extensive_game" :
                      "game";
    return makeTaggedObject(tag, game);
}

RuntimeValue createMechanism(const std::string& type, const std::vector<CallArg>& args) {
    RuntimeObject object = {{"kind", RuntimeValue(type)}};
    RuntimeValue rule = argAt(args, 0, RuntimeValue(type));
    object["rule"] = RuntimeValue(asString(rule));
    for (const CallArg& arg : args) {
        if (!arg.name.empty()) object[arg.name] = arg.value;
    }
    return makeTaggedObject("mechanism", object);
}

RuntimeObject probabilityObject(const RuntimeValue& strategy) {
    if (objectType(strategy) == "mixed_strategy") {
        RuntimeValue probabilities = objectGetOrNil(strategy, "probabilities");
        if (probabilities.isObject()) return *std::get<RuntimeValue::ObjectPtr>(probabilities.data);
    }
    if (objectType(strategy) == "strategy") {
        return {{objectStringField(strategy, "name", "strategy"), RuntimeValue(1.0)}};
    }
    if (strategy.isString()) return {{std::get<std::string>(strategy.data), RuntimeValue(1.0)}};
    return {};
}

RuntimeValue expectedPayoff(const RuntimeValue& first, const RuntimeValue& second, const RuntimeValue& game) {
    RuntimeObject firstProbabilities = probabilityObject(first);
    RuntimeObject secondProbabilities = probabilityObject(second);
    std::vector<std::string> rowStrategies = strategiesForPlayer(game, 0);
    std::vector<std::string> colStrategies = strategiesForPlayer(game, 1);
    double firstPayoff = 0.0;
    double secondPayoff = 0.0;

    for (size_t row = 0; row < rowStrategies.size(); ++row) {
        double rowProbability = 0.0;
        auto rowFound = firstProbabilities.find(rowStrategies[row]);
        if (rowFound != firstProbabilities.end() && rowFound->second.isNumber()) rowProbability = std::get<double>(rowFound->second.data);
        for (size_t col = 0; col < colStrategies.size(); ++col) {
            double colProbability = 0.0;
            auto colFound = secondProbabilities.find(colStrategies[col]);
            if (colFound != secondProbabilities.end() && colFound->second.isNumber()) colProbability = std::get<double>(colFound->second.data);
            PayoffPair payoff = payoffAt(game, row, col);
            if (payoff.ok) {
                firstPayoff += rowProbability * colProbability * payoff.first;
                secondPayoff += rowProbability * colProbability * payoff.second;
            }
        }
    }

    return RuntimeValue::list({RuntimeValue(firstPayoff), RuntimeValue(secondPayoff)});
}

std::string describeGame(const RuntimeValue& game) {
    std::ostringstream out;
    RuntimeValue validation = validateGameValue(game);
    RuntimeList players = gamePlayers(game);
    out << "GameLang game";
    if (!objectStringField(game, "game_type").empty()) out << " (" << objectStringField(game, "game_type") << ")";
    out << "\nPlayers:\n";
    for (size_t i = 0; i < players.size(); ++i) {
        out << "  - " << playerName(players[i], i) << ": "
            << valueToString(RuntimeValue::list(stringListToValues(strategiesForPlayer(game, i)))) << "\n";
    }
    out << "Validation: " << (isTruthy(objectGetOrNil(validation, "valid")) ? "valid" : "invalid") << "\n";
    RuntimeList errors = objectListField(validation, "errors");
    for (const RuntimeValue& error : errors) out << "  - " << valueToString(error) << "\n";
    out << "Pure Nash equilibria: " << valueToString(solvePureNash(game)) << "\n";
    out << "Dominated strategies: " << valueToString(dominatedStrategies(game)) << "\n";
    return out.str();
}

RuntimeValue explainNash(const RuntimeValue& game) {
    RuntimeList equilibria = asList(solvePureNash(game), "explain_nash");
    if (equilibria.empty()) {
        return RuntimeValue("No pure Nash equilibrium was found by the MVP finite-game solver.");
    }
    RuntimeList explanations;
    for (const RuntimeValue& equilibrium : equilibria) {
        RuntimeValue strategies = objectGetOrNil(equilibrium, "strategies");
        RuntimeValue payoffs = objectGetOrNil(equilibrium, "payoffs");
        std::ostringstream out;
        out << valueToString(strategies)
            << " is a pure Nash equilibrium: no player can switch strategies alone and improve from payoffs "
            << valueToString(payoffs) << ".";
        explanations.emplace_back(out.str());
    }
    if (explanations.size() == 1) return explanations.front();
    return RuntimeValue::list(explanations);
}

RuntimeValue eliminationSummary(const RuntimeValue& game) {
    RuntimeValue eliminated = dominatedStrategies(game);
    RuntimeObject remaining;
    RuntimeList players = gamePlayers(game);
    for (size_t i = 0; i < players.size(); ++i) {
        std::string name = playerName(players[i], i);
        std::set<std::string> eliminatedSet;
        RuntimeValue playerEliminated = objectGetOrNil(eliminated, name);
        if (playerEliminated.isList()) {
            for (const RuntimeValue& strategy : *std::get<RuntimeValue::ListPtr>(playerEliminated.data)) {
                eliminatedSet.insert(asString(strategy));
            }
        }
        RuntimeList kept;
        for (const std::string& strategy : strategiesForPlayer(game, i)) {
            if (eliminatedSet.find(strategy) == eliminatedSet.end()) kept.emplace_back(strategy);
        }
        remaining[name] = RuntimeValue::list(kept);
    }
    return makeTaggedObject("elimination", {
        {"eliminated", eliminated},
        {"remaining", RuntimeValue::object(remaining)}
    });
}

RuntimeValue findCounterexampleGame() {
    RuntimeList players = {
        createPlayer(positionalArgs({RuntimeValue("P1"), RuntimeValue::list({RuntimeValue("A"), RuntimeValue("B")})})),
        createPlayer(positionalArgs({RuntimeValue("P2"), RuntimeValue::list({RuntimeValue("A"), RuntimeValue("B")})}))
    };
    RuntimeList payoffs = {
        RuntimeValue::list({RuntimeValue(10), RuntimeValue(10), RuntimeValue(0), RuntimeValue(6)}),
        RuntimeValue::list({RuntimeValue(6), RuntimeValue(0), RuntimeValue(1), RuntimeValue(1)})
    };
    return createGame({
        {"players", RuntimeValue::list(players)},
        {"payoffs", RuntimeValue::list(payoffs)}
    });
}

RuntimeValue sweepValues(const std::vector<CallArg>& args) {
    RuntimeValue subject = argAt(args, 0, RuntimeValue("sweep"));
    RuntimeValue valuesArg = argNamedOrAt(args, "values", 1, RuntimeValue::list({}));
    std::string parameter = asString(argNamedOrAt(args, "param", 2, RuntimeValue("value")));
    std::string metric = asString(argNamedOrAt(args, "metric", 3, RuntimeValue("identity")));
    RuntimeList rows;
    if (!valuesArg.isList()) valuesArg = RuntimeValue::list({valuesArg});
    for (const RuntimeValue& value : *std::get<RuntimeValue::ListPtr>(valuesArg.data)) {
        RuntimeObject row = {{parameter, value}};
        if (metric == "price_of_anarchy" && (objectType(subject) == "game" || objectType(subject) == "population_game")) {
            row["result"] = priceOfAnarchy(subject);
        } else {
            row["result"] = value;
        }
        rows.push_back(RuntimeValue::object(row));
    }
    return makeTaggedObject("sweep", {{"rows", RuntimeValue::list(rows)}});
}

RuntimeValue copyWithFields(const RuntimeValue& value, RuntimeObject fields) {
    RuntimeObject object = value.isObject() ? *std::get<RuntimeValue::ObjectPtr>(value.data) : RuntimeObject{};
    for (auto& [key, fieldValue] : fields) object[key] = fieldValue;
    return RuntimeValue::object(object);
}

RuntimeList uniqueValues(const RuntimeList& values) {
    RuntimeList unique;
    for (const RuntimeValue& value : values) {
        bool seen = false;
        for (const RuntimeValue& existing : unique) {
            if (valuesEqual(existing, value)) {
                seen = true;
                break;
            }
        }
        if (!seen) unique.push_back(value);
    }
    return unique;
}

RuntimeList iterableItems(const RuntimeValue& value) {
    if (value.isList()) return *std::get<RuntimeValue::ListPtr>(value.data);
    if (objectType(value) == "set") return objectListField(value, "values");
    if (value.isObject()) {
        RuntimeList pairs;
        for (const auto& [key, member] : *std::get<RuntimeValue::ObjectPtr>(value.data)) {
            if (key == "__type") continue;
            pairs.push_back(RuntimeValue::list({RuntimeValue(key), member}));
        }
        return pairs;
    }
    if (value.isString()) {
        RuntimeList chars;
        for (char c : std::get<std::string>(value.data)) chars.emplace_back(std::string(1, c));
        return chars;
    }
    return {};
}

RuntimeValue makeMatrix(const RuntimeValue& value) {
    return makeTaggedObject("matrix", {{"values", value}});
}

RuntimeValue solveMixedNash2x2(const RuntimeValue& game) {
    std::vector<std::string> rowStrategies = strategiesForPlayer(game, 0);
    std::vector<std::string> colStrategies = strategiesForPlayer(game, 1);
    RuntimeList players = gamePlayers(game);
    if (rowStrategies.size() != 2 || colStrategies.size() != 2 || players.size() < 2) {
        return RuntimeValue::list({});
    }

    PayoffPair p00 = payoffAt(game, 0, 0);
    PayoffPair p01 = payoffAt(game, 0, 1);
    PayoffPair p10 = payoffAt(game, 1, 0);
    PayoffPair p11 = payoffAt(game, 1, 1);
    if (!p00.ok || !p01.ok || !p10.ok || !p11.ok) return RuntimeValue::list({});

    double qDenominator = p00.first - p01.first - p10.first + p11.first;
    double pDenominator = p00.second - p10.second - p01.second + p11.second;
    if (std::abs(qDenominator) < EPSILON || std::abs(pDenominator) < EPSILON) {
        return RuntimeValue::list({});
    }

    double q = (p11.first - p01.first) / qDenominator;
    double p = (p11.second - p10.second) / pDenominator;
    if (p < -EPSILON || p > 1.0 + EPSILON || q < -EPSILON || q > 1.0 + EPSILON) {
        return RuntimeValue::list({});
    }
    p = std::clamp(p, 0.0, 1.0);
    q = std::clamp(q, 0.0, 1.0);

    RuntimeObject rowMix = {
        {rowStrategies[0], RuntimeValue(p)},
        {rowStrategies[1], RuntimeValue(1.0 - p)}
    };
    RuntimeObject colMix = {
        {colStrategies[0], RuntimeValue(q)},
        {colStrategies[1], RuntimeValue(1.0 - q)}
    };
    RuntimeObject strategies = {
        {playerName(players[0], 0), makeTaggedObject("mixed_strategy", {{"probabilities", RuntimeValue::object(rowMix)}})},
        {playerName(players[1], 1), makeTaggedObject("mixed_strategy", {{"probabilities", RuntimeValue::object(colMix)}})}
    };

    RuntimeValue payoffs = expectedPayoff(
        makeTaggedObject("mixed_strategy", {{"probabilities", RuntimeValue::object(rowMix)}}),
        makeTaggedObject("mixed_strategy", {{"probabilities", RuntimeValue::object(colMix)}}),
        game);

    return RuntimeValue::list({
        makeTaggedObject("equilibrium", {
            {"concept", RuntimeValue("mixed_nash")},
            {"strategies", RuntimeValue::object(strategies)},
            {"payoffs", payoffs}
        })
    });
}

RuntimeValue solveNashWithMixedFallback(const RuntimeValue& game) {
    RuntimeValue pure = solvePureNash(game);
    if (pure.isList() && !std::get<RuntimeValue::ListPtr>(pure.data)->empty()) return pure;
    return solveMixedNash2x2(game);
}

RuntimeValue relabelEquilibria(RuntimeValue equilibria, const std::string& conceptName) {
    RuntimeList output;
    for (const RuntimeValue& equilibrium : asList(equilibria, conceptName)) {
        RuntimeObject object = asObject(equilibrium, conceptName);
        object["concept"] = RuntimeValue(conceptName);
        output.push_back(makeTaggedObject("equilibrium", object));
    }
    return RuntimeValue::list(output);
}

RuntimeValue correlatedEquilibrium(const RuntimeValue& game) {
    RuntimeList equilibria = asList(solveNashWithMixedFallback(game), "correlated_equilibrium");
    RuntimeList support;
    if (!equilibria.empty()) {
        double probability = 1.0 / static_cast<double>(equilibria.size());
        for (const RuntimeValue& equilibrium : equilibria) {
            support.push_back(RuntimeValue::object({
                {"profile", objectGetOrNil(equilibrium, "strategies")},
                {"probability", RuntimeValue(probability)}
            }));
        }
    }
    return RuntimeValue::list({
        makeTaggedObject("equilibrium", {
            {"concept", RuntimeValue("correlated_equilibrium")},
            {"support", RuntimeValue::list(support)}
        })
    });
}

RuntimeValue essCandidates(const RuntimeValue& game) {
    RuntimeList players = gamePlayers(game);
    std::vector<std::string> strategies = strategiesForPlayer(game, 0);
    RuntimeList candidates;
    if (players.size() < 2 || strategies.empty() || strategiesForPlayer(game, 1).size() != strategies.size()) {
        return RuntimeValue::list(candidates);
    }

    for (size_t s = 0; s < strategies.size(); ++s) {
        PayoffPair self = payoffAt(game, s, s);
        if (!self.ok) continue;
        bool stable = true;
        for (size_t mutant = 0; mutant < strategies.size(); ++mutant) {
            if (mutant == s) continue;
            PayoffPair mutantAgainstResident = payoffAt(game, mutant, s);
            PayoffPair residentAgainstMutant = payoffAt(game, s, mutant);
            PayoffPair mutantAgainstMutant = payoffAt(game, mutant, mutant);
            if (!mutantAgainstResident.ok || !residentAgainstMutant.ok || !mutantAgainstMutant.ok) {
                stable = false;
                break;
            }
            if (mutantAgainstResident.first > self.first + EPSILON) stable = false;
            if (std::abs(mutantAgainstResident.first - self.first) < EPSILON &&
                mutantAgainstMutant.first >= residentAgainstMutant.first - EPSILON) {
                stable = false;
            }
        }
        if (stable) candidates.push_back(RuntimeValue(strategies[s]));
    }
    return RuntimeValue::list(candidates);
}

RuntimeValue paretoEfficientOutcomes(const RuntimeValue& game) {
    std::vector<std::string> rows = strategiesForPlayer(game, 0);
    std::vector<std::string> cols = strategiesForPlayer(game, 1);
    RuntimeList outcomes;
    for (size_t row = 0; row < rows.size(); ++row) {
        for (size_t col = 0; col < cols.size(); ++col) {
            PayoffPair payoff = payoffAt(game, row, col);
            if (!payoff.ok) continue;
            bool dominated = false;
            for (size_t otherRow = 0; otherRow < rows.size(); ++otherRow) {
                for (size_t otherCol = 0; otherCol < cols.size(); ++otherCol) {
                    if (otherRow == row && otherCol == col) continue;
                    PayoffPair other = payoffAt(game, otherRow, otherCol);
                    if (!other.ok) continue;
                    if (other.first >= payoff.first - EPSILON &&
                        other.second >= payoff.second - EPSILON &&
                        (other.first > payoff.first + EPSILON || other.second > payoff.second + EPSILON)) {
                        dominated = true;
                    }
                }
            }
            if (!dominated) {
                outcomes.push_back(RuntimeValue::object({
                    {"strategies", RuntimeValue::list({RuntimeValue(rows[row]), RuntimeValue(cols[col])})},
                    {"payoffs", RuntimeValue::list({RuntimeValue(payoff.first), RuntimeValue(payoff.second)})}
                }));
            }
        }
    }
    return RuntimeValue::list(outcomes);
}

RuntimeValue bestResponses(const RuntimeValue& game, int playerIndex, const RuntimeValue& against) {
    RuntimeList players = gamePlayers(game);
    if (players.size() < 2) return RuntimeValue::list({});
    std::vector<std::string> rows = strategiesForPlayer(game, 0);
    std::vector<std::string> cols = strategiesForPlayer(game, 1);
    RuntimeList responses;
    if (playerIndex == 1) {
        std::string rowStrategy = asString(against);
        auto rowIt = std::find(rows.begin(), rows.end(), rowStrategy);
        size_t row = rowIt == rows.end() ? 0 : static_cast<size_t>(std::distance(rows.begin(), rowIt));
        double best = -std::numeric_limits<double>::infinity();
        for (size_t col = 0; col < cols.size(); ++col) {
            PayoffPair payoff = payoffAt(game, row, col);
            if (payoff.ok) best = std::max(best, payoff.second);
        }
        for (size_t col = 0; col < cols.size(); ++col) {
            PayoffPair payoff = payoffAt(game, row, col);
            if (payoff.ok && std::abs(payoff.second - best) < EPSILON) responses.emplace_back(cols[col]);
        }
        return RuntimeValue::list(responses);
    }

    std::string colStrategy = asString(against);
    auto colIt = std::find(cols.begin(), cols.end(), colStrategy);
    size_t col = colIt == cols.end() ? 0 : static_cast<size_t>(std::distance(cols.begin(), colIt));
    double best = -std::numeric_limits<double>::infinity();
    for (size_t row = 0; row < rows.size(); ++row) {
        PayoffPair payoff = payoffAt(game, row, col);
        if (payoff.ok) best = std::max(best, payoff.first);
    }
    for (size_t row = 0; row < rows.size(); ++row) {
        PayoffPair payoff = payoffAt(game, row, col);
        if (payoff.ok && std::abs(payoff.first - best) < EPSILON) responses.emplace_back(rows[row]);
    }
    return RuntimeValue::list(responses);
}

RuntimeValue simulateReplicator(const RuntimeValue& game, const std::vector<CallArg>& args) {
    RuntimeValue initialArg = argNamedOrAt(args, "initial", 1, RuntimeValue::list({RuntimeValue(0.5), RuntimeValue(0.5)}));
    RuntimeList initial = asList(initialArg, "simulate_dynamics initial");
    double firstShare = initial.empty() ? 0.5 : asNumber(initial[0], "initial share");
    int steps = static_cast<int>(asNumber(argNamedOrAt(args, "steps", 2, RuntimeValue(25)), "simulate_dynamics steps"));
    double dt = asNumber(argNamedOrAt(args, "dt", 3, RuntimeValue(0.1)), "simulate_dynamics dt");
    RuntimeList trajectory;
    for (int step = 0; step <= steps; ++step) {
        firstShare = std::clamp(firstShare, 0.0, 1.0);
        trajectory.push_back(RuntimeValue::object({
            {"step", RuntimeValue(static_cast<double>(step))},
            {"distribution", RuntimeValue::list({RuntimeValue(firstShare), RuntimeValue(1.0 - firstShare)})}
        }));
        PayoffPair aa = payoffAt(game, 0, 0);
        PayoffPair ab = payoffAt(game, 0, 1);
        PayoffPair ba = payoffAt(game, 1, 0);
        PayoffPair bb = payoffAt(game, 1, 1);
        if (!aa.ok || !ab.ok || !ba.ok || !bb.ok) break;
        double fitnessA = firstShare * aa.first + (1.0 - firstShare) * ab.first;
        double fitnessB = firstShare * ba.first + (1.0 - firstShare) * bb.first;
        double average = firstShare * fitnessA + (1.0 - firstShare) * fitnessB;
        firstShare += dt * firstShare * (fitnessA - average);
    }
    return makeTaggedObject("trajectory", {{"points", RuntimeValue::list(trajectory)}});
}

struct RoundRecord {
    bool mineCooperated = true;
    bool opponentCooperated = true;
    double myPayoff = 0.0;
};

bool chooseTournamentAction(const std::string& strategy, const std::vector<RoundRecord>& history, std::mt19937& rng) {
    std::string lowered = strategy;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lowered == "alwaysdefect" || lowered == "defect" || lowered == "all_d") return false;
    if (lowered == "alwayscooperate" || lowered == "cooperate" || lowered == "all_c") return true;
    if (lowered == "random") return std::uniform_real_distribution<double>(0.0, 1.0)(rng) < 0.5;
    if (history.empty()) return true;
    if (lowered == "titfortat") return history.back().opponentCooperated;
    if (lowered == "generous") {
        if (history.back().opponentCooperated) return true;
        return std::uniform_real_distribution<double>(0.0, 1.0)(rng) < 0.1;
    }
    if (lowered == "grudger") {
        return std::none_of(history.begin(), history.end(), [](const RoundRecord& round) { return !round.opponentCooperated; });
    }
    if (lowered == "pavlov") {
        return history.back().myPayoff >= 3.0 ? history.back().mineCooperated : !history.back().mineCooperated;
    }
    if (lowered == "titfortwotats") {
        if (history.size() < 2) return true;
        return history[history.size() - 1].opponentCooperated || history[history.size() - 2].opponentCooperated;
    }
    return history.back().opponentCooperated;
}

RuntimeValue runTournament(const std::vector<CallArg>& args, std::mt19937& rng, bool parallel = false) {
    RuntimeValue strategyValue = argAt(args, 0, RuntimeValue::object({}));
    int rounds = static_cast<int>(asNumber(argNamedOrAt(args, "rounds", 1, RuntimeValue(200)), "tournament rounds"));
    double noise = asNumber(argNamedOrAt(args, "noise", 2, RuntimeValue(0.0)), "tournament noise");

    std::vector<std::string> names;
    if (strategyValue.isObject()) {
        for (const auto& [strategyName, ignored] : *std::get<RuntimeValue::ObjectPtr>(strategyValue.data)) {
            (void)ignored;
            if (strategyName != "__type") names.push_back(strategyName);
        }
    } else if (strategyValue.isList()) {
        for (const RuntimeValue& value : *std::get<RuntimeValue::ListPtr>(strategyValue.data)) names.push_back(asString(value));
    }
    std::sort(names.begin(), names.end());

    RuntimeObject scores;
    RuntimeObject cooperation;
    RuntimeList matches;
    for (const std::string& strategyName : names) {
        scores[strategyName] = RuntimeValue(0.0);
        cooperation[strategyName] = RuntimeValue(0.0);
    }

    std::uniform_real_distribution<double> unit(0.0, 1.0);
    for (size_t i = 0; i < names.size(); ++i) {
        for (size_t j = i + 1; j < names.size(); ++j) {
            std::vector<RoundRecord> firstHistory;
            std::vector<RoundRecord> secondHistory;
            double firstScore = 0.0;
            double secondScore = 0.0;
            double firstCoop = 0.0;
            double secondCoop = 0.0;
            for (int round = 0; round < rounds; ++round) {
                bool firstAction = chooseTournamentAction(names[i], firstHistory, rng);
                bool secondAction = chooseTournamentAction(names[j], secondHistory, rng);
                if (unit(rng) < noise) firstAction = !firstAction;
                if (unit(rng) < noise) secondAction = !secondAction;
                double firstPayoff = 0.0;
                double secondPayoff = 0.0;
                if (firstAction && secondAction) { firstPayoff = 3; secondPayoff = 3; }
                else if (firstAction && !secondAction) { firstPayoff = 0; secondPayoff = 5; }
                else if (!firstAction && secondAction) { firstPayoff = 5; secondPayoff = 0; }
                else { firstPayoff = 1; secondPayoff = 1; }
                firstScore += firstPayoff;
                secondScore += secondPayoff;
                firstCoop += firstAction ? 1.0 : 0.0;
                secondCoop += secondAction ? 1.0 : 0.0;
                firstHistory.push_back({firstAction, secondAction, firstPayoff});
                secondHistory.push_back({secondAction, firstAction, secondPayoff});
            }
            scores[names[i]] = RuntimeValue(std::get<double>(scores[names[i]].data) + firstScore);
            scores[names[j]] = RuntimeValue(std::get<double>(scores[names[j]].data) + secondScore);
            cooperation[names[i]] = RuntimeValue(std::get<double>(cooperation[names[i]].data) + firstCoop);
            cooperation[names[j]] = RuntimeValue(std::get<double>(cooperation[names[j]].data) + secondCoop);
            matches.push_back(RuntimeValue::object({
                {"player1", RuntimeValue(names[i])},
                {"player2", RuntimeValue(names[j])},
                {"score1", RuntimeValue(firstScore)},
                {"score2", RuntimeValue(secondScore)},
                {"cooperation1", RuntimeValue(rounds > 0 ? firstCoop / rounds : 0.0)},
                {"cooperation2", RuntimeValue(rounds > 0 ? secondCoop / rounds : 0.0)}
            }));
        }
    }

    double opponentCount = names.size() > 1 ? static_cast<double>(names.size() - 1) * std::max(1, rounds) : 1.0;
    for (const std::string& strategyName : names) {
        cooperation[strategyName] = RuntimeValue(std::get<double>(cooperation[strategyName].data) / opponentCount);
    }

    return makeTaggedObject("tournament_results", {
        {"scores", RuntimeValue::object(scores)},
        {"cooperation_rates", RuntimeValue::object(cooperation)},
        {"matches", RuntimeValue::list(matches)},
        {"parallel", RuntimeValue(parallel)}
    });
}

RuntimeValue rankTournament(const RuntimeValue& value) {
    RuntimeValue scoresValue = objectGetOrNil(value, "scores");
    RuntimeObject scores = scoresValue.isObject() ? *std::get<RuntimeValue::ObjectPtr>(scoresValue.data) : RuntimeObject{};
    RuntimeList rows;
    for (const auto& [name, score] : scores) {
        rows.push_back(RuntimeValue::object({{"name", RuntimeValue(name)}, {"score", score}}));
    }
    std::sort(rows.begin(), rows.end(), [](const RuntimeValue& left, const RuntimeValue& right) {
        double leftScore = asNumber(objectGetOrNil(left, "score"), "score");
        double rightScore = asNumber(objectGetOrNil(right, "score"), "score");
        return leftScore > rightScore;
    });
    return RuntimeValue::list(rows);
}

std::string csvEscape(const std::string& text) {
    bool needsQuoting = text.find(',') != std::string::npos || text.find('"') != std::string::npos || text.find('\n') != std::string::npos;
    if (!needsQuoting) return text;
    std::string escaped = "\"";
    for (char c : text) {
        if (c == '"') escaped += "\"\"";
        else escaped += c;
    }
    escaped += "\"";
    return escaped;
}

RuntimeValue exportCsv(const RuntimeValue& value, const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) throw std::runtime_error("Could not open CSV export path: " + path);

    RuntimeValue rowsValue = value;
    if (objectType(value) == "sweep") rowsValue = objectGetOrNil(value, "rows");
    if (objectType(value) == "tournament_results") rowsValue = rankTournament(value);

    if (rowsValue.isList()) {
        RuntimeList rows = *std::get<RuntimeValue::ListPtr>(rowsValue.data);
        bool objectRows = !rows.empty() && rows.front().isObject();
        if (objectRows) {
            std::vector<std::string> headers;
            std::set<std::string> seen;
            for (const RuntimeValue& row : rows) {
                for (const auto& [key, ignored] : *std::get<RuntimeValue::ObjectPtr>(row.data)) {
                    (void)ignored;
                    if (key == "__type") continue;
                    if (seen.insert(key).second) headers.push_back(key);
                }
            }
            for (size_t i = 0; i < headers.size(); ++i) {
                if (i > 0) file << ",";
                file << csvEscape(headers[i]);
            }
            file << "\n";
            for (const RuntimeValue& row : rows) {
                RuntimeObject object = *std::get<RuntimeValue::ObjectPtr>(row.data);
                for (size_t i = 0; i < headers.size(); ++i) {
                    if (i > 0) file << ",";
                    auto found = object.find(headers[i]);
                    file << csvEscape(found == object.end() ? "" : valueToString(found->second));
                }
                file << "\n";
            }
        } else {
            for (const RuntimeValue& row : rows) {
                if (row.isList()) {
                    RuntimeList columns = *std::get<RuntimeValue::ListPtr>(row.data);
                    for (size_t i = 0; i < columns.size(); ++i) {
                        if (i > 0) file << ",";
                        file << csvEscape(valueToString(columns[i]));
                    }
                } else {
                    file << csvEscape(valueToString(row));
                }
                file << "\n";
            }
        }
    } else {
        file << csvEscape(valueToString(rowsValue)) << "\n";
    }

    return RuntimeValue(path);
}

RuntimeValue exportDot(const RuntimeValue& value, const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) throw std::runtime_error("Could not open DOT export path: " + path);
    file << "digraph GameLang {\n";
    file << "  rankdir=LR;\n";
    if (objectType(value) == "game") {
        RuntimeList players = gamePlayers(value);
        for (size_t i = 0; i < players.size(); ++i) {
            file << "  player" << i << " [label=\"" << playerName(players[i], i) << "\", shape=box];\n";
        }
        std::vector<std::string> rows = strategiesForPlayer(value, 0);
        std::vector<std::string> cols = strategiesForPlayer(value, 1);
        for (size_t row = 0; row < rows.size(); ++row) {
            for (size_t col = 0; col < cols.size(); ++col) {
                PayoffPair payoff = payoffAt(value, row, col);
                file << "  outcome_" << row << "_" << col << " [label=\""
                     << rows[row] << "/" << cols[col] << "\\n("
                     << payoff.first << "," << payoff.second << ")\"];\n";
            }
        }
    } else {
        file << "  value [label=\"" << valueToString(value) << "\"];\n";
    }
    file << "}\n";
    return RuntimeValue(path);
}

RuntimeValue listExamples() {
    return RuntimeValue::list({
        RuntimeValue("examples/feature_showcase.gl"),
        RuntimeValue("examples/prisoners_dilemma.gl"),
        RuntimeValue("examples/auction_theory.gl"),
        RuntimeValue("examples/strategy_tournament.gl"),
        RuntimeValue("examples/evolutionary_dynamics.gl"),
        RuntimeValue("examples/market_entry_analysis.gl")
    });
}

} // namespace

RuntimeValue Interpreter::callFunction(const std::string& name, const std::vector<CallArg>& args, bool parallel) {
    if (name == "print") {
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) std::cout << " ";
            std::cout << valueToString(args[i].value);
        }
        std::cout << "\n";
        return RuntimeValue();
    }
    if (name == "pretty_print") {
        RuntimeValue value = argAt(args, 0, RuntimeValue());
        std::cout << valueToString(value) << "\n";
        return value;
    }
    if (name == "len") {
        RuntimeValue value = argAt(args, 0, RuntimeValue());
        if (value.isList()) return RuntimeValue(static_cast<double>(std::get<RuntimeValue::ListPtr>(value.data)->size()));
        if (value.isObject()) return RuntimeValue(static_cast<double>(std::get<RuntimeValue::ObjectPtr>(value.data)->size()));
        if (value.isString()) return RuntimeValue(static_cast<double>(std::get<std::string>(value.data).size()));
        return RuntimeValue(0);
    }
    if (name == "sum") {
        RuntimeValue value = argAt(args, 0, RuntimeValue::list({}));
        double total = 0.0;
        if (value.isList()) {
            for (const RuntimeValue& item : *std::get<RuntimeValue::ListPtr>(value.data)) total += asNumber(item, "sum");
        } else {
            for (const CallArg& arg : args) total += asNumber(arg.value, "sum");
        }
        return RuntimeValue(total);
    }
    if (name == "median") {
        RuntimeValue value = argAt(args, 0, RuntimeValue::list({}));
        RuntimeList list = asList(value, "median");
        std::vector<double> numbers;
        for (const RuntimeValue& item : list) numbers.push_back(asNumber(item, "median"));
        if (numbers.empty()) return RuntimeValue();
        std::sort(numbers.begin(), numbers.end());
        size_t mid = numbers.size() / 2;
        if (numbers.size() % 2 == 1) return RuntimeValue(numbers[mid]);
        return RuntimeValue((numbers[mid - 1] + numbers[mid]) / 2.0);
    }
    if (name == "flatten") {
        RuntimeList output;
        RuntimeList input = asList(argAt(args, 0, RuntimeValue::list({})), "flatten");
        for (const RuntimeValue& item : input) {
            if (item.isList()) {
                for (const RuntimeValue& nested : *std::get<RuntimeValue::ListPtr>(item.data)) output.push_back(nested);
            } else {
                output.push_back(item);
            }
        }
        return RuntimeValue::list(output);
    }
    if (name == "collect") return argAt(args, 0, RuntimeValue());
    if (name == "map") {
        RuntimeList output;
        RuntimeValue input = argAt(args, 0, RuntimeValue::list({}));
        RuntimeValue callable = argAt(args, 1, RuntimeValue());
        for (const RuntimeValue& item : iterableItems(input)) {
            output.push_back(callValue(callable, positionalArgs({item}), parallel));
        }
        return RuntimeValue::list(output);
    }
    if (name == "flat_map") {
        RuntimeList output;
        RuntimeValue input = argAt(args, 0, RuntimeValue::list({}));
        RuntimeValue callable = argAt(args, 1, RuntimeValue());
        for (const RuntimeValue& item : iterableItems(input)) {
            RuntimeValue mapped = callValue(callable, positionalArgs({item}), parallel);
            if (mapped.isList()) {
                for (const RuntimeValue& nested : *std::get<RuntimeValue::ListPtr>(mapped.data)) output.push_back(nested);
            } else {
                output.push_back(mapped);
            }
        }
        return RuntimeValue::list(output);
    }
    if (name == "filter") {
        RuntimeList output;
        RuntimeValue input = argAt(args, 0, RuntimeValue::list({}));
        RuntimeValue callable = argAt(args, 1, RuntimeValue());
        for (const RuntimeValue& item : iterableItems(input)) {
            if (isTruthy(callValue(callable, positionalArgs({item}), parallel))) output.push_back(item);
        }
        return RuntimeValue::list(output);
    }
    if (name == "all") {
        RuntimeValue input = argAt(args, 0, RuntimeValue::list({}));
        RuntimeValue callable = argAt(args, 1, RuntimeValue());
        for (const RuntimeValue& item : iterableItems(input)) {
            RuntimeValue check = callable.isNil() ? item : callValue(callable, positionalArgs({item}), parallel);
            if (!isTruthy(check)) return RuntimeValue(false);
        }
        return RuntimeValue(true);
    }
    if (name == "max") {
        RuntimeList items = iterableItems(argAt(args, 0, RuntimeValue::list({})));
        if (items.empty()) return RuntimeValue();
        RuntimeValue keyCallable = argNamedOrAt(args, "key", 1, RuntimeValue());
        RuntimeValue best = items.front();
        double bestScore = keyCallable.isNil()
            ? asNumber(best, "max")
            : asNumber(callValue(keyCallable, positionalArgs({best}), parallel), "max key");
        for (size_t i = 1; i < items.size(); ++i) {
            double score = keyCallable.isNil()
                ? asNumber(items[i], "max")
                : asNumber(callValue(keyCallable, positionalArgs({items[i]}), parallel), "max key");
            if (score > bestScore) {
                bestScore = score;
                best = items[i];
            }
        }
        return best;
    }

    if (name == "num") {
        RuntimeValue value = argAt(args, 0, RuntimeValue(0));
        if (value.isNumber()) return value;
        return RuntimeValue(std::stod(asString(value)));
    }
    if (name == "str") return RuntimeValue(asString(argAt(args, 0, RuntimeValue())));
    if (name == "bool") return RuntimeValue(isTruthy(argAt(args, 0, RuntimeValue())));
    if (name == "list") return RuntimeValue::list(iterableItems(argAt(args, 0, RuntimeValue::list({}))));
    if (name == "set") return makeTaggedObject("set", {{"values", RuntimeValue::list(uniqueValues(iterableItems(argAt(args, 0, RuntimeValue::list({})))))}}); 
    if (name == "dict") {
        RuntimeValue value = argAt(args, 0, RuntimeValue::object({}));
        if (value.isObject()) return value;
        RuntimeObject object;
        for (const RuntimeValue& pairValue : iterableItems(value)) {
            RuntimeList pair = asList(pairValue, "dict item");
            if (pair.size() >= 2) object[asString(pair[0])] = pair[1];
        }
        return RuntimeValue::object(object);
    }
    if (name == "matrix") return makeMatrix(argAt(args, 0, RuntimeValue::list({})));

    if (name == "player") return createPlayer(args);
    if (name == "strategy") return createStrategy(args);
    if (name == "pure") return createStrategy(args);
    if (name == "mixed") return createStrategy(args);
    if (name == "player_type") {
        RuntimeObject object;
        for (const CallArg& arg : args) {
            if (!arg.name.empty()) object[arg.name] = arg.value;
        }
        return makeTaggedObject("player_type", object);
    }
    if (name == "belief") return createBelief(args);
    if (name == "game") return createGame(args);
    if (name == "population_game") return createGame(args, "population");
    if (name == "extensive_game") return createGame(args, "extensive");
    if (name == "auction") return createMechanism("auction", args);
    if (name == "voting") return createMechanism("voting", args);
    if (name == "matching") return createMechanism("matching", args);
    if (name == "mechanism") return createMechanism("mechanism", args);

    if (name == "validate_game") return validateGameValue(argAt(args, 0, RuntimeValue()));
    if (name == "describe") return RuntimeValue(describeGame(argAt(args, 0, RuntimeValue())));
    if (name == "solve_nash" || name == "find_all_nash") return solveNashWithMixedFallback(argAt(args, 0, RuntimeValue()));
    if (name == "solve_equilibrium") return solveNashWithMixedFallback(argAt(args, 0, RuntimeValue()));
    if (name == "solve") {
        RuntimeValue game = argAt(args, 0, RuntimeValue());
        std::string conceptName = asString(argNamedOrAt(args, "concept", 1, RuntimeValue("nash")));
        if (conceptName == "nash" || conceptName == "pure" || conceptName == "pure_nash") return solvePureNash(game);
        if (conceptName == "mixed" || conceptName == "mixed_nash") return solveMixedNash2x2(game);
        if (conceptName == "correlated" || conceptName == "correlated_equilibrium") return correlatedEquilibrium(game);
        if (conceptName == "perfect" || conceptName == "perfect_equilibrium") return relabelEquilibria(solveNashWithMixedFallback(game), "perfect_equilibrium");
        if (conceptName == "proper" || conceptName == "proper_equilibrium") return relabelEquilibria(solveNashWithMixedFallback(game), "proper_equilibrium");
        if (conceptName == "ess") return makeTaggedObject("ess", {{"strategies", essCandidates(game)}});
        return relabelEquilibria(solveNashWithMixedFallback(game), conceptName);
    }
    if (name == "correlated_equilibrium") return correlatedEquilibrium(argAt(args, 0, RuntimeValue()));
    if (name == "perfect_equilibrium" || name == "proper_equilibrium") {
        return relabelEquilibria(solveNashWithMixedFallback(argAt(args, 0, RuntimeValue())), name);
    }
    if (name == "ess" || name == "find_ess") {
        RuntimeValue game = argAt(args, 0, RuntimeValue());
        return makeTaggedObject("ess", {
            {"strategies", essCandidates(game)},
            {"dynamics", objectGetOrNil(game, "dynamics")}
        });
    }
    if (name == "explain_nash") return explainNash(argAt(args, 0, RuntimeValue()));
    if (name == "dominated_strategies") return dominatedStrategies(argAt(args, 0, RuntimeValue()));
    if (name == "find_dominant_strategies") return dominantStrategies(argAt(args, 0, RuntimeValue()));
    if (name == "iterated_elimination") return eliminationSummary(argAt(args, 0, RuntimeValue()));
    if (name == "dominates") {
        RuntimeValue strategy = argAt(args, 0, RuntimeValue());
        RuntimeValue game = argAt(args, 2, argNamedOrAt(args, "game", 1, RuntimeValue()));
        RuntimeValue dominant = dominantStrategies(game);
        std::string target = asString(strategy);
        if (objectType(strategy) == "strategy") target = objectStringField(strategy, "name", target);
        if (!dominant.isObject()) return RuntimeValue(false);
        for (const auto& [player, listValue] : *std::get<RuntimeValue::ObjectPtr>(dominant.data)) {
            (void)player;
            if (!listValue.isList()) continue;
            for (const RuntimeValue& item : *std::get<RuntimeValue::ListPtr>(listValue.data)) {
                if (asString(item) == target) return RuntimeValue(true);
            }
        }
        return RuntimeValue(false);
    }
    if (name == "expected_payoff") {
        return expectedPayoff(argAt(args, 0, RuntimeValue()), argAt(args, 1, RuntimeValue()), argAt(args, 2, RuntimeValue()));
    }
    if (name == "social_welfare") {
        RuntimeValue payoffs = argAt(args, 0, RuntimeValue::list({}));
        if (payoffs.isList()) return callFunction("sum", positionalArgs({payoffs}));
        return RuntimeValue();
    }
    if (name == "pareto_efficient") return paretoEfficientOutcomes(argAt(args, 0, RuntimeValue()));
    if (name == "is_pareto_efficient") {
        RuntimeValue outcome = argAt(args, 0, RuntimeValue());
        RuntimeValue game = argAt(args, 1, RuntimeValue());
        if (game.isNil()) return RuntimeValue(true);
        RuntimeList efficient = asList(paretoEfficientOutcomes(game), "pareto outcomes");
        for (const RuntimeValue& row : efficient) {
            if (valuesEqual(objectGetOrNil(row, "payoffs"), outcome)) return RuntimeValue(true);
        }
        return RuntimeValue(false);
    }
    if (name == "price_of_anarchy") return priceOfAnarchy(argAt(args, 0, RuntimeValue()));
    if (name == "best_response" || name == "best_responses") {
        RuntimeValue game = argAt(args, 0, RuntimeValue());
        int player = static_cast<int>(asNumber(argNamedOrAt(args, "player", 1, RuntimeValue(0)), "best_response player"));
        RuntimeValue against = argNamedOrAt(args, "against", 2, RuntimeValue());
        if (against.isNil()) {
            RuntimeList equilibria = asList(solveNashWithMixedFallback(game), "best_response equilibria");
            return equilibria.empty() ? RuntimeValue::list({}) : objectGetOrNil(equilibria.front(), "strategies");
        }
        return bestResponses(game, player, against);
    }
    if (name == "is_nash") {
        RuntimeValue game = argAt(args, 0, RuntimeValue());
        RuntimeValue profile = argAt(args, 1, RuntimeValue());
        RuntimeList equilibria = asList(solveNashWithMixedFallback(game), "is_nash");
        for (const RuntimeValue& equilibrium : equilibria) {
            if (valuesEqual(objectGetOrNil(equilibrium, "strategies"), profile)) return RuntimeValue(true);
        }
        return RuntimeValue(false);
    }
    if (name == "filter_stable") return argAt(args, 0, RuntimeValue());
    if (name == "rank_by_payoff" || name == "rank_by_welfare") {
        RuntimeList rows = iterableItems(argAt(args, 0, RuntimeValue::list({})));
        std::sort(rows.begin(), rows.end(), [](const RuntimeValue& left, const RuntimeValue& right) {
            auto payoffScore = [](const RuntimeValue& value) {
                RuntimeValue payoffs = objectGetOrNil(value, "payoffs");
                if (!payoffs.isList()) return 0.0;
                double total = 0.0;
                for (const RuntimeValue& payoff : *std::get<RuntimeValue::ListPtr>(payoffs.data)) {
                    if (payoff.isNumber()) total += std::get<double>(payoff.data);
                }
                return total;
            };
            RuntimeValue leftPayoffs = objectGetOrNil(left, "payoffs");
            RuntimeValue rightPayoffs = objectGetOrNil(right, "payoffs");
            (void)leftPayoffs;
            (void)rightPayoffs;
            double leftScore = payoffScore(left);
            double rightScore = payoffScore(right);
            return leftScore > rightScore;
        });
        return RuntimeValue::list(rows);
    }

    if (name == "tournament" || name == "tournament_match") return runTournament(args, randomEngine, parallel);
    if (name == "rank_by_performance" || name == "aggregate_scores") return rankTournament(argAt(args, 0, RuntimeValue()));

    if (name == "sweep") return sweepValues(args);
    if (name == "comparative_statics") return sweepValues(args);
    if (name == "seed") {
        unsigned int seedValue = static_cast<unsigned int>(asNumber(argAt(args, 0, RuntimeValue(0)), "seed"));
        randomEngine.seed(seedValue);
        return RuntimeValue(static_cast<double>(seedValue));
    }
    if (name == "random") {
        return RuntimeValue(std::uniform_real_distribution<double>(0.0, 1.0)(randomEngine));
    }
    if (name == "random_choice") {
        RuntimeList list = asList(argAt(args, 0, RuntimeValue::list({})), "random_choice");
        if (list.empty()) return RuntimeValue();
        std::uniform_int_distribution<size_t> pick(0, list.size() - 1);
        return list[pick(randomEngine)];
    }
    if (name == "uniform") {
        double low = asNumber(argAt(args, 0, RuntimeValue(0)), "uniform low");
        double high = asNumber(argAt(args, 1, RuntimeValue(1)), "uniform high");
        return makeTaggedObject("distribution", {
            {"name", RuntimeValue("uniform")},
            {"low", RuntimeValue(low)},
            {"high", RuntimeValue(high)}
        });
    }
    if (name == "wins") {
        double bid = asNumber(argAt(args, 0, RuntimeValue(0)), "wins bid");
        RuntimeList others = iterableItems(argAt(args, 1, RuntimeValue::list({})));
        for (const RuntimeValue& other : others) {
            if (asNumber(other, "other bid") > bid) return RuntimeValue(false);
        }
        return RuntimeValue(true);
    }
    if (name == "simulate_auction") {
        RuntimeValue mechanism = argAt(args, 0, RuntimeValue());
        int bidders = static_cast<int>(asNumber(objectGetOrNil(mechanism, "bidders").isNil() ? RuntimeValue(2) : objectGetOrNil(mechanism, "bidders"), "auction bidders"));
        double expectedValue = 50.0;
        RuntimeValue values = objectGetOrNil(mechanism, "values");
        if (objectType(values) == "distribution") {
            expectedValue = (asNumber(objectGetOrNil(values, "low"), "uniform low") +
                             asNumber(objectGetOrNil(values, "high"), "uniform high")) / 2.0;
        }
        return makeTaggedObject("auction_result", {
            {"bidders", RuntimeValue(static_cast<double>(bidders))},
            {"expected_revenue", RuntimeValue(expectedValue * bidders / static_cast<double>(bidders + 1))},
            {"mechanism", mechanism}
        });
    }
    if (name == "bid_optimally") return argAt(args, 0, RuntimeValue());
    if (name == "calculate_profit") return argAt(args, 0, RuntimeValue(0));

    if (name == "to_normal_form" || name == "to_strategic_form") {
        return copyWithFields(argAt(args, 0, RuntimeValue()), {
            {"game_type", RuntimeValue(name == "to_normal_form" ? "normal" : "strategic")},
            {"transformation", RuntimeValue(name)}
        });
    }
    if (name == "add_noise" || name == "add_trembling") {
        RuntimeValue game = argAt(args, 0, RuntimeValue());
        double epsilon = asNumber(argAt(args, 1, RuntimeValue(0.0)), name + " epsilon");
        return copyWithFields(game, {
            {name == "add_noise" ? "noise" : "trembling_epsilon", RuntimeValue(epsilon)}
        });
    }
    if (name == "repeat") {
        RuntimeValue game = argAt(args, 0, RuntimeValue());
        return copyWithFields(game, {
            {"game_type", RuntimeValue("repeated")},
            {"rounds", argNamedOrAt(args, "rounds", 1, RuntimeValue(1))},
            {"discount", argNamedOrAt(args, "discount", 2, RuntimeValue(1.0))}
        });
    }
    if (name == "add_private_types") {
        return copyWithFields(argAt(args, 0, RuntimeValue()), {
            {"private_types", argAt(args, 1, RuntimeValue())}
        });
    }
    if (name == "simulate_dynamics") return simulateReplicator(argAt(args, 0, RuntimeValue()), args);
    if (name == "analyze_dynamics") {
        RuntimeValue trajectory = simulateReplicator(argAt(args, 0, RuntimeValue()), args);
        RuntimeList points = objectListField(trajectory, "points");
        return makeTaggedObject("dynamics_analysis", {
            {"trajectory", trajectory},
            {"converged", RuntimeValue(points.size() > 1)}
        });
    }
    if (name == "is_evolutionarily_stable") {
        RuntimeValue strategy = argAt(args, 0, RuntimeValue());
        RuntimeValue game = argAt(args, 1, RuntimeValue());
        RuntimeList candidates = asList(essCandidates(game), "ess candidates");
        for (const RuntimeValue& candidate : candidates) {
            if (asString(candidate) == asString(strategy)) return RuntimeValue(true);
        }
        return RuntimeValue(false);
    }

    if (name == "find_game" || name == "counterexample_search") {
        RuntimeValue game = findCounterexampleGame();
        RuntimeValue threshold = argNamedOrAt(args, "greater_than", 0, RuntimeValue());
        if (!threshold.isNil()) {
            RuntimeValue poa = priceOfAnarchy(game);
            if (poa.isNumber() && std::get<double>(poa.data) <= asNumber(threshold, "counterexample threshold")) {
                return RuntimeValue();
            }
        }
        return game;
    }

    if (name == "export_json") {
        RuntimeValue value = argAt(args, 0, RuntimeValue());
        std::string path = asString(argAt(args, 1, RuntimeValue("gamelang.json")));
        std::ofstream file(path);
        if (!file.is_open()) throw std::runtime_error("Could not open JSON export path: " + path);
        file << valueToJson(value, 0) << "\n";
        return RuntimeValue(path);
    }
    if (name == "export_csv") {
        return exportCsv(argAt(args, 0, RuntimeValue()), asString(argAt(args, 1, RuntimeValue("gamelang.csv"))));
    }
    if (name == "export_dot") {
        return exportDot(argAt(args, 0, RuntimeValue()), asString(argAt(args, 1, RuntimeValue("gamelang.dot"))));
    }
    if (name == "examples") return listExamples();

    throw std::runtime_error("Unknown function: " + name);
}

} // namespace GameLang
