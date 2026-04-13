# Findings (ordered by severity)

1. Missing LV2 worker end_run callback can break plugin state progression
- Status: Fixed
- Evidence: `deliver_worker_responses()` drains responses and calls `work_response`, but `end_run` is never called after `lilv_instance_run`.
- Locations: app/src/main/cpp/LV2Plugin.hpp:558, app/src/main/cpp/LV2Plugin.hpp:1181
- Impact: Plugins that rely on `end_run` for per-cycle finalization may behave incorrectly.

2. Potential null dereference if worker ringbuffers fail to allocate
- Status: Fixed
- Evidence: `iface` is set before allocating `requests`/`responses`, and allocation results are not checked.
- Locations: app/src/main/cpp/LV2Plugin.hpp:1085, app/src/main/cpp/LV2Plugin.hpp:1087, app/src/main/cpp/LV2Plugin.hpp:1088, app/src/main/cpp/LV2Plugin.hpp:1135, app/src/main/cpp/LV2Plugin.hpp:1186
- Impact: Crash risk under low-memory allocation failure.

3. Possible teardown race between audio thread and worker teardown
- Status: Fixed
- Evidence: `stop_worker()` frees worker resources while `process()` may still call `deliver_worker_responses()` based only on `iface` check.
- Locations: app/src/main/cpp/LV2Plugin.hpp:561, app/src/main/cpp/LV2Plugin.hpp:1210
- Impact: Potential use-after-free or data race during shutdown if lifecycle is not externally serialized.

4. Oversized worker responses are silently dropped
- Status: Fixed
- Evidence: Responses larger than `response_buffer` are drained and discarded with no retry/resize/log.
- Locations: app/src/main/cpp/LV2Plugin.hpp:1195, app/src/main/cpp/LV2Plugin.hpp:1201
- Impact: Functional failures for plugins returning larger response payloads.

5. Busy-spin path in worker thread on partial packet availability
- Status: Fixed
- Evidence: When only the size header is available but payload is incomplete, loop continues without sleep/yield.
- Location: app/src/main/cpp/LV2Plugin.hpp:1155
- Impact: Avoidable CPU usage under contention/timing skew.

6. Dead field `work_pending` is written but not used
- Status: Fixed
- Evidence: `work_pending.store(true, ...)` is set and never consumed.
- Location: app/src/main/cpp/LV2Plugin.hpp:1140
- Impact: Maintenance noise and unclear synchronization intent.

7. LV2_Worker_Schedule feature provided before ringbuffer allocation
- Status: Fixed
- Evidence: Pre-allocate ringbuffers at lines 1069-1078 BEFORE calling `lilv_plugin_instantiate()` at line 1094. Feature now has valid ringbuffer pointers from the start.
- Locations: app/src/main/cpp/LV2Plugin.hpp:1069-1078 (pre-allocation), app/src/main/cpp/LV2Plugin.hpp:1094 (instantiation), app/src/main/cpp/LV2Plugin.hpp:1107-1125 (worker setup uses pre-allocated buffers)
- Impact: Plugin can safely call `schedule_work()` during instantiation without failures.

8. Use-After-Free Race: concurrent schedule_work() and ringbuffer free
- Status: Fixed  
- Evidence: (1) Ringbuffers now atomic pointers at lines 1145-1146; (2) `host_schedule_work()` now adds/subtracts `scheduler_readers` counter at lines 1188, 1193, 1199, 1206; (3) `stop_worker()` now waits for both `rt_readers` AND `scheduler_readers` at lines 1273-1279; (4) Ringbuffer free uses atomic exchange at lines 1283-1291.
- Locations: app/src/main/cpp/LV2Plugin.hpp:1145-1146 (atomic pointers), app/src/main/cpp/LV2Plugin.hpp:1173-1206 (host_schedule_work with tracking), app/src/main/cpp/LV2Plugin.hpp:1273-1291 (stop_worker waits for both counters and frees safely)
- Impact: CRITICAL race condition eliminated - ringbuffers cannot be freed while schedule_work() is in progress.

9. Missing enabled gate in host_schedule_work callback
- Status: Fixed
- Evidence: Added enabled check at line 1181-1183 in `host_schedule_work()` matching the gate in `process()` -> `deliver_worker_responses()` at line 567.
- Locations: app/src/main/cpp/LV2Plugin.hpp:1181-1183 (enabled gate in host_schedule_work)
- Impact: Plugin cannot schedule work after shutdown begins; gate prevents writes to freed resources.

10. No reader counting for schedule_work calls, only for deliver_worker_responses
- Status: Fixed
- Evidence: Added `scheduler_readers` atomic counter at line 1150. `host_schedule_work()` increments/decrements it at lines 1188, 1193, 1199, 1206. `stop_worker()` waits for both `rt_readers` AND `scheduler_readers` to reach zero at lines 1273-1279 before freeing ringbuffers.
- Locations: app/src/main/cpp/LV2Plugin.hpp:1150 (scheduler_readers field), app/src/main/cpp/LV2Plugin.hpp:1188-1206 (tracking in host_schedule_work), app/src/main/cpp/LV2Plugin.hpp:1273-1279 (wait for both in stop_worker)
- Impact: CRITICAL synchronization added - ringbuffers not freed until all in-flight schedule_work() calls complete.

11. Allocation failure does not propagate to plugin
- Status: Fixed
- Evidence: Now pre-allocate at lines 1069-1078 BEFORE plugin instantiation. If allocation fails, ringbuffers remain null and are checked at lines 1115-1125. If null when plugin has worker support, error logged and worker disabled. Plugin receives feature but with null ringbuffers, preventing crashes.
- Locations: app/src/main/cpp/LV2Plugin.hpp:1069-1078 (pre-allocation with failure handling), app/src/main/cpp/LV2Plugin.hpp:1115-1125 (check before enabling worker)
- Impact: Graceful degradation - plugin still receives feature but scheduling consistently returns NO_SPACE, preventing undefined behavior.

12. Unprotected host_respond() calls during worker shutdown (CRITICAL RACE)
- Status: Fixed
- Evidence: Worker thread calls plugin's work() which invokes respond() callback. respond() (host_respond at line 1237) writes to responses ringbuffer without reader tracking. If stop_worker() frees ringbuffer while respond() is executing, UAF crash occurs. Added responder_readers counter at line 1151; host_respond() increments/decrements at lines 1248, 1252, 1259, 1264, 1268. stop_worker() waits for responder_readers==0 before freeing at lines 1293-1295.
- Locations: app/src/main/cpp/LV2Plugin.hpp:1151 (responder_readers field), app/src/main/cpp/LV2Plugin.hpp:1237-1268 (host_respond with tracking), app/src/main/cpp/LV2Plugin.hpp:1293-1295 (wait in stop_worker)
- Impact: CRITICAL - Prevents use-after-free in worker thread's response callback; eliminates random null pointer crashes during port changes and plugin lifecycle.

13. TOCTOU race on iface pointer - null dereference after check (CRITICAL)
- Status: Fixed
- Evidence: iface was non-atomic pointer, causing TOCTOU bugs: (1) worker_thread_func checks iface at line 1220, but it could be set to null by stop_worker before use at line 1229; (2) deliver_worker_responses checks iface at line 1270, calls it at line 1281; (3) process checks iface at line 568, accesses at line 572-573. Made iface atomic<const LV2_Worker_Interface*> at line 1163. All loads now use `iface.load(std::memory_order_acquire)` and stores use `.store(..., std::memory_order_release)` at lines 1118, 1332.
- Locations: app/src/main/cpp/LV2Plugin.hpp:1163 (atomic iface field), app/src/main/cpp/LV2Plugin.hpp:1220, 1270, 1332 (atomic loads/stores)
- Impact: CRITICAL - Eliminates null pointer dereferences in audio thread when iface becomes null during shutdown. This is the root cause of recent crashes with memcpy on null/freed memory.
