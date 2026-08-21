# NASDAQ ITCH 5.0 Limit Order Book & Matching Engine

A C++20 limit order book reconstructed from raw NASDAQ TotalView-ITCH 5.0 binary market data, plus an independent price-time priority matching engine validated against real exchange executions.

Built and validated against a full trading day of real NASDAQ data (~269 million messages, 8,906 symbols). The project emphasizes measurable correctness: each major claim is backed by a validation methodology rather than simply asserted.

---

## Summary

The project reconstructs a full-day NASDAQ order book directly from raw ITCH 5.0 binary data and independently validates its price-time ordering against millions of real exchange executions.

Key results:

```text
~269M       ITCH messages processed
~8,906      symbols
0           missing order references
98.4%       shadow execution-order agreement
~36M        raw mmap parsing msgs/sec
~1.5–1.6M   parsing + book maintenance msgs/sec
~610K       full validation instrumentation msgs/sec
~4.97M      standalone matching-engine orders/sec
83 ns       matching-engine P50 latency
570 ns      matching-engine P99 latency
```

---

## What's in here

- **`src/parser.cpp`** — mmap-based ITCH 5.0 binary parser.
- **`src/itch_messages.hpp`** — zero-copy decoding for the ITCH message types used (`S`, `R`, `A`, `F`, `E`, `C`, `X`, `D`, `U`, `H`).
- **`src/order_book.hpp`** — passive limit order book that mirrors exchange state message-by-message.
- **`src/matching_engine.hpp`** — independent price-time priority matching engine (`submit`, `cancel`, `replace`).
- **`test/test_matching_engine.cpp`** — 12 unit tests covering 8 behavioral categories.
- **`bench/bench_matching_engine.cpp`** — matching-engine throughput and P50/P99/P99.9 latency benchmark.

---

## Architecture

```text
   NASDAQ ITCH 5.0 file (binary, mmap'd)
              |
              v
       itch_messages.hpp
       (zero-copy decode)
              |
              v
         parser.cpp
       (message loop)
          /        \
         v          v
   order_book.hpp   matching_engine.hpp
   (passive mirror) ("shadow" engine)
         |                  |
         v                  v
   book/integrity       independent
      checks           price-time logic
                            |
                            v
                    execution comparison
                            |
                            v
                      98.4% agreement
````

`OrderBook` and `MatchingEngine` are deliberately separate components.

The `OrderBook` passively reconstructs the state reported by the exchange. The `MatchingEngine` independently applies price-time priority to resting orders and is not fed the exchange's execution decisions.

This separation makes the shadow-matching validation meaningful: the two systems are not simply checking themselves against the same logic.

---

## Validation methodology

Three independent checks were used, each targeting a different class of correctness issue.

### 1. Referential integrity

Every `E`/`C`/`X`/`D`/`U` message must reference an order that actually exists in the reconstructed book.

**Result: 0 missing order references** across approximately 269M messages and 263M applied book operations.

---

### 2. Book invariant checking

Once a symbol is actively trading:

```text
best_bid < best_ask
```

should normally hold.

Initial validation produced thousands of apparent violations, which were investigated rather than simply ignored.

| Validation stage             |                    Result | Issue identified                                           |
| ---------------------------- | ------------------------: | ---------------------------------------------------------- |
| Initial                      |                     8,649 | `bid >= ask` incorrectly treated locked markets as crossed |
| Crossed/locked split         | 8,579 crossed / 70 locked | Locked markets can legitimately occur                      |
| Global market-open gate      |                     8,535 | Symbols do not all begin trading simultaneously            |
| Per-symbol `H` trading state |                   **634** | Each symbol has its own trading state/opening process      |

The remaining 634 symbols were investigated with before/after message logging.

A recurring pattern was:

```text
execution
    ↓
new order at same price + same side
    ↓
very short time interval
    ↓
another execution
    ↓
another replenishment
```

Some chains contained hundreds of replenishments.

This is consistent with reserve/iceberg liquidity being replenished by the exchange. ITCH does not expose a flag identifying an order as an iceberg or reveal its hidden reserve quantity, so this is an inference from observed message behavior rather than a claim that the feed explicitly identifies icebergs.

---

### 3. Shadow matching against real executions

For every real exchange execution (`E`/`C`), the independent per-symbol `MatchingEngine` is checked:

> Was the order the exchange executed actually at the front of the matching engine's price/side queue?

The shadow engine receives displayed order additions and lifecycle events (`A`/`F`/`X`/`D`/`U`), but does **not** receive the exchange's execution decisions.

### Result

```text
Shadow agree:       5,729,669
Shadow disagree:       93,072
Shadow missing:             0

Agreement rate:          98.4%
```

The disagreements consistently found another resting order at the expected price/side rather than an empty book.

Several of the disagreeing symbols overlap with the symbols exhibiting strong replenishment/iceberg-like behavior in the invariant analysis.

This is consistent with hidden reserve liquidity that cannot be reconstructed from the displayed ITCH order stream alone.

### What this validates

The reconstructed order queues reproduce the observed exchange execution order **98.4% of the time** under the visible order data.

### What this does not prove

It does not prove that the reconstructed engine exactly reproduces NASDAQ's internal matching implementation.

The exchange's complete internal state — particularly hidden/reserve liquidity — is not observable from the displayed ITCH feed.

---

## Performance

Performance is reported in separate stages because each benchmark measures a different workload.

### Raw ITCH parsing

Using `mmap` for the binary ITCH file and zero-copy message decoding:

**~36M messages/sec**

This benchmark measures the parsing/data-ingestion stage without full order-book maintenance or validation instrumentation.

The large throughput comes from avoiding traditional per-message file I/O and copying: the entire file is memory-mapped and message fields are decoded directly from the mapped buffer.

### Parsing + book maintenance

With the reconstructed order book being updated for every relevant message:

**~1.5–1.6M messages/sec**

This includes order adds, executions, cancels, deletes, and replaces across all symbols.

### Full validation instrumentation

With the additional validation work enabled:

* shadow matching
* per-symbol shadow engines
* execution comparisons
* iceberg/reserve replenishment chain tracking
* disagreement logging
* additional validation counters

**~610K messages/sec**

This number is intentionally reported separately because the validation machinery is substantially more expensive than the production-style parsing/book-maintenance path.

The performance progression therefore looks like:

```text
Raw mmap parsing                    ~36M msgs/sec
Parsing + book maintenance       ~1.5–1.6M msgs/sec
Full validation instrumentation    ~610K msgs/sec
```

These numbers are not directly interchangeable: each represents a progressively heavier workload.

---

## Matching engine benchmark

The matching engine was also benchmarked independently from the ITCH parser.

Using 1,000,000 synthetic random orders, single-threaded, compiled with `-O3`:

| Metric        |            Result |
| ------------- | ----------------: |
| Throughput    | ~4.97M orders/sec |
| P50 latency   |             83 ns |
| P99 latency   |            570 ns |
| P99.9 latency |          2,131 ns |

This benchmark measures the matching engine alone.

It is intentionally reported separately from the parser throughput because an "order/sec" matching-engine benchmark and a "message/sec" ITCH replay benchmark measure different workloads.

The latency tail is expected to be larger than the median because some operations require multi-level sweeps, additional data-structure traversal, or encounter cache misses.

### Test environment

```text
CPU:        AMD Ryzen 7 6800H
Compiler:   C++20
Optimization: -O3
Warnings:   -Wall -Wextra -Wpedantic
RNG seed:   42
```

---

## Testing

The standalone matching engine has:

* 12 unit tests
* 8 behavioral categories
* partial fills
* multi-level sweeps
* price-time priority
* cancel behavior
* replace behavior
* cancel/replace races
* invariant sanity checks

Run:

```bash
./build/test_matching_engine
```

Run the standalone benchmark:

```bash
./build/bench_matching_engine
```

Run the full ITCH replay:

```bash
./build/parser
```

---

## What I'd add next

* **Parallelize by symbol** — symbols are logically independent, making them a natural parallelization boundary.
* **Automate iceberg detection** — identify symbols with strong replenishment signatures rather than relying on manual overlap analysis.
* **Multi-day validation** — determine whether the 98.4% shadow agreement rate remains stable across different trading days and market conditions.
* **Expand test coverage** — add additional matching-engine edge cases around complex replace and partial-fill sequences.

---

## Known limitations

* **Hidden/iceberg liquidity is not explicitly reconstructed.** ITCH does not expose hidden reserve size or an iceberg flag. This is the primary suspected source of the remaining shadow-matching disagreement.
* **Off-exchange and non-displayed liquidity is not represented.**
* **Single-threaded replay.** Symbols are logically independent, but the current replay is intentionally sequential.
* **Validation is currently based on one full trading day.** Multi-day validation is a planned extension.
* **Exchange-internal behavior is not fully observable.** The implementation reconstructs the displayed order book and validates it against observable executions, rather than claiming access to NASDAQ's proprietary internal matching state.

---

## Build & run

Build:

```bash
cmake --build build
```

Run the parser:

```bash
./build/parser
```

Run unit tests:

```bash
./build/test_matching_engine
```

Run the benchmark:

```bash
./build/bench_matching_engine
```

The parser currently expects the ITCH file at:

```text
/home/mathdee/itch_data/12302019.NASDAQ_ITCH50
```

Update the path in `parser.cpp` to point to your own NASDAQ ITCH 5.0 data file.

---

## Data

The validation run used a full trading day of NASDAQ TotalView-ITCH 5.0 data:

```text
Messages:  ~269 million
Symbols:   ~8,906
Book ops:  ~263 million
```

The parser processes the binary feed sequentially and maintains an independent order book for each symbol locate.

---

## Background reading

* [How to Build a Fast Limit Order Book](https://web.archive.org/web/20110219163448/http://howtohft.wordpress.com/2011/02/15/how-to-build-a-fast-limit-order-book/) — informed the add/cancel/execute-first design and data-structure layout.
* [NASDAQ TotalView-ITCH 5.0 Specification](https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/NQTVITCHSpecification.pdf) — message formats and feed semantics.
* [Databento ITCH Microstructure Primer](https://databento.com/microstructure/itch) — additional background on ITCH and market microstructure.

