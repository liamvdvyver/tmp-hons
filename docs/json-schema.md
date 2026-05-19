# Planner JSON input/output schema

This document describes the JSON formats accepted and produced by `planner`.

## Input schema

The planner expects a single JSON object with two top-level fields:

```json
{
  "domain": { ... },
  "problem": { ... }
}
```

## Domain

```json
{
  "constants": ["optional", "object", "names"],
  "predicates": {
    "predicate_name": ["arg1", "arg2"]
  },
  "actions": {
    "action_name": {
      "terms": ["?x", "?y"],
      "pre": { ...formula... },
      "eff": { ...formula... }
    }
  }
}
```

### `constants`
- optional
- array of object names available in every problem

### `predicates`
- object mapping predicate names to a parameter list
- the parameter names are only used to indicate arity

Example:
```json
{
  "on": ["?x", "?y"],
  "clear": ["?x"]
}
```

### `actions`
- object mapping action names to action definitions

Each action definition contains:
- `terms`: parameter names
- `pre`: precondition formula
- `eff`: effect formula

## Problem

```json
{
  "objects": ["problem", "specific", "objects"],
  "init": [ ...atoms... ],
  "goal": { ...formula... }
}
```

### `objects`
- optional
- array of object names local to this problem

### `init`
- array of atoms that are true initially
- any grounded predicate not listed here is treated as false initially

### `goal`
- a formula

## Atom schema

An atom is represented as:

```json
{
  "pred": ["predicate_name", ["arg1", "arg2"]]
}
```

Example:
```json
{
  "pred": ["on", ["a", "b"]]
}
```

## Formula schema

A formula may be one of:

### Empty / null
```json
null
```
or an empty object/array, which is treated as trivially true by the current implementation.

### Atomic formula
```json
{
  "pred": ["predicate_name", ["arg1", "arg2"]]
}
```

### Negation
```json
{
  "not": { ...formula... }
}
```

### Conjunction
```json
{
  "and": [
    { ...formula... },
    { ...formula... }
  ]
}
```

### Disjunction
```json
{
  "or": [
    { ...formula... },
    { ...formula... }
  ]
}
```

## Complete input example

```json
{
  "domain": {
    "constants": ["table"],
    "predicates": {
      "on": ["?x", "?y"],
      "clear": ["?x"],
      "holding": ["?x"]
    },
    "actions": {
      "pick-up": {
        "terms": ["?x"],
        "pre": {
          "and": [
            { "pred": ["clear", ["?x"]] },
            { "pred": ["on", ["?x", "table"]] }
          ]
        },
        "eff": {
          "and": [
            { "pred": ["holding", ["?x"]] },
            { "not": { "pred": ["on", ["?x", "table"]] } }
          ]
        }
      }
    }
  },
  "problem": {
    "objects": ["a", "b"],
    "init": [
      { "pred": ["clear", ["a"]] },
      { "pred": ["clear", ["b"]] },
      { "pred": ["on", ["a", "table"]] },
      { "pred": ["on", ["b", "table"]] }
    ],
    "goal": {
      "pred": ["holding", ["a"]]
    }
  }
}
```

## Output schema (`--json-out` / `-j`)

When JSON output is enabled, the planner prints and writes plans in this format:

```json
{
  "plan": [
    [
      {
        "name": "action_name",
        "arguments": ["arg1", "arg2"]
      }
    ],
    [
      {
        "name": "parallel_action_1",
        "arguments": []
      },
      {
        "name": "parallel_action_2",
        "arguments": ["x"]
      }
    ]
  ]
}
```

## Output interpretation

- `plan` is an array of time steps
- each time step is an array of chosen actions
- each action object contains:
  - `name`: action name
  - `arguments`: grounded object names in argument order

For sequential plans, each time step usually contains exactly one action.
For partially ordered plans, a time step may contain multiple actions.
