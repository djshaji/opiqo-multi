# Audio callback issues

This document collects the problems found while analysing `FullDuplexPass.h`'s audio callback and block-adapter logic, together with short explanations and suggested fixes.

## Summary

- The callback adapts variable-sized Oboe bursts into fixed-size blocks, runs blocks through an LV2 plugin chain, applies gain and queues processed samples for output. The important issues below affect correctness, performance and thread-safety.

## Problems

### Problem 1 — Bypass logic is inverted / disables processing

Observed code:

```cpp
void processPluginChain(float *buffer, int32_t numSamples) {
	if (!(bypass && !*bypass)) {
		return;
	}
	// ...process plugins...
}
```

Why it's wrong
- The condition returns early when `bypass` is null, which unintentionally disables processing if no bypass pointer is provided. The intended behaviour is usually: if `bypass` exists and is true, skip processing; otherwise process.

Suggested fix

```cpp
// Skip processing only when an external bypass flag exists and is true.
if (bypass && *bypass) return;
```

### Problem 2 — Unused / inflexible plugin pointers

Observed: `LV2Plugin* plugin` plus `plugin1..plugin4` are declared and only a subset are used. This is brittle and not extensible.

Recommendation
- Replace these fixed pointers with a single `std::vector<LV2Plugin*> plugins;` and iterate the vector in `processPluginChain`. This simplifies runtime management and scales to an arbitrary chain length.

### Problem 3 — `pluginMutex` ownership and pointer usage

Observed: `std::mutex* pluginMutex` is a raw pointer. Ownership is unclear and risks dangling pointers.

Recommendation
- Prefer embedding a mutex (`std::mutex pluginMutex;`) or use a smart pointer (`std::shared_ptr<std::mutex>`) and document ownership. If the mutex is external, require callers to pass a valid pointer and document the lifetime guarantees.

### Problem 4 — Input/output channel-count assumptions

Observed: `mSamplesPerFrame` is taken from the output stream channel count and used for both input and output sample calculations.

Why this is risky
- If input and output streams have different channel counts the calculation `numInputSamples = numInputFrames * mSamplesPerFrame` will be incorrect.

Recommendation
- Ensure both streams use the same channel count or explicitly query the input stream channel count for input calculations. Add an assert or log to catch mismatches at startup.

### Problem 5 — Samples vs frames passed to plugin

Observed: code passes `mFixedBlockSamples` (samples = frames * channels) to `LV2Plugin::process`. Many plugin APIs expect frame counts (number of frames), not sample counts.

Action
- Verify `LV2Plugin::process` expects frames or samples. If it expects frames, pass `mFixedBlockFrames` instead of `mFixedBlockSamples`.

### Problem 6 — `mProcessedQueue` uses vector front-erases (O(n))

Observed: `mProcessedQueue` is a `std::vector<float>` used as a queue; code uses `insert` at end and `erase(begin(), begin()+n)` to drop consumed samples.

Why it's bad
- Erasing from the front of a vector is O(n) and may cause memory movement and allocations in the audio thread.

Recommendation
- Use `std::deque<float>` or a lock-free ring buffer / circular buffer for the processed queue to avoid costly copies on erase. A single circular buffer with head/tail indices is ideal for realtime audio.

### Problem 7 — Unsafe erase count when compacting queue

Observed: when compacting, code assumes `mProcessedReadIndex` is within `mProcessedQueue.size()`; if logic diverges this could be out-of-bounds.

Fix

```cpp
int eraseCount = std::min(mProcessedReadIndex, static_cast<int32_t>(mProcessedQueue.size()));
if (eraseCount > 0) {
	mProcessedQueue.erase(mProcessedQueue.begin(), mProcessedQueue.begin() + eraseCount);
}
mProcessedReadIndex = 0;
```

### Problem 8 — Thread-safety of control/state variables

Observed: `bypass`, `gain`, plugin pointers and `mRequestedBlockFrames` can be modified by other threads (e.g. UI) while audio thread reads them. Only plugin processing is protected by `pluginMutex` (and the mutex pointer may be null).

Recommendation
- Make frequently-updated control variables atomic (`std::atomic<bool>` for bypass, `std::atomic<float>` or guarded writes for gain) or ensure all control changes happen under a mutex that the audio thread also locks. Document required thread-safety for any pointer members.

### Problem 9 — `queueManager->process` parameter ordering/semantics

Observed call:

```cpp
queueManager->process(const_cast<float *>(inputFloats), outputStart, numOutputSamples);
```

Recommendation
- Confirm `LockFreeQueueManager::process` expects `(input, output, samples)`. If the order or meaning differs, adjust the call.

### Problem 10 — Callback always returns Continue

Observed: callback returns `oboe::DataCallbackResult::Continue` in all cases.

Note
- This is normal for steady-state, but consider returning `Stop` on fatal errors or if you need to request a restart. Not a bug but something to consider for robustness.

## Tests and validation

- Bypass behaviour: test with `bypass` null, true and false to ensure processing enabled/disabled as expected.
- Channel mismatch: create streams with differing channel counts to verify behaviour or assert early.
- Plugin frame/sample contract: add unit tests or small harness to confirm `LV2Plugin::process` expects frames vs samples.
- Queue stress test: run with small bursts and a long chain to observe allocations and CPU spikes; replace vector with deque/circular buffer if necessary.

## Next steps

- Apply the bypass fix and safe erase guard (small, safe changes) first.
- If you want, I can prepare a minimal patch for those two changes and run a local build/test.


## Action items for you (suggested)
- Fix the bypass condition immediately as shown above — it's a logic bug that will commonly disable processing if bypass is not set.
- Confirm whether LV2Plugin::process() expects samples or frames and adjust the argument accordingly.
- Consider refactoring plugin pointers to a std::vector<LV2Plugin*> and use deque or ring buffer for mProcessedQueue if you observe CPU spikes or allocations under stress.
- Make bypass and gain atomic or ensure modifications happen under a mutex to prevent races.
- Add logging/asserts to detect mismatched channel counts early.

## Also
- Prepare a minimal patch implementing the bypass fix and safe erase guard (two-line changes), or
- Refactor plugin storage and processed queue to more robust structures (larger change),
- Add assert checks for input/output channel count mismatch and a runtime configuration check.

## Changes applied in the codebase

The following edits were implemented to address the issues described above. These were kept minimal where possible and focused on realtime-safety, correctness and diagnostics.

- Added a header-only SPSC ring buffer to avoid O(n) front-erases and allocations in the audio callback:
  - `app/src/main/cpp/SpscRingBuffer.h`
	- Pre-allocated power-of-two buffer, lock-free for one producer + one consumer.
	- Methods: `init()`, `push()`, `read()`, `drop()`, `reset()`.

- Replaced the `std::vector<float>` processed queue with the SPSC ring buffer and added a dropped-block diagnostic counter:
  - `app/src/main/cpp/FullDuplexPass.h`
	- Replaced front-erase queue usage with `SpscRingBuffer<float> mProcessedQueue`.
	- Added `std::atomic<uint32_t> droppedProcessedBlocks{0}` and a public getter `getDroppedProcessedBlocks()`.
	- When pushing a processed block, code now checks the `push()` return value and increments the counter when the ring is full (policy: drop-newest).
	- Fixed bypass check logic (skip processing only when bypass exists and is true) and iterates `std::vector<LV2Plugin*> plugins` for processing.

- Made control variables safe for lock-free reads from the audio thread:
  - `app/src/main/cpp/LiveEffectEngine.h` / `LiveEffectEngine.cpp`
	- `gain` and `bypass` are `std::atomic<float>` and `std::atomic<bool>` respectively (engine header).
	- The duplex stream receives pointers to these atomics (`mDuplexStream->gain = &gain;`) so the audio callback can read them without locking.
  - `app/src/main/cpp/jni_bridge.cpp`
	- Fixed JNI `setGain` to store into the atomic with `engine->gain.store(...)` instead of dereferencing a pointer.

- Exposed the dropped-block diagnostic to Java and added a native test harness for the SPSC buffer:
  - JNI: added `Java_org_acoustixaudio_opiqo_multi_AudioEngine_testSpscRingBuffer(...)` which runs a producer/consumer test, validates ordering and returns a short report string.
  - Java binding: `AudioEngine.testSpscRingBuffer(int iterations, int capacity)` added.
  - UI: `MainActivity` test menu now includes `SPSC Ring Buffer Test` and runs the native test asynchronously, showing the result in an AlertDialog.

Build and verification

- After these changes I ran a full `assembleDebug` build (native + Java) to ensure the edits compile. The build completed successfully.

How to run the native SPSC test from the app

- In the app Test menu choose `SPSC Ring Buffer Test` (added to the existing test dialog). The test runs off the UI thread with default parameters (1_000_000 iterations, capacity 1024) and shows the result when finished.
- Alternatively call `AudioEngine.testSpscRingBuffer(iterations, capacity)` from Java to run the native test programmatically.

Notes and further work

- The SPSC test uses a consumer-side vector sized to `iterations` for validation; choose iteration counts appropriate for device memory.
- If you prefer a different overflow policy (drop-oldest) or want to expose/reset diagnostics counters via JNI/UI, I can implement that next.

## Other changes made in this session (not previously listed)

The session included additional refactors and fixes that touch plugin management, JNI glue and engine startup. These are listed here with the affected files so you have a single place documenting all applied edits.

- Plugin-slot refactor (replace fixed plugin1..plugin4 pointers with a vector):
  - Files changed:
	- `app/src/main/cpp/LiveEffectEngine.h` — added `std::vector<LV2Plugin*> plugins;` and initialized it with 4 slots in the constructor.
	- `app/src/main/cpp/LiveEffectEngine.cpp` — initializes `plugins.assign(4, nullptr);` and copies `plugins` into the duplex stream under lock when creating streams.
	- `app/src/main/cpp/FullDuplexPass.h` — replaced per-slot members by `std::vector<LV2Plugin*> plugins;` and updated processing code to iterate the vector.
	- `app/src/main/cpp/jni_bridge.cpp` — replaced many switch/case branches with indexed, bounds-checked accesses `engine->plugins[idx]` across functions such as `setValue`, `addPlugin`, `deletePlugin`, `setPluginEnabled`, `getPreset`, `getPresetList`, `setFilePath`, `getWritables`, and `initPlugins`.

- Renamed Lilv plugin collection to avoid name clash:
  - `app/src/main/cpp/LiveEffectEngine.h` — the Lilv plugin collection is `const LilvPlugins * lv2Plugins` (renamed from a conflicting symbol).
  - `app/src/main/cpp/jni_bridge.cpp` — uses `engine->lv2Plugins` and `LILV_FOREACH` accordingly when enumerating available plugins.

- Plugin mutex and publishing snapshot to the audio thread:
  - When streams are opened the engine now copies the `plugins` vector under `pluginMutex` and publishes the pointer to the duplex stream:
	- `mDuplexStream->plugins = plugins;` (under lock)
	- `mDuplexStream->pluginMutex = &pluginMutex;` so the duplex stream may use the engine's mutex for plugin lifecycle safety.

- Atomic control variables and audio-thread usage:
  - `LiveEffectEngine.h` now holds `std::atomic<float> gain` and `std::atomic<bool> bypass` (previously raw pointers/float*), and `LiveEffectEngine.cpp` publishes their addresses to the duplex stream (`mDuplexStream->gain = &gain; mDuplexStream->bypass = &bypass;`).
  - `FullDuplexPass.h` reads these via atomic loads (lock-free) inside the audio callback.

- JNI and glue fixes beyond `setGain`:
  - Fixed incorrect dereferences and accesses (for example `*engine->gain = ...` corrected to `engine->gain.store(...)`).
  - Added a safe JNI getter `Java_org_acoustixaudio_opiqo_multi_AudioEngine_getDroppedProcessedBlocks` that returns the diagnostic dropped-block counter from the duplex stream.

- Small code hygiene and build fixes during the refactor:
  - Renamed or adjusted variables and types where necessary to resolve compile errors that surfaced during the refactor (for example avoiding duplicate symbol names and correcting pointer vs atomic assignments).

If you want, I can expand any of these bullet points with before/after code snippets and the exact diff hunks for easier review. I can also run targeted unit checks or add a small UI widget to expose `getDroppedProcessedBlocks()` live in the app. Which would you like next?


