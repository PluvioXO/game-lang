#include "interpreter/interpreter.h"
#include "interpreter/runtime_support.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace GameLang;

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void requireNumber(const RuntimeValue& value, double expected, const std::string& message) {
    require(value.isNumber(), message + " is not a number");
    require(std::abs(std::get<double>(value.data) - expected) < 1e-9, message);
}

RuntimeValue eval(Interpreter& interpreter, const std::string& source) {
    return interpreter.evaluateExpressionSource(source);
}

void denseSyntaxFeatures() {
    Interpreter interpreter;

    RuntimeList evens = asList(eval(interpreter, "[x^2 | x in 1..4 if x % 2 == 0]"), "list comprehension");
    require(evens.size() == 2, "list comprehension size");
    requireNumber(evens[0], 4, "first list comprehension value");
    requireNumber(evens[1], 16, "second list comprehension value");

    RuntimeValue setValue = eval(interpreter, "{x | x in [1,1,2,3]}");
    require(objectType(setValue) == "set", "set comprehension type");
    require(objectListField(setValue, "values").size() == 3, "set comprehension uniqueness");

    requireNumber(eval(interpreter, "sum(x | x in 1..3)"), 6, "generator expression sum");

    interpreter.execute("f := (x) -> x + 1", false);
    requireNumber(eval(interpreter, "f(4)"), 5, "lambda call");

    RuntimeList mapped = asList(eval(interpreter, "[1,2,3].map(x -> x * 2)"), "map result");
    require(mapped.size() == 3, "map size");
    requireNumber(mapped[2], 6, "map third value");

    RuntimeList filtered = asList(eval(interpreter, "[1,2,3].filter(x -> x > 1)"), "filter result");
    require(filtered.size() == 2, "filter size");
    requireNumber(filtered[0], 2, "filter first value");

    interpreter.execute("i := 7", false);
    require(asString(eval(interpreter, "f\"Bidder{i}\"")) == "Bidder7", "f-string interpolation");
}

void destructuringAndMatch() {
    Interpreter interpreter;
    interpreter.execute(R"(
player_data := {name: "Alice", strategies: ["C", "D"]}
{name, strategies} := player_data
[first, *rest] := [1, 2, 3]
)", false);

    require(asString(interpreter.getVariable("name")) == "Alice", "object destructuring name");
    require(asList(interpreter.getVariable("strategies"), "strategies").size() == 2, "object destructuring strategies");
    requireNumber(interpreter.getVariable("first"), 1, "list destructuring first");
    require(asList(interpreter.getVariable("rest"), "rest").size() == 2, "list destructuring rest");

    RuntimeValue matched = eval(interpreter, R"(match game(players: 2, matrix: [[1,1|0,0], [0,0|1,1]]) {
        game(players: n) if n == 2 -> "two",
        _ -> "other"
    })");
    require(asString(matched) == "two", "match object pattern and guard");
}

void controlFlowFeatures() {
    Interpreter interpreter;
    interpreter.execute(R"(
total := 0
for x in 1..4 {
    total := total + x
}
if total == 10 {
    label := "ok"
} else {
    label := "bad"
}
while total < 13 {
    total := total + 1
}
)", false);

    requireNumber(interpreter.getVariable("total"), 13, "while/for total");
    require(asString(interpreter.getVariable("label")) == "ok", "if/else branch");
}

void gameTheoryFeatures() {
    Interpreter interpreter;
    interpreter.execute(R"(
matching_pennies := game { players: 2, matrix: [[1,-1|-1,1], [-1,1|1,-1]] }
pd := game(
    players: [
        player("Alice", strategies: ["C", "D"]),
        player("Bob", strategies: ["C", "D"])
    ],
    payoffs: [[3,3 | 0,5], [5,0 | 1,1]]
)
)", false);

    RuntimeValue validation = eval(interpreter, "validate_game(matching_pennies)");
    require(isTruthy(objectGetOrNil(validation, "valid")), "numeric player strategy inference validates");

    RuntimeList mixed = asList(eval(interpreter, "solve_nash(matching_pennies)"), "mixed nash");
    require(mixed.size() == 1, "mixed nash count");
    require(asString(objectGetOrNil(mixed[0], "concept")) == "mixed_nash", "mixed nash concept");

    RuntimeList correlated = asList(eval(interpreter, "correlated_equilibrium(pd)"), "correlated equilibrium");
    require(correlated.size() == 1, "correlated equilibrium count");
    require(!objectListField(correlated[0], "support").empty(), "correlated equilibrium support");

    RuntimeValue ess = eval(interpreter, "find_ess(pd)");
    require(objectType(ess) == "ess", "ess result type");
    require(objectGetOrNil(ess, "strategies").isList(), "ess strategies list");

    RuntimeValue trajectory = eval(interpreter, "simulate_dynamics(pd, initial: [0.5, 0.5], steps: 3)");
    require(objectType(trajectory) == "trajectory", "trajectory type");
    require(objectListField(trajectory, "points").size() == 4, "trajectory point count");

    RuntimeValue transformed = eval(interpreter, "pd |> add_trembling(0.05) |> repeat(rounds: 10) |> to_normal_form()");
    require(asString(objectGetOrNil(transformed, "game_type")) == "normal", "game transformation pipeline");

    RuntimeList pareto = asList(eval(interpreter, "pareto_efficient(pd)"), "pareto efficient outcomes");
    require(!pareto.empty(), "pareto efficient outcomes");

    RuntimeValue ranking = eval(interpreter, "{TitForTat: \"TitForTat\", Pavlov: \"Pavlov\"} |>> tournament_match(rounds: 5)");
    require(objectType(ranking) == "tournament_results", "parallel tournament result type");
    require(isTruthy(objectGetOrNil(ranking, "parallel")), "parallel pipeline flag");
}

void typeAndMechanismFeatures() {
    Interpreter interpreter;
    requireNumber(eval(interpreter, "num(\"4\")"), 4, "num conversion");
    require(asString(eval(interpreter, "str(4)")) == "4", "str conversion");
    require(isTruthy(eval(interpreter, "bool(1)")), "bool conversion");
    require(objectType(eval(interpreter, "matrix([[1,2],[3,4]])")) == "matrix", "matrix value");

    RuntimeValue auctionResult = eval(interpreter, "simulate_auction(auction(first_price_sealed_bid, bidders: 3, values: uniform(0, 90)))");
    require(objectType(auctionResult) == "auction_result", "auction simulation result");
}

} // namespace

int main() {
    try {
        denseSyntaxFeatures();
        destructuringAndMatch();
        controlFlowFeatures();
        gameTheoryFeatures();
        typeAndMechanismFeatures();
    } catch (const std::exception& error) {
        std::cerr << "Test failed: " << error.what() << "\n";
        return 1;
    }
    return 0;
}
