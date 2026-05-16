# GameLang Tutorial

## 1. Define A Game

```gamelang
pd := game(
    players: [
        player("Alice", strategies: ["C", "D"]),
        player("Bob", strategies: ["C", "D"])
    ],
    payoffs: [[3,3 | 0,5], [5,0 | 1,1]]
)
```

## 2. Solve And Explain

```gamelang
print(validate_game(pd))
print(solve_nash(pd))
print(explain_nash(pd))
```

## 3. Use Dense Syntax

```gamelang
scores := [x^2 | x in 1..5 if x > 2]
labels := [f"Bidder{i}" | i in 1..3]
best := scores |> max()
```

## 4. Run A Tournament

```gamelang
strategies := {
    TitForTat: "TitForTat",
    Pavlov: "Pavlov",
    Random: "Random"
}

ranking := strategies
    |>> tournament(rounds: 100)
    |> rank_by_performance()
```

## 5. Export Results

```gamelang
export_csv(ranking, "ranking.csv")
export_json(solve_nash(pd), "pd_nash.json")
export_dot(pd, "pd.dot")
```
