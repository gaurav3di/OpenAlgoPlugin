# Critical Fixes Applied - October 24, 2025

## ✅ Fix #1: PING/PONG Keepalive (RFC 6455 Compliance)

**Problem:** Server closes connection every 30-60 seconds with "keepalive ping timeout"

**Root Cause:** Plugin sent PONG with empty payload, but RFC 6455 requires echoing PING's payload exactly

**Fix Applied:**
- `Plugin.cpp` line 2163: Extract PING payload in `DecodeWebSocketFrame()`
- `Plugin.cpp` line 2770: Echo payload in PONG response

**Expected Result:**
```
✅ OpenAlgo: Received PING with 4-byte payload, sent PONG with echoed payload
```

---

## ✅ Fix #2: Duplicate Timestamps in Quotations

**Problem:** Multiple bars with same timestamp (e.g., 20 bars at `10:49:00`)

**Root Cause:** HTTP backfill includes current minute's bar, then we APPENDED tick bar, creating duplicate

**Fix Applied:**
- `Plugin.cpp` line 1857-1902: Check if last HTTP bar has same timestamp as tick bar
  - If **SAME**: REPLACE it (overwrite)
  - If **DIFFERENT**: APPEND it (new minute)

**Expected Result:**
```
✅ OpenAlgo: GetQuotesEx - REPLACED tick bar at [92613]: O=1520.90 H=1521.20 L=1520.90 C=1521.10 V=780 TickCnt=20
```

---

## 🔨 Build Instructions

### CRITICAL: You MUST Rebuild in Visual Studio

1. **Open Visual Studio 2022**
2. Open `C:\Users\Admin1\source\repos\OpenAlgoPlugin\OpenAlgoPlugin.sln`
3. Select **Release** configuration
4. Select **x64** platform
5. **Build → Rebuild Solution** (Ctrl+Shift+B)
6. Wait for completion
7. Output: `C:\Users\Admin1\source\repos\OpenAlgoPlugin\Release\OpenAlgo.dll`

### Install New DLL

1. **Close AmiBroker completely**
2. Navigate to `C:\Program Files\AmiBroker\Plugins\`
3. **Backup:** Rename `OpenAlgo.dll` to `OpenAlgo.dll.old`
4. **Copy:** New DLL from `Release\OpenAlgo.dll` to Plugins folder
5. **Restart AmiBroker**

---

## 🧪 Testing Checklist

### Test 1: PING/PONG Fix

Open **DebugView** and look for:

✅ **SUCCESS:**
```
OpenAlgo: DecodeWebSocketFrame returned: PING_FRAME:XXXXXXXX
OpenAlgo: Received PING with 4-byte payload, sent PONG with echoed payload
```

❌ **FAILURE (old code still loaded):**
```
OpenAlgo: DecodeWebSocketFrame returned: PING_FRAME
OpenAlgo: Received PING, sent PONG
```

### Test 2: Duplicate Timestamps Fix

Open **Quotations Editor** in AmiBroker:

✅ **SUCCESS:** Each timestamp appears **ONLY ONCE**
```
24-10-2025 10:54:00  (1 bar)
24-10-2025 10:53:00  (1 bar)
24-10-2025 10:52:00  (1 bar)
```

❌ **FAILURE:** Same timestamp appears **MULTIPLE TIMES**
```
24-10-2025 10:49:00  (20 bars) ← WRONG!
```

Look for in DebugView:
```
✅ OpenAlgo: GetQuotesEx - REPLACED tick bar at [92613]
```

### Test 3: Connection Stability

Monitor for **2-3 minutes**:

✅ **SUCCESS:** No disconnections, continuous ticks
❌ **FAILURE:** `SERVER CLOSED CONNECTION` appears

---

## 📋 Before vs After

### BEFORE (Broken)

**Quotations Editor:**
```
RELIANCE  24-10-2025 10:49:00  1445.5  Volume: 105
RELIANCE  24-10-2025 10:49:00  1445.5  Volume: 100  ← DUPLICATE!
RELIANCE  24-10-2025 10:49:00  1445.5  Volume: 93   ← DUPLICATE!
RELIANCE  24-10-2025 10:49:00  1445.5  Volume: 92   ← DUPLICATE!
...
```

**DebugView:**
```
OpenAlgo: Received PING, sent PONG
OpenAlgo: SERVER CLOSED CONNECTION
```

### AFTER (Fixed)

**Quotations Editor:**
```
RELIANCE  24-10-2025 10:54:00  1447.5  Volume: 13755
RELIANCE  24-10-2025 10:53:00  1447.2  Volume: 25357  ← Each timestamp ONCE!
RELIANCE  24-10-2025 10:52:00  1447.7  Volume: 24439
```

**DebugView:**
```
OpenAlgo: Received PING with 4-byte payload, sent PONG with echoed payload
OpenAlgo: GetQuotesEx - REPLACED tick bar at [92613]
```

---

## ⚠️ Important Notes

1. **Both fixes require rebuilding** - source code changes don't take effect until you compile
2. **Use Visual Studio 2022** - VS Code is for editing, not building C++ DLLs
3. **Close AmiBroker before copying DLL** - otherwise file is locked
4. **Check DebugView logs** to verify which code version is running

---

## 🐛 Troubleshooting

### "Old code still running"

**Symptom:** DebugView shows old log messages
**Cause:** DLL wasn't replaced or AmiBroker wasn't restarted
**Fix:**
1. Close AmiBroker completely (check Task Manager)
2. Delete `OpenAlgo.dll` from Plugins folder
3. Copy new DLL
4. Start AmiBroker

### "Build failed"

**Symptom:** Visual Studio shows errors
**Cause:** Syntax error or missing includes
**Fix:**
1. Check error messages
2. Verify all includes present
3. Clean solution and rebuild

### "Still seeing duplicates"

**Symptom:** Multiple bars with same timestamp
**Cause:** New DLL not loaded
**Fix:**
1. Check file timestamp of `OpenAlgo.dll` in Plugins folder
2. Should match the time you copied it
3. Restart AmiBroker

---

**Status:** ✅ FIXES APPLIED - READY TO BUILD AND TEST

**Next Step:** BUILD IN VISUAL STUDIO (not VS Code!)
