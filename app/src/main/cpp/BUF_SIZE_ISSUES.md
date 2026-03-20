# LV2 buf-size Extension Issues in LV2Plugin.hpp

## Issue 1 — Spec Violation: `boundedBlockLength` requires `minBlockLength` in options (line 1099)

The `lv2:boundedBlockLength` feature **requires** both `bufsz:minBlockLength` and `bufsz:maxBlockLength`
to be present in the `options:options` array. Currently only `maxBlockLength` is provided:

```cpp
LV2_Options_Option options[] = {
    { LV2_OPTIONS_INSTANCE, 0, urids_.buf_maxBlock, sizeof(uint32_t), urids_.atom_Int, &max_block_length_ },
    // ← minBlockLength is missing!
    { LV2_OPTIONS_INSTANCE, 0, 0, 0, 0, nullptr }
};
```

A conforming plugin that declares `lv2:requiredFeature bufsz:boundedBlockLength` will fail or
misbehave because the host violates the contract.

**Fix:** Add `minBlockLength` (and nominally `nominalBlockLength`, `sequenceSize`) to the options array.

---

## Issue 2 — Missing URIDs (lines 833, 857)

Only `buf_maxBlock` (`LV2_BUF_SIZE__maxBlockLength`) is mapped. The following URIDs are never
registered in `urids_` or `init_urids()`:

| Missing URID field | LV2 Constant | Notes |
|--------------------|--------------|-------|
| `buf_minBlock`     | `LV2_BUF_SIZE__minBlockLength`     | **Required** by `boundedBlockLength` spec |
| `buf_nominalBlock` | `LV2_BUF_SIZE__nominalBlockLength` | Optional but widely expected |
| `buf_seqSize`      | `LV2_BUF_SIZE__sequenceSize`       | Needed for MIDI sequence buffer sizing |

**Fix:** Add to `urids_` struct and map in `init_urids()`:
```cpp
urids_.buf_minBlock     = map_uri(LV2_BUF_SIZE__minBlockLength);
urids_.buf_nominalBlock = map_uri(LV2_BUF_SIZE__nominalBlockLength);
urids_.buf_seqSize      = map_uri(LV2_BUF_SIZE__sequenceSize);
```

---

## Issue 3 — `fixedBlockLength` not advertised (line 930)

On Android with Oboe the block size is typically fixed per audio session. The spec provides
`LV2_BUF_SIZE__fixedBlockLength` which implies `boundedBlockLength` and allows plugins to
optimize by skipping dynamic-size checks. This feature is entirely absent.

```cpp
// Current:
features_.bbl_feature.URI = LV2_BUF_SIZE__boundedBlockLength;

// Better for fixed-size Oboe callbacks:
features_.bbl_feature.URI = LV2_BUF_SIZE__fixedBlockLength; // implies bounded
```

---

## Issue 4 — `sequenceSize` not communicated via options

Plugins that process MIDI use the `bufsz:sequenceSize` option to know the maximum byte size of an
`atom:Sequence` buffer at each port. Without it, plugins fall back to internal defaults or may
malfunction. The internal `Port::atom_buf_size` default of 8192 (and `required_atom_size_`) are
used for allocations but never exposed to the plugin through options.

**Fix:** Add a `sequenceSize` option entry pointing to `required_atom_size_`:
```cpp
{ LV2_OPTIONS_INSTANCE, 0, urids_.buf_seqSize, sizeof(uint32_t), urids_.atom_Int, &required_atom_size_ },
```

---

## Recommended Complete Fix for `init_instance()` options array

```cpp
// Add to urids_ struct declaration:
LV2_URID buf_minBlock;
LV2_URID buf_nominalBlock;
LV2_URID buf_seqSize;

// Add to init_urids():
urids_.buf_minBlock     = map_uri(LV2_BUF_SIZE__minBlockLength);
urids_.buf_nominalBlock = map_uri(LV2_BUF_SIZE__nominalBlockLength);
urids_.buf_seqSize      = map_uri(LV2_BUF_SIZE__sequenceSize);

// Replace options[] in init_instance():
LV2_Options_Option options[] = {
    { LV2_OPTIONS_INSTANCE, 0, urids_.buf_minBlock,     sizeof(uint32_t), urids_.atom_Int, &max_block_length_ },
    { LV2_OPTIONS_INSTANCE, 0, urids_.buf_maxBlock,     sizeof(uint32_t), urids_.atom_Int, &max_block_length_ },
    { LV2_OPTIONS_INSTANCE, 0, urids_.buf_nominalBlock, sizeof(uint32_t), urids_.atom_Int, &max_block_length_ },
    { LV2_OPTIONS_INSTANCE, 0, urids_.buf_seqSize,      sizeof(uint32_t), urids_.atom_Int, &required_atom_size_ },
    { LV2_OPTIONS_INSTANCE, 0, 0, 0, 0, nullptr }
};

// And in init_features() (if block size is fixed):
features_.bbl_feature.URI = LV2_BUF_SIZE__fixedBlockLength;
```
