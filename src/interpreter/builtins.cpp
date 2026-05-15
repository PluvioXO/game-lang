#include "interpreter.h"

#include "game_analysis.h"
#include "runtime_support.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
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
    RuntimeValue strategiesArg = argNamedOrAt(args, "strategies", 1, RuntimeValue());
    if (strategiesArg.isObject()) {
        strategiesObject = *std::get<RuntimeValue::ObjectPtr>(strategiesArg.data);
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

RuntimeValue runTournament(const std::vector<CallArg>& args, std::mt19937& rng) {
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
        {"matches", RuntimeValue::list(matches)}
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
    (void)parallel;

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

    if (name == "player") return createPlayer(args);
    if (name == "strategy") return createStrategy(args);
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
    if (name == "solve_nash" || name == "find_all_nash") return solvePureNash(argAt(args, 0, RuntimeValue()));
    if (name == "solve") {
        RuntimeValue game = argAt(args, 0, RuntimeValue());
        std::string conceptName = asString(argNamedOrAt(args, "concept", 1, RuntimeValue("nash")));
        if (conceptName == "nash" || conceptName == "pure" || conceptName == "pure_nash") return solvePureNash(game);
        return RuntimeValue::list({
            makeTaggedObject("equilibrium", {
                {"concept", RuntimeValue(conceptName)},
                {"note", RuntimeValue("MVP placeholder: refinement-specific solver is registered but not numerically implemented yet.")}
            })
        });
    }
    if (name == "correlated_equilibrium" || name == "perfect_equilibrium" || name == "proper_equilibrium") {
        return RuntimeValue::list({
            makeTaggedObject("equilibrium", {
                {"concept", RuntimeValue(name)},
                {"note", RuntimeValue("MVP placeholder for advanced equilibrium refinements.")}
            })
        });
    }
    if (name == "ess" || name == "find_ess") {
        RuntimeValue game = argAt(args, 0, RuntimeValue());
        return makeTaggedObject("ess", {
            {"strategies", objectGetOrNil(game, "strategies")},
            {"note", RuntimeValue("MVP ESS hook; use population_game plus payoffs for future dynamic checks.")}
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
    if (name == "price_of_anarchy") return priceOfAnarchy(argAt(args, 0, RuntimeValue()));

    if (name == "tournament" || name == "tournament_match") return runTournament(args, randomEngine);
    if (name == "rank_by_performance" || name == "aggregate_scores") return rankTournament(argAt(args, 0, RuntimeValue()));

    if (name == "sweep") return sweepValues(args);
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
