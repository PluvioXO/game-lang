# GameLang - Declarative Game Theory Programming Language

A syntax-dense, declarative programming language specifically designed for game theory analysis and simulation. GameLang combines Python-like dense syntax with powerful game theory primitives to make complex strategic analysis concise and expressive.

## Features

### Syntax-Dense Design
- **List comprehensions**: `[u(s1,s2) | s1,s2 in strategy_pairs if viable(s1,s2)]`
- **Variable unpacking**: `alice, bob := create_players("Alice", "Bob")`
- **Pipeline operators**: `game |> solve_nash() |> filter_stable() |> rank_by_payoff()`
- **Pattern matching**: Advanced match expressions for game analysis
- **Lambda functions**: `utility := (s1, s2) -> payoff_matrix[s1][s2]`

### Game Theory Primitives
- **Native player/strategy types**: Built-in support for game theory concepts
- **Equilibrium solvers**: Nash, dominant strategies, ESS, and more
- **Game transformations**: Normal form, extensive form, population games
- **Payoff analysis**: Welfare, efficiency, dominance relationships

### Rich Type System
- Numbers, strings, booleans with game theory extensions
- Lists, sets, dictionaries with comprehensions
- Specialized types: `player`, `strategy`, `game`, `equilibrium`
- Matrix operations for payoff tables

## Quick Start

### Building GameLang

```bash
# Using Make
make

# Using CMake
mkdir build && cd build
cmake ..
make
```

### Running Programs

```bash
# Interactive REPL
./bin/gamelang

# Execute a file
./bin/gamelang examples/prisoners_dilemma.gl
```

## Language Examples

### Prisoner's Dilemma
```gamelang
# Define players with compact syntax
players := [
    player("Alice", strategies: ["C", "D"]),
    player("Bob", strategies: ["C", "D"])
]

# Payoff matrix using dense notation
pd_game := game(
    players: players,
    payoffs: [
        [3,3 | 0,5],  # (Cooperate, Cooperate) vs (Cooperate, Defect)
        [5,0 | 1,1]   # (Defect, Cooperate) vs (Defect, Defect)
    ]
)

# Find equilibria and analyze
equilibria := solve_nash(pd_game)
dominant := [s | s in strategies if dominates(s, all_strategies)]
```

### Auction Theory
```gamelang
# Parametric auction with bidder types
auction := (cost, demand) -> game {
    players: [player(f"Bidder{i}", valuation: uniform(0, 100)) | i in 1..5],
    mechanism: first_price_sealed_bid,
    payoff: (bid, val, others) -> wins(bid, others) ? (val - bid) : 0
}

# Parameter sweep and analysis
outcomes := {(c, d, simulate_auction(auction(c, d))) 
             | c in 0..10 step 0.5, d in 5..20 step 1}
```

### Evolutionary Dynamics
```gamelang
# Population game with replicator dynamics
hawk_dove := population_game {
    strategies: ["Hawk", "Dove"],
    payoffs: [[V/2 - C/2, V], [0, V/2]] where V := 10, C := 15,
    dynamics: replicator
}

# Simulate and analyze convergence
trajectory := simulate_dynamics(hawk_dove, initial: [0.8, 0.2], time: 0..50)
ess := find_ess(hawk_dove)
```

### Strategy Tournament
```gamelang
# Define strategies with dense syntax
strategies := {
    "TitForTat": (history) -> history ? history[-1].opponent : cooperate,
    "Generous": (history) -> random() < 0.9 ? tit_for_tat(history) : cooperate,
    "Pavlov": (history) -> history[-1].my_payoff >= 3 ? repeat_last() : switch()
}

# Tournament with pipeline processing
results := strategies
    |>> tournament_match(rounds: 200)  # parallel execution
    |> aggregate_scores()
    |> rank_by_performance()
```

## Architecture

GameLang is implemented in C++17 with a modular architecture:

```
src/
├── lexer/          # Tokenization and lexical analysis
├── parser/         # Recursive descent parser (TODO)
├── ast/            # Abstract syntax tree nodes
└── interpreter/    # Runtime, evaluator, builtins, and game analysis
    ├── interpreter.*          # Program execution, statements, bindings
    ├── runtime_value.*        # Runtime value representation and formatting
    ├── runtime_support.*      # Shared coercion, object, and argument helpers
    ├── token_utils.*          # Statement splitting and token helpers
    ├── expression_evaluator.* # Expression parser/evaluator
    ├── builtins.*             # Built-in function dispatch
    └── game_analysis.*        # Validation, Nash, dominance, welfare analysis
```

### Current Implementation Status

 **Completed:**
- Lexical analyzer with game theory keywords
- AST node definitions for all language constructs
- Project structure and build system
- REPL and file execution
- Comprehensive example programs
- Token-driven parser/evaluator for the runnable MVP subset
- Variables, lists, dictionaries, ranges, indexing, member access, `where` bindings, assertions, and pipelines
- Native game/player/strategy/belief/mechanism values
- Pure Nash solving, explanation, validation, dominance analysis, expected mixed-strategy payoff, tournament simulation, sweeps, export helpers, and counterexample search

 **In Progress:**
- Full recursive-descent/AST-backed parser
- Lambda execution, comprehensions, pattern matching, and advanced control flow
- Numeric solvers for mixed Nash, correlated equilibrium, refinement concepts, and ESS dynamics
- Richer diagnostics and source spans

### Runnable MVP Feature Surface

```gamelang
pd := game(
    players: [
        player("Alice", strategies: ["C", "D"]),
        player("Bob", strategies: ["C", "D"])
    ],
    payoffs: [[3,3 | 0,5], [5,0 | 1,1]]
)

print(validate_game(pd))
print(solve_nash(pd))
print(explain_nash(pd))
print(find_dominant_strategies(pd))
print(iterated_elimination(pd))

ranking := {
    TitForTat: "TitForTat",
    Grudger: "Grudger",
    Pavlov: "Pavlov"
} |> tournament(rounds: 100) |> rank_by_performance()

export_csv(ranking, "ranking.csv")
```

Useful REPL commands:

```text
:tokens <source>
:ast <source>
:run <source>
:load examples/feature_showcase.gl
:examples
```

## Game Theory Features

### Equilibrium Concepts
- Nash equilibrium (pure and mixed)
- Dominant strategy equilibrium
- Evolutionarily stable strategies (ESS)
- Correlated equilibrium
- Perfect and proper equilibrium

### Game Types
- **Normal form**: Traditional payoff matrices
- **Extensive form**: Game trees with perfect/imperfect information
- **Population games**: Evolutionary dynamics
- **Mechanism design**: Auctions, voting, matching

### Analysis Tools
- Dominance relationships
- Best response functions
- Welfare analysis (Pareto efficiency, social welfare)
- Stability analysis
- Comparative statics

## Advanced Features

### Dense Syntax Constructs
```gamelang
# Multiple assignment
a, b, c := [1, 2, 3]
{name, strategies} := player_data

# Comprehensions with filters
viable_strategies := {s | s in all_strategies if payoff(s) > threshold}
payoff_matrix := [[u(s1,s2) | s2 in opponent_strats] | s1 in my_strats]

# Pipeline operations
result := data 
    |> transform_payoffs()
    |> solve_equilibrium() 
    |> filter_stable()
    |> rank_by_welfare()

# Pattern matching
action := match game_state {
    {players: 2, zero_sum: T} -> solve_minimax(game),
    {type: "auction"} -> bid_optimally(valuation),
    _ -> general_strategy(game)
}
```

### Function Composition
```gamelang
# Lambda expressions
utility := (my_action, opponent_action) -> payoff_matrix[my_action][opponent_action]
best_response := (beliefs) -> max(strategies, key: s -> expected_payoff(s, beliefs))

# Higher-order functions
strategies := strategies.map(s -> optimize(s, context))
equilibria := games.flat_map(find_all_equilibria)
```

## Documentation

- [Language Specification](LANGUAGE_SPEC.md) - Complete syntax and semantics
- [Game Theory Primer](docs/game_theory.md) - Mathematical background (TODO)
- [API Reference](docs/api.md) - Built-in functions and types (TODO)
- [Tutorial](docs/tutorial.md) - Learning GameLang step by step (TODO)

## Citation

If you use GameLang in research, teaching materials, or a project write-up, please cite the repository:

```text
Nicholson, M. (2026). GameLang: Declarative Game Theory Programming Language.
GitHub. https://github.com/PluvioXO/game-lang
```

BibTeX:

```bibtex
@software{nicholson_2026_gamelang,
  author = {Nicholson, Maximilian},
  title = {GameLang: Declarative Game Theory Programming Language},
  year = {2026},
  publisher = {GitHub},
  url = {https://github.com/PluvioXO/game-lang}
}
```

## Contributing

GameLang is an open-source project. Contributions are welcome!

### Development Roadmap
1. **Parser Implementation** - Complete recursive descent parser
2. **Interpreter Core** - Expression evaluation and statement execution
3. **Game Theory Library** - Built-in equilibrium solvers and analysis tools
4. **Type System** - Static type checking for game theory constructs
5. **Performance** - Optimization for large-scale simulations
6. **Ecosystem** - Package manager, libraries, IDE support

### Getting Started
```bash
git clone https://github.com/PluvioXO/game-lang.git
cd game-lang
make
./bin/gamelang examples/prisoners_dilemma.gl
```

## License

MIT License - see [LICENSE](LICENSE) file for details.

## Use Cases

### Academic Research
- Game theory simulations and analysis
- Mechanism design experiments
- Behavioral economics modeling
- Evolutionary game theory research

### Industry Applications
- Auction design and optimization
- Strategic pricing models
- Market analysis and competition
- Algorithm design for strategic environments

### Education
- Interactive game theory teaching
- Strategy tournament competitions
- Economic simulation projects
- Research methodology training

---

**GameLang** - Making game theory analysis as expressive as the mathematics behind it.

For feature requests please issue a issue ticket!
