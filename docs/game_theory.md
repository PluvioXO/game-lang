# Game Theory Primer

This primer summarizes the finite-game concepts implemented by the runnable
GameLang MVP.

## Normal-Form Games

A normal-form game lists players, each player's available strategies, and the
payoffs for every strategy profile. GameLang represents two-player payoff
tables as rows for player one and columns for player two.

## Nash Equilibrium

A Nash equilibrium is a strategy profile where no player can improve by
changing only their own strategy. GameLang solves pure equilibria for finite
one- and two-player normal-form games, with a two-by-two mixed equilibrium
fallback when no pure equilibrium exists.

## Dominance

A strategy is dominated when another strategy is always at least as good and
sometimes better against the opponent's strategies. The MVP includes direct
dominance checks and an iterated-elimination summary.

## Welfare And Efficiency

Social welfare is the sum of player payoffs for an outcome. Pareto-efficient
outcomes are those where no other outcome improves one player without making
another player worse off.

## Evolutionary Stability

The MVP checks pure ESS candidates in symmetric two-strategy population games
and includes a simple discrete replicator-dynamics simulation.
