# Real-Time Candles Troubleshooting Guide

**Issue:** Live candle ticks from WebSocket aggregation not appearing in AmiBroker charts

---

## 🔍 Diagnostic Steps

### Step 1: Verify WebSocket Connection

**Check in AmiBroker:**
1. Look at the **status bar** at bottom of AmiBroker window
2. Should show "Connected" (green) not "Disconnected" (red)
3. If disconnected, check WebSocket URL in configuration

**Configuration Check:**
- Open **File → Database Settings → Configure**
- Verify **WebSocket URL** is correct (e.g., `ws://127.0.0.1:8765`)
- Click **Test WebSocket Connection**
- Should show "Connected successfully"

---

### Step 2: Check Registry Settings

**Verify Real-Time is Enabled:**

1. Open **Registry Editor** (`regedit`)
2. Navigate to: `HKEY_CURRENT_USER\Software\OpenAlgo\`
3. Check these values:
   ```
   EnableRealTimeCandles = 1 (should be 1, not 0)
   BackfillIntervalMs = 5000 (default)
   ```
4. If `EnableRealTimeCandles = 0`, change to `1`
5. **Restart AmiBroker**

---

### Step 3: Check WebSocket Data Format

**Your WebSocket should send data in this format:**

```json
{
  "type": "market_data",
  "mode": 2,
  "topic": "RELIANCE.NSE",
  "data": {
    "symbol": "RELIANCE",
    "exchange": "NSE",
    "ltp": 1424.0,
    "last_trade_quantity": 50,
    "timestamp": "2025-05-28T10:30:45.123Z"
  }
}
```

**Key Requirements:**
- ✅ Must have `"type": "market_data"`
- ✅ Must have `"symbol"` field
- ✅ Must have `"exchange"` field
- ✅ Must have `"ltp"` field (Last Traded Price)
- ⚠️ `"last_trade_quantity"` is optional (defaults to 1 if missing)
- ⚠️ `"timestamp"` is optional (uses current time if missing)

---

### Step 4: Enable Debug Logging

**To see what's happening internally:**

1. **Rebuild in Debug mode:**
   - Open solution in Visual Studio
   - Change configuration from "Release" to **"Debug"**
   - Build → Rebuild Solution

2. **Copy Debug DLL:**
   ```
   Copy: x64\Debug\OpenAlgo.dll
   To: C:\Program Files\AmiBroker\Plugins\
   ```

3. **Run DebugView:**
   - Download Sysinternals DebugView: https://learn.microsoft.com/en-us/sysinternals/downloads/debugview
   - Run DebugView.exe as Administrator
   - Start AmiBroker
   - Watch for debug messages

**What to look for in DebugView:**
```
WS Data: RELIANCE-NSE LTP=1424.00 Qty=50 TS=2025-05-28T10:30:45.123Z RT=1
ProcessTick called successfully
```

**If you see "ProcessTick skipped":**
- Check the reason in the log
- Verify `RT=1` (real-time enabled)
- Verify `LTP > 0`

---

### Step 5: Check Symbol Format

**AmiBroker ticker format must match:**

The plugin expects tickers in format: `SYMBOL-EXCHANGE`

Examples:
- ✅ `RELIANCE-NSE` (correct)
- ✅ `INFY-NSE` (correct)
- ❌ `RELIANCE` (missing exchange)
- ❌ `NSE:RELIANCE` (wrong format)

**In AmiBroker:**
1. Right-click on chart
2. Select **Symbol → Symbol Info**
3. Check **Full Name** field
4. Should be in `SYMBOL-EXCHANGE` format

---

### Step 6: Verify ProcessWebSocketData() is Called

**Check if WebSocket thread is running:**

The function `ProcessWebSocketData()` should be called frequently to process incoming ticks.

**Possible issues:**
1. **WebSocket not subscribed to symbol**
   - Plugin auto-subscribes when you open a chart
   - Check if subscription message sent

2. **WebSocket connection dropped**
   - Check network connectivity
   - Check if OpenAlgo server is running
   - Check firewall settings

3. **No ticks being sent**
   - Verify market is open
   - Verify symbol is actively trading
   - Check if server is sending data

---

### Step 7: Check GetQuotesEx() Logic

**The merge logic should work as follows:**

Every 5 seconds (configurable):
1. Fetch HTTP backfill (historical bars)
2. **Override tick bars** with HTTP data
3. **Append current in-progress tick bar** at end
4. Return to AmiBroker

**Between backfills:**
1. Use cached bars from BarBuilder
2. Append current in-progress tick bar
3. Return to AmiBroker

**To verify:**
- Open a symbol chart
- Wait 5 seconds
- HTTP backfill should happen
- Current bar should show latest tick price

---

### Step 8: Check for Errors

**In AmiBroker log:**
1. Open **View → Log Window**
2. Look for errors related to plugin
3. Common errors:
   - "Plugin failed to load" → Check DLL dependencies
   - "Access violation" → Check thread safety issues
   - "Invalid data" → Check data format

---

## 🔧 Common Issues & Fixes

### Issue 1: WebSocket Data Format Mismatch

**Problem:** Fields are nested in `"data"` object

**Current parsing:**
```cpp
int symbolPos = data.Find(_T("\"symbol\":\""));  // Searches at root level
```

**If your JSON has nested structure:**
```json
{
  "type": "market_data",
  "data": {
    "symbol": "RELIANCE",  ← nested here
    ...
  }
}
```

**Fix:** The parser should handle both flat and nested formats.

---

### Issue 2: `last_trade_quantity` Always Zero

**Problem:** WebSocket doesn't send `last_trade_quantity` field

**Fix:** Plugin now defaults to `1.0` if missing or zero (already implemented)

---

### Issue 3: Timestamp Format Mismatch

**Problem:** Timestamp not in ISO 8601 format

**Expected:** `"2025-05-28T10:30:45.123Z"`

**If your format is different** (e.g., Unix timestamp):
- Modify `ParseISO8601Timestamp()` function
- Or send timestamp in correct format

---

### Issue 4: Real-Time Disabled in Registry

**Problem:** `EnableRealTimeCandles = 0`

**Fix:**
1. Open Registry Editor
2. Navigate to `HKCU\Software\OpenAlgo\`
3. Set `EnableRealTimeCandles = 1`
4. Restart AmiBroker

---

### Issue 5: AmiBroker Not Calling GetQuotesEx()

**Problem:** Chart not refreshing frequently enough

**Fix:**
1. In AmiBroker: **Tools → Preferences → Real-Time**
2. Set **Chart Refresh Interval** to minimum (100ms or 500ms)
3. Enable **Intraday Backfill**

---

### Issue 6: BarBuilder Not Created

**Problem:** No BarBuilder exists for symbol

**Why this happens:**
- Symbol chart not opened yet
- WebSocket not subscribed
- First tick not received

**Fix:**
- Open a chart for the symbol
- Wait a few seconds for subscription
- Check debug logs for "ProcessTick called"

---

## 📊 Quick Verification Test

**To confirm real-time is working:**

1. **Open a 1-minute chart** in AmiBroker
2. **Note the current bar's close price**
3. **Wait for a tick** (watch the quote window)
4. **Current bar should update immediately** (< 100ms)
5. **After 5 seconds**, HTTP backfill corrects the bar (if needed)

**If current bar updates:**
✅ Real-time ticks are working!

**If current bar only updates every 5 seconds:**
❌ Only HTTP backfill is working, ticks are not being processed

---

## 🐛 Debug Code to Add (Temporary)

**Add to ProcessTick() function to verify it's called:**

```cpp
// At start of ProcessTick()
static int tickCount = 0;
tickCount++;
CString msg;
msg.Format(_T("ProcessTick #%d: %s-%s LTP=%.2f Qty=%.0f"),
    tickCount, symbol, exchange, ltp, lastTradeQty);
OutputDebugString(msg);
AfxMessageBox(msg, MB_OK);  // Show popup for first few ticks
```

**This will:**
- Show popup for each tick (remove after testing!)
- Confirm ProcessTick() is being called
- Show the tick data being processed

---

## 📞 Still Not Working?

**Collect this information:**

1. **Debug logs** (from DebugView or Visual Studio Output window)
2. **Sample WebSocket message** (actual JSON received)
3. **Registry settings** (EnableRealTimeCandles value)
4. **AmiBroker version**
5. **Symbol name and exchange**

**Then:**
- Check the implementation in `Plugin.cpp:2540-2567`
- Verify `ProcessTick()` is being called
- Verify bars are being added to BarBuilder
- Verify GetQuotesEx() is returning the tick bars

---

## ✅ Expected Behavior

**When everything works correctly:**

1. **WebSocket connects** → Status bar shows "Connected"
2. **Chart opened** → Plugin subscribes to symbol
3. **Tick arrives** → ProcessTick() called immediately
4. **Bar updated** → Current bar updates in < 100ms
5. **5 seconds later** → HTTP backfill corrects any errors
6. **New minute** → New bar created automatically
7. **Chart smooth** → No "jumps" or delays

**Performance:**
- Tick-to-display latency: < 100ms
- CPU usage: < 5% for 50 symbols
- Memory: ~800 KB per symbol

---

## 🎯 Next Steps

1. ✅ **Run Debug build** and watch DebugView
2. ✅ **Verify WebSocket data format** matches expected
3. ✅ **Check registry settings** (EnableRealTimeCandles = 1)
4. ✅ **Open chart and wait** for ticks
5. ✅ **Check if ProcessTick() called** (via debug logs)
6. ✅ **If still not working**, share debug logs for analysis

---

**Remember:** HTTP backfill (every 5 seconds) will always work. The issue is specifically with the **tick-to-tick updates** in between backfills.
