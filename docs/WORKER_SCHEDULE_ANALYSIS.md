# LV2_Worker_Schedule Race Condition Analysis

## Problem Summary
GxPluginMono causes random crashes when changing MODEL, T_MODEL, and C_MODEL ports. These port changes trigger worker scheduling, which interacts with LV2_Worker_Schedule in a way that creates critical race conditions.

---

## Issue #1: Worker Schedule Feature Before Ringbuffer Initialization (CRITICAL)

### Location
[LV2Plugin.hpp:439-452](LV2Plugin.hpp#L439-L452) - Initialization sequence

### Root Cause
The LV2_Worker_Schedule feature is provided to the plugin **before** ringbuffers are allocated:

**Sequence:**
1. **Line 441**: `init_features()` sets up `host_worker_.schedule` structure:
   - `host_worker_.schedule.schedule_work = host_schedule_work` → function pointer
   - `host_worker_.schedule.handle = &host_worker_` → structure pointer
   - At this point: `host_worker_.requests = nullptr` and `host_worker_.responses = nullptr`

2. **Line 1086**: `lilv_plugin_instantiate()` is called with the feature
   - Plugin receives feature with valid function pointer but null ringbuffers

3. **Lines 1101-1102**: Ringbuffers allocated (after instantiation)
   - Too late if plugin called `schedule_work()` during instantiation

### Impact
- If plugin calls `schedule->schedule_work()` during instantiation, it will fail silently (returns error)
- Plugin may be left in inconsistent state expecting successful scheduling
- Port changes later may trigger crashes when scheduling work

### Evidence
[LV2Plugin.hpp:1088](LV2Plugin.hpp#L1088):
```cpp
LV2_Feature* feats[] = { &features_.um_f, &features_.unm_f, &opt_f,
            &features_.bbl_feature, &features_.map_path_feature,
            &features_.make_path_feature, &features_.free_path_feature,
            &host_worker_.feature, nullptr };  // <-- Feature passed HERE
            
instance_ = lilv_plugin_instantiate(plugin_, sample_rate_, feats);  // Line 1086
// ... 10+ lines of code ...
if (iface) {
    host_worker_.dsp_handle = lilv_instance_get_handle(instance_);
    host_worker_.requests = lv2_ringbuffer_create(8192);  // Line 1101 - Created AFTER
    host_worker_.responses = lv2_ringbuffer_create(8192); // Line 1102
```

---

## Issue #2: Use-After-Free Race in host_schedule_work (CRITICAL)

### Location
[LV2Plugin.hpp:1160-1175](LV2Plugin.hpp#L1160-L1175) - host_schedule_work callback
[LV2Plugin.hpp:1249-1289](LV2Plugin.hpp#L1249-L1289) - stop_worker function

### Root Cause
No synchronization between audio thread calling `host_schedule_work()` and main thread calling `stop_worker()` that frees ringbuffers.

**Race Scenario:**
```
Thread A (Audio/RT):                    Thread B (Main/Shutdown):
process() [line 504]
  val_changed() → true
  schedule_work() [gxamp.cpp:590]
    host_schedule_work() [line 1160]
      check: w->requests != null     closePlugin() [line 471]
      (still valid)                    stop_worker() [line 475]
                                         enabled = false
                                         wait for rt_readers
                                         lv2_ringbuffer_free(requests)
                                         host_worker_.requests = nullptr
      lv2_ringbuffer_write()          
      ↓                     
      CRASH! Writing to freed memory
```

### Impact
- **Severity**: CRITICAL - Causes random crashes when changing ports while stopping
- Undefined behavior when ringbuffer is freed but still being accessed
- Memory corruption possible
- Plugin may crash mid-process due to timing window

### Evidence
[LV2Plugin.hpp:1160-1175]:
```cpp
static LV2_Worker_Status host_schedule_work(
    LV2_Worker_Schedule_Handle handle, uint32_t size, const void* data) {
    
    auto* w = (LV2HostWorker*)handle;
    if (!w || !w->requests) {
        return LV2_WORKER_ERR_NO_SPACE;
    }
    // No lock here! ↓
    const size_t total = sizeof(uint32_t) + size;
    if (lv2_ringbuffer_write_space(w->requests) < total)  // RACE: could be freed
        return LV2_WORKER_ERR_NO_SPACE;

    lv2_ringbuffer_write(w->requests, (const char*)&size, sizeof(uint32_t));  // CRASH HERE
    lv2_ringbuffer_write(w->requests, (const char*)data, size);
    return LV2_WORKER_SUCCESS;
}
```

[LV2Plugin.hpp:1249-1289]:
```cpp
void stop_worker() {
    host_worker_.enabled.store(false, std::memory_order_release);
    while (host_worker_.rt_readers.load(std::memory_order_acquire) != 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    // At this point, rt_readers == 0, but there's NO GUARANTEE
    // that rt thread has exited host_schedule_work()
    
    if (host_worker_.requests) {
        lv2_ringbuffer_free(host_worker_.requests);  // FREE
        host_worker_.requests = nullptr;  // SET TO NULL - audio thread may be inside function!
    }
    // ...
}
```

---

## Issue #3: Incomplete Enabled Gate for schedule_work (CRITICAL)

### Location
[LV2Plugin.hpp:1160-1175](LV2Plugin.hpp#L1160-L1175) - host_schedule_work has no enabled check
[LV2Plugin.hpp:562-572](LV2Plugin.hpp#L562-L572) - process checks enabled, but too late

### Root Cause
The `enabled` flag gates access in `process()` → `deliver_worker_responses()`, but NOT in the plugin's call to `schedule_work()`.

**Flow:**
1. Plugin calls `schedule->schedule_work()` directly (external call, not through LV2Plugin)
2. Goes directly to `host_schedule_work()` [line 1160]
3. NO enabled check in `host_schedule_work()`
4. Meanwhile, main thread sets `enabled = false` and starts freeing resources

**Evidence - Missing Check:**
```cpp
// In host_schedule_work (line 1160):
static LV2_Worker_Status host_schedule_work(
    LV2_Worker_Schedule_Handle handle, uint32_t size, const void* data) {
    
    auto* w = (LV2HostWorker*)handle;
    // ❌ NO CHECK: if (!w->enabled) { return ... }
    if (!w || !w->requests) {
        return LV2_WORKER_ERR_NO_SPACE;
    }
    // Proceeds even if enabled == false!
    lv2_ringbuffer_write(w->requests, (const char*)&size, sizeof(uint32_t));
    ...
}
```

**Contrast with process() [line 562]:**
```cpp
// ✅ HAS CHECK
if (host_worker_.enabled.load(std::memory_order_acquire)) {
    host_worker_.rt_readers.fetch_add(1, std::memory_order_acq_rel);
    // ...
    host_worker_.rt_readers.fetch_sub(1, std::memory_order_acq_rel);
}
```

The `enabled` flag only gates the `deliver_worker_responses()` call, not the incoming `schedule_work()` call.

### Impact
- Even after shutdown begins, plugin can still call `schedule_work()`
- Ringbuffer may be freed while plugin is attempting to queue work
- No protection for this code path

---

## Issue #4: Missing Memory Barrier Between enabled=false and ringbuffer free

### Location
[LV2Plugin.hpp:1249-1260](LV2Plugin.hpp#L1249-L1260) - Shutdown sequence

### Root Cause
The `enabled` flag is set to false with `memory_order_release`, but this doesn't guarantee that all in-flight calls to `host_schedule_work()` have completed before ringbuffers are freed.

**Problem:**
```cpp
void stop_worker() {
    host_worker_.enabled.store(false, std::memory_order_release);
    // memory_order_release only synchronizes THIS store
    // It does NOT wait for in-flight readers to see the new value
    
    while (host_worker_.rt_readers.load(std::memory_order_acquire) != 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    // rt_readers only tracks deliver_worker_responses() section!
    // Does NOT track host_schedule_work() calls!
    
    // ... ringbuffer is freed ...
}
```

The issue is that `rt_readers` counter only protects the code in `process()` → `deliver_worker_responses()` section. It does NOT protect the plugin's direct call to `schedule_work()`.

### Impact
- Plugin can call `schedule_work()` outside the protected section
- No counting mechanism for these calls
- Ringbuffers can be freed while `schedule_work()` is in progress

---

## Issue #5: Double-Free on Ringbuffer Allocation Failure

### Location
[LV2Plugin.hpp:1101-1122](LV2Plugin.hpp#L1101-L1122) - Worker initialization

### Root Cause
If ringbuffer allocation fails, the cleanup properly frees them, but the `enabled` flag is only set in the success case.

**Code:**
```cpp
if (iface) {
    host_worker_.dsp_handle = lilv_instance_get_handle(instance_);
    host_worker_.requests = lv2_ringbuffer_create(8192);   // May return nullptr
    host_worker_.responses = lv2_ringbuffer_create(8192);  // May return nullptr

    if (!host_worker_.requests || !host_worker_.responses) {
        LOGE("Failed to create worker ringbuffers...");
        if (host_worker_.requests) {
            lv2_ringbuffer_free(host_worker_.requests);
            host_worker_.requests = nullptr;
        }
        if (host_worker_.responses) {
            lv2_ringbuffer_free(host_worker_.responses);
            host_worker_.responses = nullptr;
        }
        host_worker_.dsp_handle = nullptr;
        host_worker_.enabled.store(false, std::memory_order_release);  // Key line
    } else {
        host_worker_.iface = iface;
        host_worker_.running.store(true, std::memory_order_release);
        host_worker_.worker_thread = std::thread(worker_thread_func, &host_worker_);
        host_worker_.enabled.store(true, std::memory_order_release);
    }
}
```

**Issue:** When ringbuffer allocation fails:
1. The feature was already provided to the plugin
2. Plugin has the `schedule_work` callback
3. But `enabled` is set to false and ringbuffers are null
4. Plugin may attempt to call `schedule_work()` anyway (it has the pointer)
5. Will get `LV2_WORKER_ERR_NO_SPACE` (which is correct)
6. But plugin doesn't know WHY or can't recover

### Impact
- Plugin may be in inconsistent state after failed allocation
- No way for plugin to know allocation failed
- May cause plugin to behave unpredictably

---

## Root Cause Summary

The crashes when changing MODEL, T_MODEL, C_MODEL ports occur because:

1. **Port change** triggers `schedule_work()` call from plugin (gxamp.cpp:590)
2. This goes directly to `host_schedule_work()` callback without `enabled` check
3. If `stop_worker()` is concurrently freeing ringbuffers:
   - Ringbuffer pointers become null or point to freed memory
   - `host_schedule_work()` writes to freed memory → **CRASH**
4. No synchronization primitive prevents concurrent access to ringbuffer pointers

---

## Recommended Fixes

### Fix #1: Gate host_schedule_work() with enabled flag
Add enabled check at the start of the callback:
```cpp
if (!w->enabled.load(std::memory_order_acquire)) {
    return LV2_WORKER_ERR_BUSY;
}
```

### Fix #2: Create scheduling reader counter
Add separate counter for active `schedule_work()` calls (separate from rt_readers for deliver_worker_responses):
```cpp
std::atomic<uint32_t> scheduler_readers{0};

// In host_schedule_work:
w->scheduler_readers.fetch_add(1, std::memory_order_acq_rel);
// ... operate on ringbuffers ...
w->scheduler_readers.fetch_sub(1, std::memory_order_acq_rel);

// In stop_worker - wait for BOTH counters:
while (host_worker_.rt_readers.load(...) != 0 ||
       host_worker_.scheduler_readers.load(...) != 0) {
    std::this_thread::sleep_for(...);
}
```

### Fix #3: Initialize ringbuffers BEFORE passing feature
Move ringbuffer allocation before `lilv_plugin_instantiate()`.

### Fix #4: Use atomic pointers for ringbuffers
Replace `lv2_ringbuffer_t*` with atomic pointers to avoid TOCTOU bugs:
```cpp
std::atomic<lv2_ringbuffer_t*> requests{nullptr};
std::atomic<lv2_ringbuffer_t*> responses{nullptr};
```

