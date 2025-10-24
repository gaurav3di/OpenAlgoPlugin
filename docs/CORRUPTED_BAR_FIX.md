# Corrupted Bar & Improved Duplicate Detection Fix

## Issues Discovered from Logs

### Issue #1: Corrupted Last Bar from HTTP API

**Evidence from Logs:**
```
HTTP Bar[9794]: 2025-10-24 31:63 O=1450.00 H=1450.50 L=1441.00 C=1447.40 V=4351266
```

**Problem:** HTTP API returns a bar with INVALID timestamp:
- Hour = 31 (valid range: 0-23)
- Minute = 63 (valid range: 0-59)

This corrupted bar appears as the **LAST bar** in EVERY HTTP response, breaking the previous duplicate detection logic.

### Issue #2: Previous Duplicate Detection Was Broken

**Why it Failed:**
1. Compared all bars against the LAST bar's timestamp
2. But the last bar has a corrupted timestamp (31:63)
3. Valid bars (11:28, 11:29, etc.) never matched the corrupted timestamp
4. Result: Duplicates were NEVER detected or removed!

**Evidence:**
```
HTTP Bar[9794]: 2025-10-24 11:28 O=1448.00 H=1448.00 L=1447.50 C=1447.50 V=232
HTTP Bar[9795]: 2025-10-24 11:28 O=1448.00 H=1448.00 L=1448.00 C=1448.00 V=204
After duplicate removal: 9796 bars  <- BOTH STILL THERE!
```

### Issue #3: HTTP API Adds Bars Instead of Updating

**Pattern Observed:**
```
GetQuotesEx #3: HTTP returned 9795 bars
  Bar[9793]: 11:27 O=1447.50 H=1448.30 L=1447.20 C=1448.00 V=59340
  Bar[9794]: 11:28 O=1448.00 H=1448.00 L=1448.00 C=1448.00 V=204

GetQuotesEx #4: HTTP returned 9796 bars (increased by 1!)
  Bar[9793]: 11:27 ...
  Bar[9794]: 11:28 O=1448.00 H=1448.00 L=1447.50 C=1447.50 V=232  <- NEW bar, same timestamp!
  Bar[9795]: 11:28 O=1448.00 H=1448.00 L=1448.00 C=1448.00 V=204  <- Old bar still there

GetQuotesEx #5: HTTP returned 9797 bars (increased again!)
  Bar[9794]: 11:28 O=1448.00 H=1448.00 L=1447.50 C=1447.70 V=235  <- ANOTHER new bar!
  Bar[9795]: 11:28 O=1448.00 H=1448.00 L=1447.50 C=1447.50 V=232
  Bar[9796]: 11:28 O=1448.00 H=1448.00 L=1448.00 C=1448.00 V=204
```

The HTTP API keeps ADDING new bars with the same timestamp instead of UPDATING the existing bar.

## New Solution

### Two-Step Cleanup Process

**STEP 1: Remove Corrupted Bar**
```cpp
// Check for invalid timestamp (Hour > 23 or Minute > 59)
if (lastBar.PackDate.Hour > 23 || lastBar.PackDate.Minute > 59)
{
    // Remove the corrupted bar
    cleanedBarCount = httpLastValid - 1;
    Log: "CORRUPTED BAR DETECTED! Removed bar with invalid timestamp..."
}
```

**STEP 2: Comprehensive Duplicate Removal**
- Scan last **50 bars** (increased from 20 for more thorough cleanup)
- For each timestamp, keep **LAST occurrence** (most recent data from server)
- Remove all earlier occurrences (outdated data)
- Compact array to remove gaps

**Algorithm:**
```cpp
// Mark which bars to keep
for each bar from end-2 to start:
    Check if timestamp exists in any LATER bar
    If YES → Mark as duplicate, REMOVE
    If NO → Mark to KEEP

// Compact array (move kept bars to front)
newCount = 0
for each bar:
    if (keepBar[i]):
        pQuotes[newCount++] = pQuotes[i]

Log: "DUPLICATE CLEANUP SUMMARY: Removed X duplicate bars"
```

### Why Keep LAST Occurrence?

The logs show each new bar has MORE recent data:
- Bar #1 at 11:28: V=204 (old tick data)
- Bar #2 at 11:28: V=232 (updated with more ticks)
- Bar #3 at 11:28: V=235 (latest data)

We want the **LATEST data** from the server, so we keep the LAST occurrence.

## Expected Results

### Before Fix:
```
HTTP returned 9797 bars
  Bar[9794]: 11:28 V=235
  Bar[9795]: 11:28 V=232
  Bar[9796]: 11:28 V=204
  Bar[9797]: 31:63 (corrupted)
After duplicate removal: 9797 bars  <- NO CHANGE!
```

### After Fix:
```
HTTP returned 9797 bars
  Bar[9794]: 11:28 V=235
  Bar[9795]: 11:28 V=232
  Bar[9796]: 11:28 V=204
  Bar[9797]: 31:63 (corrupted)

CORRUPTED BAR DETECTED! Removed bar with invalid timestamp 31:63
Removing duplicate bar[9794] at 11:28 (keeping bar[9796] with newer data)
Removing duplicate bar[9795] at 11:28 (keeping bar[9796] with newer data)
DUPLICATE CLEANUP SUMMARY: Removed 2 duplicate bars

After cleanup: 9795 bars (removed corrupted + duplicates)
  Bar[9794]: 11:27 V=59340
  Bar[9795]: 11:28 V=235  <- ONLY ONE bar at 11:28, with latest data!
```

## About Low Tick Frequency

### Observation from Logs
```
WEBSOCKET TICK #1: 11:28:33
WEBSOCKET TICK #2: 11:28:38 (5 seconds later)
WEBSOCKET TICK #3: 11:28:43 (5 seconds later)
...
Total: ~20 ticks per minute
```

### This is NOT a Plugin Bug

**Analysis:**
- RELIANCE is a highly liquid stock
- During active trading hours, should have **hundreds** of ticks per minute
- Plugin is receiving ALL ticks the WebSocket server sends
- Server is **throttling/batching** tick data (sending ~1 tick every 3-5 seconds)

**Possible Reasons:**
1. **Broker API throttling** - Upstox free tier may have rate limits
2. **WebSocket server batching** - OpenAlgo server batches ticks before sending
3. **Market hours** - Low activity time (check if during market hours)

**NOT a Client Issue:**
- WebSocket connection is stable (PING/PONG working correctly)
- Plugin processes ticks immediately when received
- Auto-reconnect working (reconnects after server closes every ~40 seconds)

## Server-Side Issues (For OpenAlgo Team to Fix)

1. **Corrupted Bar Bug** - HTTP API returns invalid timestamp (31:63)
2. **Duplicate Bar Bug** - HTTP API adds new bars instead of updating
3. **WebSocket Disconnects** - Server closes connection every ~40 seconds despite correct PONG
4. **Low Tick Frequency** - Only sending ~20 ticks/minute for liquid stocks

## Code Changes

**File:** `Plugin.cpp`

**Lines 1875-1980:** Comprehensive duplicate detection and corrupted bar removal

## Build Instructions

1. Open Visual Studio 2022
2. Select **Release x64** configuration
3. Build → Rebuild Solution
4. Copy `x64\Release\OpenAlgo.dll` to AmiBroker plugins
5. Restart AmiBroker

## Testing

**Watch for these new logs in DebugView:**

1. **Corrupted Bar Detection:**
   ```
   CORRUPTED BAR DETECTED! Removed bar with invalid timestamp 2025-10-24 31:63
   ```

2. **Duplicate Removal:**
   ```
   Removing duplicate bar[9794] at 2025-10-24 11:28 (keeping bar[9796] with newer data)
   DUPLICATE CLEANUP SUMMARY: Removed 2 duplicate bars
   ```

3. **Final Cleanup:**
   ```
   After cleanup: 9795 bars (removed corrupted + duplicates)
   ```

4. **Quotation Editor:**
   - Open quotation editor
   - Scroll to bottom
   - **Verify:** Each timestamp appears ONLY ONCE
   - **Verify:** No duplicate rows

## Related Files

- `DUPLICATE_TIMESTAMP_FIX.md` - Previous duplicate detection attempt
- `WEBSOCKET_PING_PONG_FIX.md` - WebSocket keepalive fix
- `FIXES_APPLIED.md` - Comprehensive testing guide
