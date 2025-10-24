# Duplicate Timestamp Fix

## Problem Statement

The HTTP API has a bug where it keeps adding bars with the same timestamp instead of updating the existing bar. This caused multiple bars to appear with identical timestamps in AmiBroker's quotation editor.

### Evidence from Logs

```
OpenAlgo: GetQuotesEx - HTTP returned 9855 bars
OpenAlgo: GetQuotesEx - APPENDED tick bar at [9855]
...
OpenAlgo: GetQuotesEx - HTTP returned 9856 bars  <- Bar count increased!
OpenAlgo: GetQuotesEx - APPENDED tick bar at [9856]
...
OpenAlgo: GetQuotesEx - HTTP returned 9857 bars  <- Bar count increased again!
OpenAlgo: GetQuotesEx - APPENDED tick bar at [9857]
```

All these bars have the SAME timestamp (e.g., 11:07:00), creating duplicates.

## Root Cause

The HTTP API endpoint (`/api/v1/history`) is broken - instead of updating the current minute's bar, it appends a new bar with the same timestamp on every call. This is a **server-side bug**.

## Solution

### Client-Side Workaround

Since we cannot fix the server immediately, we implemented a client-side workaround in the plugin:

1. **Duplicate Detection** (Plugin.cpp:1875-1910)
   - After receiving HTTP bars, scan the last 20 bars
   - Detect if any bars have the same timestamp as the last bar
   - If duplicates found, keep only the FIRST occurrence
   - Remove all subsequent duplicate timestamp bars

2. **Logic**
   ```cpp
   // Check last few bars for duplicate timestamps
   for (int i = httpLastValid - 2; i >= max(0, httpLastValid - 20); i--)
   {
       if (currDate matches lastDate)
       {
           // Found duplicate - keep this one, remove all later duplicates
           cleanedBarCount = i + 1;
           break;
       }
   }
   ```

3. **Then Merge with Tick Bar**
   - After cleaning duplicates, check if last HTTP bar matches tick bar timestamp
   - If yes: REPLACE the HTTP bar with tick bar
   - If no: APPEND tick bar as new bar

## Enhanced Logging

Added comprehensive structured logging to help debug issues:

### HTTP API Response Logging
```
===== HTTP API RESPONSE =====
HTTP returned X bars for SYMBOL-EXCHANGE
HTTP Bar[9852]: 2025-10-24 11:04 O=100.0 H=101.0 L=99.0 C=100.5 V=1000
HTTP Bar[9853]: 2025-10-24 11:05 O=100.5 H=102.0 L=100.0 C=101.0 V=1500
HTTP Bar[9854]: 2025-10-24 11:06 O=101.0 H=101.5 L=100.5 C=101.0 V=800
DUPLICATE TIMESTAMP DETECTED! Removed 2 duplicate bars at timestamp 2025-10-24 11:06
After duplicate removal: 9853 bars
```

### WebSocket Tick Logging
```
===== WEBSOCKET TICK #123 =====
WS Tick: Symbol=SBIN-NSE LTP=765.50 Qty=10 TS=2025-10-24T11:06:45
WS Data: O=765.00 H=766.00 L=764.50 C=765.50 V=5000 OI=0
RT Enabled=1
Timestamp - Server=2025-10-24 11:06:45 System=2025-10-24 11:06:46 (using System)
```

### Tick Bar Data Logging
```
===== TICK BAR DATA =====
Tick Bar: 2025-10-24 11:06 O=765.00 H=766.00 L=764.50 C=765.50 V=5000 TickCnt=45
```

### Merge Result Logging
```
===== MERGE RESULT =====
REPLACED tick bar at index [9853]
Final bar: O=765.00 H=766.00 L=764.50 C=765.50 V=5000 TickCnt=45
```

### Final Result Logging
```
===== FINAL RESULT =====
Returning 9854 total bars (HTTP + tick) for SBIN-NSE
```

## Code Changes

### File: Plugin.cpp

**Lines 1853-1914**: HTTP response logging and duplicate detection
**Lines 1916-1930**: Tick bar data logging
**Lines 1932-1974**: Merge logic with enhanced logging
**Lines 1988-1992**: Final result logging
**Lines 3051-3060**: WebSocket tick logging with structured headers
**Lines 3104-3119**: Enhanced timestamp logging with readable format

## Testing

### Build Instructions
1. Open Visual Studio 2022
2. Select **Release x64** configuration (NOT Debug!)
3. Build → Rebuild Solution
4. Copy `x64\Release\OpenAlgo.dll` to AmiBroker plugin directory
5. Restart AmiBroker

### Verification
1. Open DebugView (Run as Administrator)
2. Filter: `OpenAlgo`
3. Open chart in AmiBroker
4. Watch logs for:
   - `DUPLICATE TIMESTAMP DETECTED!` messages (if HTTP API has duplicates)
   - `REPLACED tick bar` or `APPENDED tick bar` messages
   - Structured logging with clear sections

### Expected Results
- ✅ NO duplicate timestamps in quotation editor
- ✅ Each minute should have exactly ONE bar
- ✅ Tick bar should REPLACE HTTP bar when timestamps match
- ✅ Clear, structured logs showing HTTP data, WebSocket ticks, and merge results

## Notes

- This fix is a **workaround** for a server-side bug
- The proper fix should be made in the OpenAlgo Python server
- Server should update the current minute's bar instead of appending duplicates
- Enhanced logging will help identify other issues quickly

## Related Files
- `PING_PONG_FIX.md` - WebSocket keepalive fix
- `KEEPALIVE_FIX_SUMMARY.md` - Quick reference
- `FIXES_APPLIED.md` - Comprehensive testing guide
