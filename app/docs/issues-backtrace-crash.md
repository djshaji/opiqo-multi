# Crash Analysis: SIGSEGV in AVX2 FFT (gx_amp / zita-convolver)

## Crash Location

Frame #0 — FFTW AVX2-128 butterfly `t1buv_8`, invoked through zita-convolver's
partition-convolution inside `gx_amp.lv2`.

```
#0  t1buv_8 (rio=0x794bc4, ...)   ← misaligned FFTW internal pointer
#3  Convlevel::process             zita-convolver.cc:803
#4  Convlevel::readout             zita-convolver.cc:840
#5  Convproc::process(sync=false)  zita-convolver.cc:321
#7  __start_rt_text                gx_amp.so (plugin run)
#11 LV2Plugin::process             LV2Plugin.hpp:595
#12 main                           test.cpp:25
```

---

## Root Cause: Block-size mismatch (CRITICAL)

`test.cpp` constructs the plugin with a block size of **4096**:

```cpp
// test.cpp:14
LV2Plugin plugin(world, uri, 48000., 4096);
```

`LV2Plugin` propagates this as `buf_minBlock`, `buf_maxBlock`, and `buf_nominalBlock`
to the plugin via the LV2 options array. `gx_amp` uses this value to configure
`Convproc` — partition sizes, FFT lengths, and internal read/write head offsets
are all derived from the configured block length.

However, `process()` is called with only **512** frames:

```cpp
// test.cpp:25
plugin.process(input, output, 512);
```

Feeding a shorter block shifts `Convlevel`'s internal read/write heads out of phase
with the FFT partition boundaries. `Convlevel::readout` consequently passes a
mis-offset pointer (`rio = 0x794bc4`, 4-byte aligned instead of 32-byte aligned)
into the FFTW plan, which issues an AVX2 aligned-load → **SIGSEGV**.

### Fix

Make the process call size match the configured block size:

```cpp
// Option A — change constructed block size to match call site
LV2Plugin plugin(world, uri, 48000., 512);

// Option B — change call site to match configured block size (resize arrays too)
alignas(32) float input[4096]  = {};
alignas(32) float output[4096] = {};
plugin.process(input, output, 4096);
```

---

## Bug 2: Dead channel counters in audio port connection loop (`LV2Plugin.hpp:533`)

```cpp
uint32_t input_index = 0, output_index = 0;   // declared, never used
for (auto& p : ports_) {
    if (!p.is_audio) continue;
    float* target = inputBuffer;               // always points to channel 0
    if (!p.is_input) target = outputBuffer;    // always points to channel 0
    lilv_instance_connect_port(instance_, p.index, target);
}
```

Every input audio port is connected to the same `inputBuffer` base pointer; every
output audio port to the same `outputBuffer` base pointer. For mono `gx_amp` this
is harmless, but any stereo or multi-channel plugin will have all channels aliasing
the same memory, causing corrupt reads and writes.

`input_index` / `output_index` were clearly intended to stride through an
interleaved or planar buffer but were never wired up.

### Fix

```cpp
uint32_t input_index = 0, output_index = 0;
for (auto& p : ports_) {
    if (!p.is_audio) continue;
    if (p.is_input) {
        lilv_instance_connect_port(instance_, p.index, inputBuffer  + input_index  * numFrames);
        ++input_index;
    } else {
        lilv_instance_connect_port(instance_, p.index, outputBuffer + output_index * numFrames);
        ++output_index;
    }
}
```

*(Assumes planar layout — adjust stride if interleaved.)*

---

## Bug 3: Stack audio buffers not aligned for AVX2 (`test.cpp:20-21`)

```cpp
float input[512]  = {0};   // stack — only 16-byte alignment guaranteed
float output[512] = {0};
```

Connecting unaligned buffers to an AVX2 plugin is undefined behaviour. Use
`alignas(32)`:

```cpp
alignas(32) float input[512]  = {};
alignas(32) float output[512] = {};
```

---

## Summary Table

| # | Severity | File | Issue |
|---|----------|------|-------|
| 1 | **Critical** | `test.cpp:14,25` | Block size passed to constructor (4096) does not match `process()` call (512) → misaligned FFTW internal pointer → SIGSEGV |
| 2 | High | `LV2Plugin.hpp:533` | `input_index`/`output_index` unused; all audio channels connect to the same buffer pointer |
| 3 | Low | `test.cpp:20-21` | Stack audio buffers lack `alignas(32)` required for AVX2 plugins |
