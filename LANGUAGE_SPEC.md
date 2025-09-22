# GameLang Language Specification

GameLang is a declarative, syntax-dense programming language optimized for game theory analysis and simulation.

## Core Philosophy

GameLang emphasizes:
- **Declarative game definitions** - Define what the game is, not how to compute it
- **Dense syntax** - Maximum expressiveness with minimal code
- **Built-in game theory primitives** - Native support for players, strategies, payoffs, equilibria
- **Functional composition** - Combine simple games into complex scenarios

## Data Types

### Primitive Types
- `num` - Numbers (integer or float)
- `str` - Strings
- `bool` - Boolean values (`T`/`F`)
- `nil` - Null value

### Game Theory Types
- `player` - Game participant with strategies and utilities
- `strategy` - Player action or mixed strategy
- `payoff` - Utility matrix or function
- `game` - Complete game definition
- `outcome` - Game result with payoffs
- `equilibrium` - Nash/other equilibrium solutions

### Collections
- `list` - Dynamic arrays with comprehensions
- `set` - Unique element collections
- `dict` - Key-value mappings
- `matrix` - N-dimensional payoff matrices

## Variable Declaration & Unpacking

```gamelang
# Simple assignment
x := 42
name := "Prisoner"

# Multiple assignment (unpacking)
a, b, c := [1, 2, 3]
player1, player2 := create_players("Alice", "Bob")

# Destructuring with patterns
{name, strategy} := player1
[first, *rest] := strategies
```

## Dense Syntax Features

### List Comprehensions
```gamelang
# Basic comprehension
squares := [x^2 | x in 1..10]

# With filtering
evens := [x | x in 1..20 if x % 2 == 0]

# Nested comprehensions for payoff matrices
payoffs := [[u(s1,s2) | s2 in opponent_strategies] | s1 in my_strategies]

# Set comprehensions
dominant_strategies := {s | s in strategies if dominates(s, all_other_strategies)}
```

### Generator Expressions
```gamelang
# Memory-efficient iterations
sum_payoffs := sum(u(s1,s2) | s1,s2 in strategy_pairs)
best_response := max(strategies, key: s -> expected_payoff(s, beliefs))
```

### Lambda Functions
```gamelang
# Inline functions
utility := (s1, s2) -> payoff_matrix[s1][s2]
dominant := s -> all(utility(s, o) >= utility(alt, o) | alt in alternatives, o in opponent_strats)

# Function composition
compose := (f, g) -> x -> f(g(x))
```

## Game Theory Primitives

### Player Definition
```gamelang
# Simple player
p1 := player("Alice", strategies: ["Cooperate", "Defect"])

# Player with utility function
p2 := player("Bob") {
    strategies: ["High", "Low"],
    utility: (my_action, others) -> calculate_profit(my_action, others),
    beliefs: uniform_over(opponent_strategies)
}

# Player types with inheritance
rational_player := player_type {
    decision_rule: best_response,
    belief_update: bayesian
}
```

### Strategy Definitions
```gamelang
# Pure strategies
cooperate := strategy("Cooperate")
defect := strategy("Defect")

# Mixed strategies with probabilities
mixed := strategy(cooperate: 0.7, defect: 0.3)

# Behavioral strategies (extensive form)
tit_for_tat := strategy {
    round_1: cooperate,
    round_n: (history) -> history[-1].opponent_action
}

# Strategy sets
all_strategies := {pure(action) | action in ["C", "D"]} ∪ 
                 {mixed(p) | p in 0..1 step 0.1}
```

### Game Definitions
```gamelang
# Normal form game (compact notation)
prisoners_dilemma := game {
    players: ["Alice", "Bob"],
    strategies: {
        "Alice": ["C", "D"],
        "Bob": ["C", "D"]
    },
    payoffs: [
        [(3,3), (0,5)],  # Alice cooperates
        [(5,0), (1,1)]   # Alice defects
    ]
}

# Matrix notation for 2x2 games
pd := game(players: 2, matrix: [[3,3|0,5], [5,0|1,1]])

# Extensive form game
centipede := extensive_game {
    initial_node: root,
    nodes: build_tree(depth: 4),
    info_sets: perfect_recall,
    payoffs: (100-10*t, t) at terminal(t)
}

# Population games
hawk_dove := population_game {
    strategies: ["Hawk", "Dove"],
    fitness: (s, pop) -> expected_payoff(s, pop.distribution),
    dynamics: replicator
}
```

## Advanced Syntax

### Pattern Matching
```gamelang
# Match on game types
analyze := (g) -> match g {
    game(players: 2, zero_sum: T) -> solve_zero_sum(g),
    game(players: n) if n > 2 -> coalition_analysis(g),
    extensive_game(_) -> backward_induction(g),
    population_game(_) -> evolutionary_stable_strategies(g),
    _ -> general_nash_equilibrium(g)
}

# Pattern matching on outcomes
evaluate_outcome := match outcome {
    (payoff1, payoff2) if payoff1 > payoff2 -> "Player 1 wins",
    (p1, p2) if abs(p1 - p2) < 0.01 -> "Tie",
    _ -> "Player 2 wins"
}
```

### Conditional Expressions
```gamelang
# Ternary operator
action := payoff > threshold ? cooperate : defect

# Multi-way conditionals
strategy := 
    | reputation > 0.8 -> always_cooperate
    | reputation < 0.2 -> always_defect  
    | _ -> tit_for_tat
```

### Pipeline Operators
```gamelang
# Function chaining
result := game 
    |> add_noise(0.1)
    |> solve_nash() 
    |> filter_stable()
    |> rank_by_payoff()

# Parallel computation
equilibria := strategies 
    |>> test_equilibrium  # parallel map
    |> filter(is_nash)
    |> collect()
```

## Built-in Game Theory Functions

### Equilibrium Concepts
```gamelang
# Nash equilibrium
nash := solve_nash(game, algorithm: "lemke_howson")
all_nash := find_all_nash(game)

# Other solution concepts
dominant := find_dominant_strategies(game)
correlated := correlated_equilibrium(game)
evolutionary := ess(population_game)

# Refinements
perfect := perfect_equilibrium(extensive_game)
proper := proper_equilibrium(game, epsilon: 0.01)
```

### Game Transformations
```gamelang
# Convert between forms
normal := to_normal_form(extensive_game)
strategic := to_strategic_form(coalitional_game)

# Game modifications
noisy := add_trembling(game, epsilon: 0.05)
repeated := repeat(game, rounds: 100, discount: 0.95)
incomplete := add_private_types(game, type_dist)
```

### Analysis Functions
```gamelang
# Welfare and efficiency
social_welfare := sum(payoffs)
pareto_efficient := is_pareto_efficient(outcome)
price_of_anarchy := max_welfare / nash_welfare

# Stability and dynamics
stable := is_evolutionarily_stable(strategy, population)
converges := analyze_dynamics(game, initial_state, time_horizon: 1000)
```

## Example Programs

### Prisoner's Dilemma Tournament
```gamelang
# Define strategies compactly
strategies := {
    "TitForTat": (history) -> history ? history[-1].opponent : cooperate,
    "Generous": (history) -> random() < 0.9 ? tit_for_tat(history) : cooperate,
    "Pavlov": (history) -> history[-1].my_payoff >= 3 ? repeat_last() : switch(),
    "Random": (_) -> random_choice([cooperate, defect])
}

# Tournament with all pairings
tournament := {play_match(s1, s2, rounds: 200) 
               | s1, s2 in strategies.items() 
               if s1 != s2}

# Analyze results
rankings := tournament
    |> aggregate_scores()
    |> rank_by_total_payoff()
    |> with_confidence_intervals()

winners := [name | name, score in rankings if score.rank <= 3]
```

### Market Entry Game
```gamelang
# Parametric game family
market_entry := (cost, demand) -> game {
    players: ["Incumbent", "Entrant"],
    strategies: {
        "Incumbent": ["Accommodate", "Fight"],
        "Entrant": ["Enter", "Stay_Out"]
    },
    payoffs: [
        # Enter, Stay_Out
        [(demand/2 - cost, demand/2), (demand, 0)],      # Accommodate  
        [(-cost, demand - cost), (demand, 0)]            # Fight
    ]
}

# Parameter sweep
equilibria := {(c, d, solve_nash(market_entry(c, d))) 
               | c in 0..10 step 0.5, d in 5..20 step 1}

# Find entry deterrence region  
deterrence_region := {(c, d) | (c, d, eq) in equilibria 
                     if eq.entrant_strategy == "Stay_Out"}
```

### Evolutionary Game Dynamics
```gamelang
# Rock-Paper-Scissors population game
rps := population_game {
    strategies: ["Rock", "Paper", "Scissors"],
    payoffs: cyclic_matrix(win: 1, lose: -1, tie: 0),
    population: uniform_distribution(3)
}

# Simulate dynamics
trajectory := simulate(rps, 
    dynamics: replicator,
    initial: [0.5, 0.3, 0.2],
    time: 0..100 step 0.1
)

# Find fixed points and analyze stability
fixed_points := find_fixed_points(rps.dynamics)
stability := {fp: analyze_stability(rps, fp) | fp in fixed_points}
```

## Grammar (EBNF)

```ebnf
program        = statement* EOF ;

statement      = exprStmt
               | varDecl
               | funDecl
               | ifStmt
               | whileStmt
               | forStmt
               | returnStmt
               | blockStmt ;

exprStmt       = expression ";" ;
varDecl        = "let" IDENTIFIER ( "=" expression )? ";" ;
funDecl        = "fun" IDENTIFIER "(" parameters? ")" blockStmt ;
ifStmt         = "if" "(" expression ")" statement ( "else" statement )? ;
whileStmt      = "while" "(" expression ")" statement ;
forStmt        = "for" "(" ( varDecl | exprStmt | ";" )
                           expression? ";"
                           expression? ")" statement ;
returnStmt     = "return" expression? ";" ;
blockStmt      = "{" statement* "}" ;

expression     = assignment ;
assignment     = IDENTIFIER ( "=" | "+=" | "-=" | "*=" | "/=" ) assignment
               | ternary ;
ternary        = logicalOr ( "?" expression ":" expression )? ;
logicalOr      = logicalAnd ( "||" logicalAnd )* ;
logicalAnd     = equality ( "&&" equality )* ;
equality       = comparison ( ( "!=" | "==" ) comparison )* ;
comparison     = term ( ( ">" | ">=" | "<" | "<=" ) term )* ;
term           = factor ( ( "-" | "+" ) factor )* ;
factor         = unary ( ( "/" | "*" | "%" ) unary )* ;
unary          = ( "!" | "-" ) unary | exponent ;
exponent       = postfix ( "^" unary )* ;
postfix        = primary ( "(" arguments? ")" | "[" expression "]" | "." IDENTIFIER )* ;
primary        = "true" | "false" | "null"
               | NUMBER | STRING | IDENTIFIER
               | "(" expression ")"
               | "[" arguments? "]"
               | "{" objectFields? "}" ;

parameters     = IDENTIFIER ( "," IDENTIFIER )* ;
arguments      = expression ( "," expression )* ;
objectFields   = IDENTIFIER ":" expression ( "," IDENTIFIER ":" expression )* ;
```

## Keywords

- `let` - variable declaration
- `fun` - function declaration
- `if` - conditional statement
- `else` - alternative branch
- `while` - while loop
- `for` - for loop
- `in` - for-each iterator
- `return` - return statement
- `true` - boolean literal
- `false` - boolean literal
- `null` - null literal

## Example Program

```gamelang
// Calculate factorial
fun factorial(n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

// Game character system
let player = {
    name: "Hero",
    health: 100,
    inventory: ["sword", "potion"]
};

fun takeDamage(amount) {
    player.health -= amount;
    if (player.health <= 0) {
        print("Game Over!");
        return false;
    }
    return true;
}

// Main game loop
let gameRunning = true;
while (gameRunning) {
    let damage = 10;
    gameRunning = takeDamage(damage);
    
    if (gameRunning) {
        print("Health: " + toString(player.health));
    }
}

print("Final score: " + toString(factorial(5)));
```