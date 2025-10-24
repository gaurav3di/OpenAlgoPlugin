# Auto-Reconnect Fix - Critical Bug

## Problem Discovered from Logs

### Updates Stop After 2-3 Minutes

**User Report:**
- Ticks flow much faster after WebSocket timer fix ✅
- But after 2-3 minutes, updates COMPLETELY STOP ❌
- No auto-reconnect happens
- Connection stays dead forever

### Log Analysis (logs.txt)

**Line 2397-2398 - Server Closes Connection:**
```
OpenAlgo: DecodeWebSocketFrame returned: CLOSE_FRAME (Status Code: 1011, Reason: keepalive ping timeout)
OpenAlgo: Received CLOSE_FRAME from server - closing connection
```

Server still sends "keepalive ping timeout" even though PING/PONG is working correctly. This appears to be a **server-side bug** in the OpenAlgo WebSocket proxy.

**Lines 54-1863 - ProcessWebSocketData() Called Regularly:**
```
ProcessWebSocketData() call #1 - Connected=1 Socket=1
ProcessWebSocketData() call #2 - Connected=1 Socket=1
...
ProcessWebSocketData() call #500 - Connected=1 Socket=1  <- LAST CALL!
```

**After Line 2398 - ZERO ProcessWebSocketData() Calls:**
```
[CLOSE_FRAME at 11:56:05]
...158 lines of logs...
[End of file - NO MORE ProcessWebSocketData() calls!]
```

The timer stopped calling `ProcessWebSocketData()` after disconnect!

## Root Cause

### The Fatal Bug (Plugin.cpp:1487)

**Timer Callback Code:**
```cpp
if (idEvent == TIMER_WEBSOCKET)
{
    // Process WebSocket data if real-time candles are enabled
    if (g_bRealTimeCandlesEnabled && g_bWebSocketConnected)  // ❌ BUG HERE!
    {
        ProcessWebSocketData();
    }
    return;
}
```

**What Happens:**

1. **Normal Operation:**
   - `g_bWebSocketConnected = TRUE`
   - Timer calls `ProcessWebSocketData()` every 100ms ✅
   - Ticks flow normally ✅

2. **Server Sends CLOSE_FRAME:**
   - `ProcessWebSocketData()` handles CLOSE_FRAME
   - Sets `g_bWebSocketConnected = FALSE` (Plugin.cpp:3024)
   - Closes socket

3. **Timer Callback Checks Connection:**
   - Sees `g_bWebSocketConnected = FALSE`
   - **SKIPS calling `ProcessWebSocketData()`** ❌
   - Timer continues firing, but ProcessWebSocketData never runs!

4. **Auto-Reconnect Never Runs:**
   - Auto-reconnect logic is INSIDE `ProcessWebSocketData()` (lines 2842-2866)
   - But `ProcessWebSocketData()` is never called!
   - Connection stays dead forever ❌

### Why This is Wrong

**ProcessWebSocketData() Has Built-In Auto-Reconnect:**
```cpp
BOOL ProcessWebSocketData(void)
{
    if (!g_bWebSocketConnected || g_websocket == INVALID_SOCKET)
    {
        // Auto-reconnect if disconnected
        static DWORD lastReconnectAttempt = 0;
        DWORD now = (DWORD)GetTickCount64();

        // Try reconnecting every 5 seconds
        if ((now - lastReconnectAttempt) > 5000)
        {
            lastReconnectAttempt = now;
            OutputDebugString(_T("OpenAlgo: ProcessWebSocketData() - NOT CONNECTED, attempting reconnect..."));

            if (InitializeWebSocket())
            {
                OutputDebugString(_T("OpenAlgo: *** AUTO-RECONNECT SUCCESSFUL! ***"));
                return TRUE;
            }
        }
        return FALSE;
    }

    // Normal processing...
}
```

**The timer callback was preventing this auto-reconnect logic from running!**

## Solution

### Remove Connection Check from Timer Callback

**Before (BROKEN):**
```cpp
if (idEvent == TIMER_WEBSOCKET)
{
    if (g_bRealTimeCandlesEnabled && g_bWebSocketConnected)  // ❌ Checks connection
    {
        ProcessWebSocketData();
    }
    return;
}
```

**After (FIXED):**
```cpp
if (idEvent == TIMER_WEBSOCKET)
{
    // ALWAYS call ProcessWebSocketData() if real-time candles are enabled
    // ProcessWebSocketData() has its own auto-reconnect logic when disconnected
    // DO NOT check g_bWebSocketConnected here, or auto-reconnect will never run!
    if (g_bRealTimeCandlesEnabled)
    {
        ProcessWebSocketData();
    }
    return;
}
```

**Why This Works:**
- Timer ALWAYS calls `ProcessWebSocketData()` (if RT candles enabled)
- When connected: Processes WebSocket data normally
- When disconnected: Auto-reconnect logic runs every 5 seconds
- Connection automatically restored! ✅

## Expected Behavior After Fix

### Before Fix (BROKEN):
```
11:54:00 - Connected, ticks flowing ✅
11:54:01 - Ticks flowing ✅
...
11:56:05 - Server sends CLOSE_FRAME ❌
11:56:05 - g_bWebSocketConnected = FALSE
11:56:06 - Timer fires → Sees FALSE → SKIPS ProcessWebSocketData() ❌
11:56:07 - Timer fires → Sees FALSE → SKIPS ProcessWebSocketData() ❌
...forever...
11:59:00 - Still disconnected, NO auto-reconnect! ❌
```

### After Fix (CORRECT):
```
11:54:00 - Connected, ticks flowing ✅
11:54:01 - Ticks flowing ✅
...
11:56:05 - Server sends CLOSE_FRAME ⚠️
11:56:05 - g_bWebSocketConnected = FALSE
11:56:06 - Timer fires → CALLS ProcessWebSocketData() → Sees disconnected → Too soon to retry
11:56:07 - Timer fires → CALLS ProcessWebSocketData() → Sees disconnected → Too soon to retry
...
11:56:10 - Timer fires → CALLS ProcessWebSocketData() → 5 seconds elapsed → Auto-reconnect! ✅
11:56:10 - *** AUTO-RECONNECT SUCCESSFUL! *** ✅
11:56:10 - Re-subscribes to symbols ✅
11:56:11 - Ticks flowing again! ✅
```

## New Logs to Watch For

### After Disconnect:
```
OpenAlgo: DecodeWebSocketFrame returned: CLOSE_FRAME (Status Code: 1011, Reason: keepalive ping timeout)
OpenAlgo: Received CLOSE_FRAME from server - closing connection

[100ms later - Timer continues calling ProcessWebSocketData()]
OpenAlgo: ProcessWebSocketData() call #X - Connected=0 Socket=0

[5 seconds later - Auto-reconnect triggers]
OpenAlgo: ProcessWebSocketData() - NOT CONNECTED, attempting reconnect...
OpenAlgo: InitializeWebSocket() called
OpenAlgo: InitializeWebSocket - Starting connection to ws://127.0.0.1:8765
OpenAlgo: AuthenticateWebSocket - Authentication SUCCESSFUL!
OpenAlgo: *** AUTO-RECONNECT SUCCESSFUL! ***

[Symbols re-subscribe automatically]
OpenAlgo: SubscribeToSymbol called for: RELIANCE-NSE
OpenAlgo: SubscribeToSymbol - Successfully subscribed

[Ticks resume]
OpenAlgo: ===== WEBSOCKET TICK #N =====
```

### Connection Stays Alive Indefinitely:
- Ticks flow continuously
- If server disconnects: Auto-reconnect within 5 seconds
- No more "stuck" state

## Performance Impact

**Minimal:**
- When connected: Same as before (100ms timer processes data)
- When disconnected: Timer still fires every 100ms, but ProcessWebSocketData() returns immediately
- Auto-reconnect: Only attempted once every 5 seconds (minimal overhead)

## Testing

### Verify Auto-Reconnect:

1. **Open DebugView** (Run as Administrator)
2. **Filter**: `OpenAlgo`
3. **Watch for CLOSE_FRAME** (server will send after ~2-3 minutes due to server bug)
4. **Verify auto-reconnect happens within 5 seconds**
5. **Verify ticks resume immediately after reconnect**

### Long-Duration Test:

1. **Open chart** and let it run for 10+ minutes
2. **Watch logs** - should see multiple disconnect/reconnect cycles
3. **Verify** ticks continue flowing despite disconnects
4. **No more "stuck" state!**

## Related Issues

### Server-Side Bug (Not Fixed by This)

The server still sends `CLOSE_FRAME (Status Code: 1011, Reason: keepalive ping timeout)` even though:
- ✅ Plugin sends PING every 30 seconds
- ✅ Plugin echoes PONG with correct payload

This is a **server-side bug** in the OpenAlgo WebSocket proxy. The server should be fixed, but this client-side auto-reconnect provides a **robust workaround**.

### HTTP API Corruption (Separate Issue)

**Log Line 2426:**
```
DUPLICATE CLEANUP SUMMARY: Removed 2588 duplicate bars
After cleanup: 50 bars (removed corrupted + duplicates)
```

HTTP API returned 2639 bars, but 2588 are duplicates (98% corruption!). This is a **critical server-side bug** in the OpenAlgo HTTP API that needs to be fixed.

Client-side duplicate detection helps, but the root cause is server-side.

## Summary

**Before:** Timer checked connection status BEFORE calling ProcessWebSocketData()
**After:** Timer ALWAYS calls ProcessWebSocketData(), which handles both processing and auto-reconnect

This is a **CRITICAL fix** for production stability! Without it, the plugin becomes unusable after the first disconnect. 🎯

## Code Changes

**File:** Plugin.cpp
**Line:** 1487

**Changed:**
```cpp
if (g_bRealTimeCandlesEnabled && g_bWebSocketConnected)  // ❌ WRONG
```

**To:**
```cpp
if (g_bRealTimeCandlesEnabled)  // ✅ CORRECT
```

Simple one-line fix with massive impact!
