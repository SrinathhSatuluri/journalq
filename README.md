# journalq

A durable message log for a single machine, written in C++20 with no
dependencies beyond the standard library.

Producers append to a named queue. Each record is persisted before it is
acknowledged, and consumers read from an offset they own. Two files back
every queue: a journal of fixed-size records that indexes a data file of
variable-size records. Every record carries its sizes in words, a padding
byte, and a CRC32C, so a torn or corrupt tail is found by header checks during
recovery, never by a crash later.

The design follows the patterns of Bloomberg's BlazingMQ, which I contribute
to. This project is where I learn those patterns by building them from
scratch.

## Design

- Journal and data file. The journal is the source of truth and is always
  written last; the data file holds payloads.
- Validate before trust. Recovery walks the journal, checks every header
  against the file it points into, and truncates at the first record that
  fails. A record is registered only after it passes.
- Return codes, not exceptions. Every failure path returns a code the caller
  can act on, and every check that rejects input says why in the log.
- One writer per queue, many readers. Writes are sequenced on a single thread
  and reads never block them.

## Guarantees

Targets. Each becomes a claim only once a test backs it.

- A record acknowledged to a producer survives a process crash.
- Recovery never serves a corrupt record.
- Recovery time grows with the journal, not with the data file.

## Roadmap

1. Record layouts and checksums
2. Journal writer and reader
3. Recovery pass, with crash injection tests
4. Consumer offsets
5. Network server and binary protocol
6. Fuzz harness for every parser
7. Benchmarks and profiles

## Build

Linux, GCC 12 or Clang 15 and newer.

```
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Sanitizers: configure with `-DJOURNALQ_SANITIZE=address,undefined` or
`-DJOURNALQ_SANITIZE=thread`. CI runs all three configurations on every push.

## License

Apache 2.0
