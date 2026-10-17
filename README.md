# 0/1 Knapsack – Dynamic Programming with live ASCII visualization (C)

Reads test cases from a text file, solves each with the 0/1 knapsack DP,
draws the DP table live in the terminal (cell by cell, then the traceback),
and stores the results on disk.

## Input format
```
#Test
n W          <- number of items, capacity
w1 v1        <- weight value (one line per item)
...
```

## Build & run
```
gcc -O2 -Wall -o knapsack knapsack.c
./knapsack [input_file] [delay_ms]      # defaults: knapsack_data.txt  40
./knapsack knapsack_data.txt 0          # no animation delay
```
Use a terminal with ANSI colour support (Linux/macOS terminal, Windows Terminal, VS Code terminal).
Wide tables (e.g. W = 50) scroll horizontally to fit the terminal width.

## Output
- `result.txt` – all results, one line per test:  
  `#Test <case> input (<n> <W>) result <max value>`
- `test_1/result.txt`, `test_2/result.txt`, ... – one folder per test case, each with its own
  `result.txt` (result line, items taken, final DP table).

## Results for the supplied data
| Test | (n W) | Max value |
|------|-------|-----------|
| 1 | (4 10) | 150 |
| 2 | (5 15) | 100 |
| 3 | (6 12) | 150 |
| 4 | (5 8) | 90 |
| 5 | (7 20) | 122 |
| 6 | (4 6) | 90 |
| 7 | (5 25) | 47 |
| 8 | (6 18) | 95 |
| 9 | (8 30) | 290 |
| 10 | (10 50) | 100 |
