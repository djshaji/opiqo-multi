# Latency analysis and remediation checklist

This document collects the likely causes of high audio latency observed in the app and gives prioritized, concrete fixes and measurement steps. The analysis is based on the current implementation in:

- `app/src/main/cpp/FullDuplexPass.h`
- `app/src/main/cpp/LiveEffectEngine.cpp` / `LiveEffectEngine.h`
- `app/src/main/cpp/jni_bridge.cpp`

Summary of top latency contributors
1. Large processing block size (default `blockSize = 4096`) — each block adds that many frames of buffering before output.
2. Block-adapter accumulation: callbacks accumulate input until a full block is ready, adding up to one block of delay.
3. Extra ring-buffering stage: processed blocks are pushed into `mProcessedQueue` then read later — an extra buffering hop.
4. Heavy work inside the realtime callback (eg. `queueManager->process`) or plugin process time — both increase scheduling jitter and effective latency.
5. Mutex usage on the audio path (`pluginMutex`) — if UI or plugin operations hold the mutex for long, the audio thread may stall.
6. Large ring buffer capacity (e.g. 65536 floats) — increases maximum queued latency when the buffer holds many blocks.

Where latency is introduced (specific code locations)
- `LiveEffectEngine.h` / `LiveEffectEngine.cpp`
  - `int blockSize = 4096;` — default requested plugin block size.
  - `mDuplexStream->setPluginBlockFrames(blockSize);` — publishes large block to audio path.

- `FullDuplexPass.h`
  - `initializeBlockAdapter()` sets `mFixedBlockFrames` to `mRequestedBlockFrames` or `getFramesPerBurst()`; `mFixedBlockSamples = mFixedBlockFrames * channels`.
  - `onBothStreamsReady()` accumulates input into `mInputBlock` until `mFixedBlockSamples` are available, calls `processPluginChain()`, then pushes processed block into `mProcessedQueue`.
  - `mProcessedQueue` capacity default chosen (`std::max(mFixedBlockSamples*8, 65536)`) — can be very large.
  - `queueManager->process(...)` is called inside the audio callback (may do encoder/IO work).

- `jni_bridge.cpp`
  - JNI and plugin management changes can affect how quickly plugins are swapped or initialized; long init operations while holding `pluginMutex` can cause audio thread stalls.

Concrete prioritized fixes (apply in this order)

High-impact, low-risk (do these first):

1) Use the stream's frames-per-burst (small) instead of a large hard-coded `blockSize` by default.
   - Change `LiveEffectEngine::blockSize` default to `0` so `openStreams()` will use `getFramesPerBurst()`.
   - In `initializeBlockAdapter()` prefer `getOutputStream()->getFramesPerBurst()` and clamp `mFixedBlockFrames` so it does not exceed the burst size.
   - Rationale: reduces worst-case accumulation from e.g. 4096 frames (~85 ms at 48 kHz) to a single burst (typically 32–256 frames → 0.7–5 ms).

2) Avoid the extra ring-buffer hop for in-callback synchronous processing.
   - If plugin processing is performed synchronously in the audio callback, write processed samples directly to `outputFloats` instead of `push()` then `read()` from `mProcessedQueue` within the same callback.
   - Keep the ring only for decoupling to recorder/analysis threads; do not use it as the primary path between processing and output.
   - Rationale: eliminates one buffering stage and reduces end-to-end latency.

3) Snapshot plugin pointers before processing instead of holding `pluginMutex` during processing.
   - Under `pluginMutex` copy `plugins` into a small local vector, unlock, then iterate and call `process()` on the local vector.
   - Rationale: prevents audio thread from being blocked by long-running UI/plugin operations.

Medium-impact changes:

4) Move `queueManager->process(...)` out of the realtime callback.
   - Replace it with a lock-free push to a recorder queue; let a background thread perform encoding/IO.

5) Reduce ring buffer capacity to only the amount of safety headroom you need (e.g., 2–8 blocks) instead of the current large default.

Large/complex (if needed):

6) Audit LV2 plugin `process()` functions for real-time safety. Plugins that allocate memory, perform syscalls, or access the filesystem in process() must be fixed or run on worker threads.
   - If a plugin cannot be made RT-safe, move expensive ops to worker threads or do non-RT work via the LV2 Worker/Worker-queue mechanisms.

Measurement & validation

- Micro-benchmarks to add temporarily to the audio path (log very infrequently to avoid overhead):
  - Measure elapsed time spent in `processPluginChain()` (per callback) and track max/mean.
  - Measure time spent in `queueManager->process()` (if still called) and in ring buffer operations.
  - Keep these measurements in atomics and report via JNI/Log every N seconds.

- Round-trip latency test (recommended):
  - Loopback a short impulse (or use a test signal) from input to output and measure the delay with an oscilloscope or by recording both input and output and cross-correlating offline.
  - Expected: after applying the high-impact fixes, latency should approximate 1–3 bursts + plugin processing time (a few ms), not tens of ms.

Quick commands / build notes

To rebuild after changes and verify no compile regressions:

```bash
cd /home/djshaji/AndroidStudioProjects/opiqoGuitarMultiEffectsProcessor
./gradlew assembleDebug --no-daemon -x lint
```

Next steps I can apply for you (pick one or more)
 - Apply the minimal patch now: set `blockSize = 0` in `LiveEffectEngine.h`, clamp `mFixedBlockFrames` to `getFramesPerBurst()` in `FullDuplexPass::initializeBlockAdapter()`, snapshot plugin list before processing. I will then run a build.
 - Also remove the intra-callback ring-buffer hop (write output directly) and move recorder queue pushes off the audio thread.
 - Add lightweight timing instrumentation to `FullDuplexPass` to measure per-callback plugin processing time.

If you want me to make and test the minimal patch now, say "apply minimal latency patch" and I will implement the changes and run a build.

```

