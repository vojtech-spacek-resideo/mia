# MIA Dev Pipeline Backlog

> Last refreshed: 2026-05-13 — Reconsidered approach, reverted dead code

## URGENT

- [x] ~~**[FIX]** `sensors_i2c.py` hard-fails on `ImportError` for smbus2 instead of degrading to simulation~~ — DONE 2026-05-11
- [x] ~~**[FIX]** `serial_bridge.py` has no simulation/fallback mode~~ — DONE 2026-05-12

## HIGH

- [x] ~~**[TEST]** `arduino.py` needs simulation fallback for CI~~ — DONE 2026-05-12
- [x] ~~**[TEST]** `gpio.py` (messaging client variant) needs graceful degradation when hardware server is unavailable~~ — DONE 2026-05-12
- [x] ~~**[BUILD]** Android MCP bridge TODOs: DPF status, DPF regen, AdBlue, full diagnostics, DTC reading~~ — DONE 2026-05-12 (commands aligned to backend intents)
- [ ] **[BUILD]** C++ audio: native whisper/piper library integration — source: TODO scan (requires actual library linking, not stubs)
- [ ] **[BUILD]** C++ voice FSM `on_command_execute` callback — source: TODO scan (blocked: needs context struct change + consumer)

## MEDIUM

- [ ] **[TEST]** Android instrumented test `DrivingServiceInstrumentedTest.kt` needs IdlingResource + assertions — source: TODO scan
- [x] ~~**[REFACTOR]** `hardware_manager.py` — add explicit fallback/simulation logging when sub-components fail to initialize~~ — DONE 2026-05-12
- [x] ~~**[DOC]** Schema generator `generate.py` lacks `--dry-run` flag for drift detection~~ — DONE 2026-05-12

## LOW

- [ ] **[DOC]** `__init__.py` in hardware/ — add simulation/fallback documentation reference — source: simulation gap scan

## HOMEWORK

- [ ] **[DECIDE]** Provider routing for AI audio: Whisper native vs API, Piper vs ElevenLabs — options: local-only / hybrid / cloud-first, recommendation: hybrid
- [ ] **[DECIDE]** Schema evolution strategy for vehicle_telemetry.fbs — additive only vs versioned — recommendation: additive with deprecation markers

---

## Completed

_(none yet)_
