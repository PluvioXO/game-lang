# GameLang API Reference

## Core Values

- `num(value)`, `str(value)`, `bool(value)`, `list(value)`, `set(value)`,
  `dict(value)`, and `matrix(value)` construct or coerce runtime values.
- `len(value)`, `sum(values)`, `median(values)`, `flatten(values)`, `max(values,
  key: fn)`, `all(values, fn)`, `map`, `flat_map`, `filter`, and `collect`
  support collection processing.

## Game Construction

- `player(name, strategies: [...])`
- `strategy(name)` and `strategy(A: 0.7, B: 0.3)`
- `game(players: ..., payoffs: ...)`
- `population_game(...)` and `extensive_game(...)`
- `auction(...)`, `voting(...)`, `matching(...)`, and `mechanism(...)`

## Analysis

- `validate_game(game)`
- `solve_nash(game)`, `find_all_nash(game)`, and `solve(game, concept: ...)`
- `correlated_equilibrium(game)`, `perfect_equilibrium(game)`,
  `proper_equilibrium(game)`, and `find_ess(game)`
- `explain_nash(game)`, `best_response(game, ...)`, `is_nash(game, profile)`
- `dominated_strategies(game)`, `find_dominant_strategies(game)`,
  `iterated_elimination(game)`, and `dominates(strategy, game: game)`
- `expected_payoff(strategy1, strategy2, game)`
- `social_welfare(payoffs)`, `pareto_efficient(game)`,
  `is_pareto_efficient(payoffs, game)`, and `price_of_anarchy(game)`

## Simulation And Transformation

- `tournament(strategies, rounds: n)` and `rank_by_performance(results)`
- `sweep(subject, values: [...], param: "name", metric: "price_of_anarchy")`
- `simulate_dynamics(game, initial: [...], steps: n)`
- `analyze_dynamics(game, initial: [...], steps: n)`
- `to_normal_form(game)`, `to_strategic_form(game)`, `add_noise(game, eps)`,
  `add_trembling(game, eps)`, `repeat(game, rounds: n)`, and
  `add_private_types(game, types)`
- `simulate_auction(auction(...))`

## Export

- `export_csv(value, path)`
- `export_json(value, path)`
- `export_dot(game, path)`
