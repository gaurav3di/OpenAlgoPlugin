# Product Requirements Document: Real-Time Candle Building

**Document Version:** 1.0
**Date:** 2025-10-23
**Status:** Feasibility Study
**Author:** Technical Architecture Team

---

## Executive Summary

This PRD outlines the design and implementation strategy for converting OpenAlgoPlugin from interval-based quote updates to true tick-by-tick real-time candle building. The feature will aggregate WebSocket tick data (LTP + last_trade_quantity) into OHLC bars in real-time, providing live chart updates without polling delays.

### Current State
- Bars update on fixed intervals (default: 5 seconds)
- WebSocket ticks are cached but not aggregated
- GetRecentInfo() provides snapshot quotes only
- Historical data from HTTP API backfill

### Proposed State
- Bars update on every tick (real-time)
- Tick-to-bar aggregation engine
- Volume calculated from trade quantities
- Seamless integration with historical backfill
- Sub-second chart responsiveness

---

## Table of Contents

1. [Business Context](#1-business-context)
2. [Current Architecture Analysis](#2-current-architecture-analysis)
3. [Proposed Solution](#3-proposed-solution)
4. [Technical Design](#4-technical-design)
5. [Implementation Phases](#5-implementation-phases)
6. [Risk Assessment](#6-risk-assessment)
7. [Performance Considerations](#7-performance-considerations)
8. [Testing Strategy](#8-testing-strategy)
9. [Success Metrics](#9-success-metrics)
10. [Open Questions](#10-open-questions)

---

## 1. Business Context

### 1.1 Problem Statement

**Current Limitations:**
- Charts appear "choppy" with 5-second refresh intervals
- Price action between refresh intervals is invisible to users
- Scalpers and day traders miss critical tick movements
- Delayed reaction to market events (up to 5 seconds)
- Competing platforms offer tick-level granularity

**User Impact:**
- Traders make decisions on stale data
- Missed entry/exit opportunities
- Poor user experience compared to real-time platforms (Zerodha Kite, Upstox Pro, etc.)

### 1.2 Target Users

1. **Day Traders** - Require immediate price feedback
2. **Scalpers** - Need tick-level precision for quick trades
3. **Algorithmic Traders** - Automation requires real-time data
4. **Technical Analysts** - Accurate bar formation for pattern recognition

### 1.3 Success Criteria

- **Response Time:** Chart updates within 100ms of tick arrival (visual feedback)
- **Accuracy:** OHLC accuracy maintained by HTTP backfill override (every 5 seconds)
- **Smoothness:** No visual "jumps" or freak candles
- **Compatibility:** Works seamlessly with existing historical data
- **Performance:** CPU usage < 5% for 50 symbols
- **Reliability:** No memory leaks, stable 24/7 operation
- **Self-Correcting:** Missed/duplicate ticks automatically corrected by backfill

---

## 2. Current Architecture Analysis

### 2.1 Existing Data Flow

```
WebSocket Server
    ↓ (Tick arrives)
ProcessWebSocketData()
    ↓ (Parse JSON)
g_QuoteCache.SetAt()
    ↓ (Store snapshot)
[Wait for Timer/GetRecentInfo() call]
    ↓ (5-second interval)
AmiBroker reads from cache
    ↓
Chart renders
```

**Key Files:**
- `Plugin.cpp:2244-2373` - ProcessWebSocketData()
- `Plugin.cpp:1718-1817` - GetRecentInfo()
- `Plugin.cpp:600-880` - GetQuotesEx() (historical)

### 2.2 WebSocket Data Format

**Current Message Structure:**
```json
{
  "type": "market_data",
  "mode": 2,
  "topic": "RELIANCE.NSE",
  "data": {
    "symbol": "RELIANCE",
    "exchange": "NSE",
    "ltp": 1424.0,
    "change": 6.0,
    "change_percent": 0.42,
    "volume": 100000,
    "open": 1415.0,
    "high": 1432.5,
    "low": 1408.0,
    "close": 1418.0,
    "last_trade_quantity": 50,
    "avg_trade_price": 1419.35,
    "timestamp": "2025-05-28T10:30:45.123Z"
  }
}
```

**Critical Fields for Real-Time Candles:**
- `ltp` - Last traded price (for OHLC calculation)
- `last_trade_quantity` - Trade size (for volume aggregation)
- `timestamp` - Tick time (for bar boundary detection)

### 2.3 Current Limitations

| Component | Current Behavior | Limitation |
|-----------|------------------|------------|
| **ProcessWebSocketData()** | Overwrites cache snapshot | No tick history, no aggregation |
| **QuoteCache** | Single quote struct | No bar state tracking |
| **GetRecentInfo()** | Returns cached quote | Called every 5 seconds (timer-based) |
| **GetQuotesEx()** | HTTP API for history | No live bar updates |
| **Volume Calculation** | Uses cumulative volume from server | Not calculated from trades |

### 2.4 AmiBroker Plugin API Constraints

**Key Limitations:**
1. **GetQuotesEx() is pull-based** - AmiBroker calls periodically, plugin cannot push
2. **No streaming API** - Plugin must return data when requested
3. **Quotation array is pre-allocated** - Fixed size, plugin fills from 0 to nLastValid
4. **Timestamp normalization required** - Sub-period fields must be zeroed (Plugin.cpp:796-799)
5. **Mixed EOD/Intraday support** - Plugin must handle both daily and intraday bars

**Critical Discovery - WM_USER_STREAMING_UPDATE:**
After reviewing the AmiBroker Plugin API (Plugin.h:268-272), a **push-notification mechanism** was discovered:
```cpp
// The WM_USER_STREAMING_UPDATE MESSAGE
// is used to notify AmiBroker that new streaming data arrived
#define WM_USER_STREAMING_UPDATE (WM_USER + 13000)
```

**This changes the architecture significantly!** The plugin CAN notify AmiBroker when new data arrives:

1. **Tick arrives** → ProcessWebSocketData() → ProcessTick()
2. **Bar updated** → Send `WM_USER_STREAMING_UPDATE` message to AmiBroker main window
3. **AmiBroker responds** → Calls GetQuotesEx() to fetch updated bars
4. **Chart updates** → Real-time display

**Revised Architecture (Push-Pull Hybrid):**
- **Push:** Plugin notifies AmiBroker via Windows message when ticks arrive
- **Pull:** AmiBroker calls GetQuotesEx() to fetch the updated bar data
- **Result:** True real-time updates without polling delays!

---

## 3. Proposed Solution

### 3.1 High-Level Approach

**Tick-to-Bar Aggregation Engine:**

1. **Tick Ingestion** - Receive LTP + last_trade_quantity from WebSocket
2. **Bar Building** - Aggregate ticks into in-memory OHLC bars
3. **Bar Storage** - Maintain per-symbol bar arrays in memory
4. **Bar Delivery** - Return bars to AmiBroker on GetQuotesEx() calls
5. **Backfill Integration** - Merge historical + live bars seamlessly

### 3.2 Architecture Overview (REVISED with Push Notifications)

```
┌─────────────────────────────────────────────────────────────────┐
│                      WebSocket Server                            │
└────────────────────────┬────────────────────────────────────────┘
                         │ Tick Stream
                         ↓
┌─────────────────────────────────────────────────────────────────┐
│  ProcessWebSocketData() - Enhanced                               │
│  • Parse: ltp, last_trade_quantity, timestamp                    │
│  • Identify symbol + exchange                                    │
│  • Route to Bar Builder                                          │
└────────────────────────┬────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────────────────┐
│  Real-Time Bar Builder (NEW)                                     │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │ For each tick:                                           │   │
│  │  1. Determine bar period (1-min boundary)                │   │
│  │  2. If new period → Close current bar, create new bar    │   │
│  │  3. Update current bar OHLC:                             │   │
│  │     - First tick → Open                                  │   │
│  │     - All ticks → Check High/Low, update Close           │   │
│  │  4. Accumulate volume from last_trade_quantity           │   │
│  │  5. Store bar in SymbolBarCache                          │   │
│  │  6. Return TRUE (bar updated)                            │   │
│  └──────────────────────────────────────────────────────────┘   │
└────────────────────────┬────────────────────────────────────────┘
                         ↓
                    Bar Updated?
                         ↓ YES
┌─────────────────────────────────────────────────────────────────┐
│  **PUSH NOTIFICATION** (NEW - Critical Enhancement!)             │
│  PostMessage(g_hAmiBrokerWnd, WM_USER_STREAMING_UPDATE, 0, 0)   │
│  • Non-blocking Windows message                                  │
│  • Notifies AmiBroker that new data is available                 │
│  • AmiBroker responds by calling GetQuotesEx()                   │
└────────────────────────┬────────────────────────────────────────┘
                         ↓ (AmiBroker triggered)
┌─────────────────────────────────────────────────────────────────┐
│  SymbolBarCache (NEW)                                            │
│  • CMap<CString ticker, BarArray barHistory>                     │
│  • Stores up to 10,000 bars per symbol (rolling window)          │
│  • Thread-safe (CRITICAL_SECTION)                                │
│  • Merged with historical backfill on first load                 │
└────────────────────────┬────────────────────────────────────────┘
                         ↓ (AmiBroker pulls data)
┌─────────────────────────────────────────────────────────────────┐
│  GetQuotesEx() - Enhanced                                        │
│  • Called by AmiBroker when WM_USER_STREAMING_UPDATE received    │
│  • Return bars from SymbolBarCache                               │
│  • Merge with historical data (first call only)                  │
│  • Return nLastValid index                                       │
└────────────────────────┬────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────────────────┐
│                       AmiBroker Chart                            │
│              (Renders bars in REAL-TIME < 100ms)                 │
└─────────────────────────────────────────────────────────────────┘
```

**Key Improvement:** Using WM_USER_STREAMING_UPDATE eliminates polling delay and provides true event-driven real-time updates!

### 3.3 Simplified Architecture Philosophy

**The Brilliant Simplification:**

Instead of trying to make ticks perfect, this design embraces a **hybrid approach**:

```
TICKS = SPEED (immediate visual feedback, may have errors)
  +
HTTP BACKFILL = ACCURACY (corrects all errors every 5 seconds)
  =
REAL-TIME + ACCURATE (best of both worlds!)
```

**Benefits:**
1. ✅ **No complex tick handling** - No sorting, deduplication, or validation needed
2. ✅ **Self-correcting** - HTTP backfill automatically fixes any tick errors
3. ✅ **Simpler code** - Less complexity = fewer bugs
4. ✅ **More robust** - Works even if ticks are unreliable
5. ✅ **Faster development** - Focus on core functionality, not edge cases

**Trade-off:**
- Completed bars may show temporary incorrect OHLC for up to 5 seconds before HTTP corrects them
- This is acceptable because ticks provide immediate feedback and backfill provides accuracy

### 3.4 Key Design Decisions

| Decision | Rationale |
|----------|-----------|
| **Tick buffering in plugin** | AmiBroker API is pull-based; cannot push updates |
| **In-memory bar cache** | Fast access, no disk I/O latency |
| **1-minute granularity ONLY** | Focus on 1-minute bars first; daily bars later |
| **HTTP backfill overrides ticks** | Ensures OHLC accuracy; ticks provide speed, backfill provides accuracy |
| **Backfill every 5 seconds** | Corrects missed/duplicate ticks automatically |
| **No tick ordering/deduplication** | Backfill handles all edge cases; keep it simple |
| **No market timing restrictions** | Process ticks 24/7; don't hardcode market hours |
| **Real-time enabled by default** | Checkbox "Enable Real-Time Candles" (default: checked) |
| **Fallback to HTTP if no ticks** | If no ticks for 5 seconds, use HTTP API |
| **Rolling window (10K bars)** | Balance memory usage vs. history depth (~7 days @ 1-min) |
| **Thread-safe access** | WebSocket thread writes, AmiBroker thread reads |

---

## 4. Technical Design

### 4.1 New Data Structures

#### 4.1.1 BarBuilder (Per-Symbol State)

```cpp
struct BarBuilder {
    CString symbol;
    CString exchange;
    int periodicity;  // 60 for 1-minute

    // Current bar being built
    struct Quotation currentBar;
    BOOL bBarStarted;
    time_t barStartTime;

    // Tick accumulation
    float volumeAccumulator;  // Sum of last_trade_quantity
    int tickCount;

    // Bar storage
    CArray<struct Quotation, struct Quotation> bars;  // Up to 10,000 bars
    int maxBars;

    // Timestamps
    DWORD lastTickTime;
    DWORD lastUpdateTime;

    // State flags
    BOOL bBackfillMerged;  // Historical data merged?
    BOOL bFirstTickReceived;

    // Constructor
    BarBuilder() : periodicity(60), bBarStarted(FALSE), barStartTime(0),
                   volumeAccumulator(0.0f), tickCount(0), maxBars(10000),
                   lastTickTime(0), lastUpdateTime(0),
                   bBackfillMerged(FALSE), bFirstTickReceived(FALSE) {
        memset(&currentBar, 0, sizeof(struct Quotation));
    }
};
```

#### 4.1.2 Global Bar Cache

```cpp
// Global cache of bar builders (one per symbol)
static CMap<CString, LPCTSTR, BarBuilder*, BarBuilder*> g_BarBuilders;
static CRITICAL_SECTION g_BarBuilderCriticalSection;

// AmiBroker main window handle (for streaming notifications)
static HWND g_hAmiBrokerWnd = NULL;  // Obtained from GetStatus() calls
```

### 4.2 Core Functions

#### 4.2.1 ProcessTick() - Main Aggregation Logic (SIMPLIFIED)

```
Function: ProcessTick(symbol, exchange, ltp, lastTradeQty, timestamp)

Input:
  - symbol: "RELIANCE"
  - exchange: "NSE"
  - ltp: 1424.0
  - lastTradeQty: 50
  - timestamp: Unix timestamp (seconds)

Logic:
  1. Get or create BarBuilder for this symbol

  2. Determine bar boundary (1-minute intervals ONLY)
     - barPeriodStart = (timestamp / 60) * 60  // Floor to minute

  3. Check if new bar needed:
     IF barPeriodStart != currentBar.barStartTime:
         • Finalize current bar (if exists)
         • Append to bars array
         • Start new bar:
             - currentBar.Open = ltp
             - currentBar.High = ltp
             - currentBar.Low = ltp
             - currentBar.Close = ltp
             - currentBar.Volume = 0
             - currentBar.DateTime = barPeriodStart (normalized)
         • Reset volumeAccumulator = 0

  4. Update current bar:
     - currentBar.High = max(currentBar.High, ltp)
     - currentBar.Low = min(currentBar.Low, ltp)
     - currentBar.Close = ltp
     - volumeAccumulator += lastTradeQty
     - currentBar.Volume = volumeAccumulator
     - tickCount++

  5. Update lastTickTime

  6. Return TRUE

Notes:
  - All timestamps must be normalized (seconds=0, millisec=0) per Plugin.cpp:796-799
  - CRITICAL_SECTION must protect all bar array access
  - **NO tick ordering/deduplication** - Keep it simple!
  - **NO market hours checking** - Process all ticks 24/7
  - HTTP backfill will correct any errors automatically
```

#### 4.2.2 GetQuotesEx() Enhancement (SIMPLIFIED with Auto-Correction)

```
Function: GetQuotesEx(ticker, periodicity, lastValid, size, pQuotes)

Enhanced Logic:

1. **ALWAYS call HTTP backfill every 5 seconds** (configurable)
   - Check: (currentTime - lastBackfillTime) > 5000ms
   - IF TRUE: Call GetOpenAlgoHistory() for today's data
   - This ensures OHLC accuracy and corrects any tick errors

2. IF BarBuilder exists (real-time mode enabled):
   a. EnterCriticalSection(&g_BarBuilderCriticalSection)

   b. Merge HTTP backfill bars with live tick bars:
      • **HTTP bars OVERRIDE tick bars** (by timestamp matching)
      • This auto-corrects missed/duplicate/out-of-order ticks
      • Keeps most recent tick bar (in-progress) for live feel

   c. Copy merged bars to pQuotes array
      • Start from oldest bar (limit to 10,000)
      • Include currentBar (in-progress) as latest bar
      • Update nLastValid index

   d. LeaveCriticalSection(&g_BarBuilderCriticalSection)

3. ELSE (real-time mode disabled OR no ticks received):
   - Use pure HTTP backfill (existing behavior)
   - Fallback if no ticks for 5+ seconds

4. Return nLastValid + 1

Notes:
  - **HTTP backfill is the source of truth** for completed bars
  - Ticks provide immediate visual feedback (speed)
  - HTTP backfill provides accuracy (corrects errors)
  - No complex tick handling needed - simple and robust!
  - Only 1-minute periodicity supported initially
```

#### 4.2.3 ProcessWebSocketData() Enhancement

```
Current: Lines 2244-2373
Change:

After parsing JSON at line 2304:
  1. Extract NEW fields:
     - last_trade_quantity (from "last_trade_quantity": 50)
     - timestamp (from "timestamp": "2025-05-28T10:30:45.123Z")

  2. Convert ISO timestamp to Unix seconds:
     - Parse timestamp string
     - Convert to time_t

  3. Call ProcessTick():
     BOOL barUpdated = ProcessTick(symbol, exchange, ltp, last_trade_quantity, timestamp)

  4. STILL update g_QuoteCache (for GetRecentInfo() compatibility)

  5. **CRITICAL - Notify AmiBroker of update:**
     IF barUpdated:
         // Send streaming update notification
         PostMessage(g_hAmiBrokerWnd, WM_USER_STREAMING_UPDATE, 0, 0)

     Notes:
     - g_hAmiBrokerWnd obtained from GetStatus() calls (stored globally)
     - PostMessage is non-blocking (doesn't wait for AmiBroker response)
     - AmiBroker will call GetQuotesEx() when ready
     - This provides TRUE real-time updates without polling
```

### 4.3 Timestamp Handling

**Critical Requirements:**
- **1-minute bars must have normalized timestamps** (Plugin.cpp:796-799 lesson)
- **Avoid "freak candles"** - All bars in same minute must have identical time fields

**Normalization Rules:**
```cpp
// For 1-minute bars (periodicity = 60)
barStartTime = (timestamp / 60) * 60;  // Floor to minute boundary

pQuotes[i].DateTime.PackDate.Second = 0;
pQuotes[i].DateTime.PackDate.MilliSec = 0;
pQuotes[i].DateTime.PackDate.MicroSec = 0;
```

**Example:**
```
Tick 1: 2025-05-28T10:30:15.123Z → Bar: 2025-05-28 10:30:00
Tick 2: 2025-05-28T10:30:45.678Z → Same bar: 2025-05-28 10:30:00
Tick 3: 2025-05-28T10:31:01.234Z → New bar: 2025-05-28 10:31:00
```

### 4.4 Volume Calculation

**Problem:** Server provides cumulative volume, which may not reflect real-time trades.

**Solution:** Calculate volume from trade quantities.

```cpp
// In ProcessTick()
volumeAccumulator += lastTradeQty;
currentBar.Volume = volumeAccumulator;

// On new bar:
volumeAccumulator = 0;  // Reset for new bar
```

**Edge Case:** First tick of new trading session
- Server may send volume=0 on first tick
- Plugin should handle gracefully (don't reset if non-zero)

### 4.5 Memory Management

**Bar Storage Limits:**
- **Max bars per symbol:** 10,000 (configurable)
- **Memory per bar:** ~80 bytes (struct Quotation)
- **Memory per symbol:** ~800 KB
- **For 100 symbols:** ~80 MB (acceptable)

**Rolling Window Strategy:**
```cpp
// When bars array reaches maxBars:
IF bars.GetCount() >= maxBars:
    // Remove oldest 10% to make room
    int removeCount = maxBars / 10;
    bars.RemoveAt(0, removeCount);
```

### 4.6 Thread Safety

**Concurrency Model:**
- **Writer Thread:** ProcessWebSocketData() (socket receive thread)
- **Reader Thread:** GetQuotesEx() (AmiBroker main thread)
- **Synchronization:** CRITICAL_SECTION per operation

```cpp
// In ProcessTick()
EnterCriticalSection(&g_BarBuilderCriticalSection);
// ... modify bars array ...
LeaveCriticalSection(&g_BarBuilderCriticalSection);

// In GetQuotesEx()
EnterCriticalSection(&g_BarBuilderCriticalSection);
// ... read bars array ...
LeaveCriticalSection(&g_BarBuilderCriticalSection);
```

**Lock Duration:** Keep critical sections < 1ms to avoid blocking.

---

## 5. Implementation Phases

### Phase 1: Foundation (Week 1)

**Objective:** Implement core data structures and tick processing.

**Tasks:**
1. ✅ Create BarBuilder struct
2. ✅ Add g_BarBuilders global cache
3. ✅ Initialize g_BarBuilderCriticalSection in Init()
4. ✅ Implement ProcessTick() function
5. ✅ Unit test tick-to-bar logic (offline)

**Deliverables:**
- BarBuilder.h (new file)
- ProcessTick() implementation
- Unit test suite (test_tick_aggregation.cpp)

**Success Criteria:**
- 100 ticks → Correct OHLC for 1 bar
- 500 ticks → Multiple bars with correct timestamps
- Edge cases pass (midnight rollover, gaps, etc.)

---

### Phase 2: WebSocket Integration (Week 2)

**Objective:** Connect tick processor to WebSocket data stream.

**Tasks:**
1. ✅ Enhance ProcessWebSocketData() to parse new fields
2. ✅ Add timestamp conversion (ISO 8601 → Unix time)
3. ✅ Call ProcessTick() for each incoming tick
4. ✅ Maintain backward compatibility with GetRecentInfo()
5. ✅ Add debug logging (optional, via preprocessor flag)

**Deliverables:**
- Modified Plugin.cpp:2244-2373
- ISO timestamp parser
- Integration test with mock WebSocket

**Success Criteria:**
- Live ticks processed without errors
- Bars appear in g_BarBuilders cache
- No memory leaks after 1 hour of ticking

---

### Phase 3: AmiBroker Integration (Week 3)

**Objective:** Serve real-time bars to AmiBroker charts.

**Tasks:**
1. ✅ Enhance GetQuotesEx() to return bars from cache
2. ✅ Implement backfill merge logic (historical + live)
3. ✅ Handle array size limits and overflow
4. ✅ Ensure timestamp normalization compliance
5. ✅ Test with real AmiBroker instance

**Deliverables:**
- Modified GetQuotesEx() function
- Backfill merge algorithm
- AmiBroker integration test

**Success Criteria:**
- Charts render smoothly with live updates
- No "freak candles" or timestamp issues
- Backfill merges seamlessly with live bars

---

### Phase 4: Polish & Optimization (Week 4)

**Objective:** Optimize performance and handle edge cases.

**Tasks:**
1. ✅ Optimize critical section lock duration
2. ✅ Add rolling window for bar storage
3. ✅ Handle edge cases:
   - Trading session start/end
   - Market holidays
   - Connection drops mid-bar
   - Clock skew between server/client
4. ✅ Performance profiling (CPU, memory)
5. ✅ Stress test with 100 symbols

**Deliverables:**
- Performance report
- Edge case test suite
- Optimization recommendations

**Success Criteria:**
- CPU usage < 5% for 50 symbols
- No memory growth over 24 hours
- Handles 1000 ticks/second without loss

---

### Phase 5: Documentation & Release (Week 5)

**Objective:** Prepare for production deployment.

**Tasks:**
1. ✅ Update TECHNICAL_DOCUMENTATION.md
2. ✅ Update README.md with real-time feature
3. ✅ Create user guide for enabling real-time mode
4. ✅ Add configuration options (enable/disable, bar limit)
5. ✅ Release notes and version bump (v1.1.0)

**Deliverables:**
- Updated documentation
- User guide
- Release build (OpenAlgo.dll v1.1.0)

**Success Criteria:**
- Documentation complete and accurate
- Beta testers can install and use feature
- No critical bugs in beta testing

---

## 6. Risk Assessment

### 6.1 Technical Risks

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| **AmiBroker API limitations** | HIGH | HIGH | Prototype early to validate API capabilities |
| **Memory leaks** | MEDIUM | HIGH | Rigorous leak testing with Valgrind/Dr. Memory |
| **Thread deadlocks** | MEDIUM | HIGH | Keep critical sections minimal; use timeout locks |
| **Timestamp sync issues** | HIGH | MEDIUM | Use server timestamp, not client clock |
| **Volume calculation errors** | MEDIUM | MEDIUM | Cross-validate with server volume initially |
| **Performance degradation** | LOW | MEDIUM | Profile early; optimize hotspots |

### 6.2 Data Quality Risks

| Risk | Impact | Mitigation |
|------|--------|------------|
| **Missing ticks** | Temporary incorrect bar | ✅ **HTTP backfill overrides every 5 seconds** (auto-corrects) |
| **Out-of-order ticks** | Temporary incorrect OHLC | ✅ **HTTP backfill overrides** (no sorting needed) |
| **Duplicate ticks** | Temporary volume inflation | ✅ **HTTP backfill overrides** (no deduplication needed) |
| **Clock skew** | Wrong bar assignment | ✅ Use server timestamp from tick data |
| **No ticks for 5+ seconds** | Stale data | ✅ **Fallback to HTTP API** (configurable interval) |

**Key Insight:** Ticks provide **speed** (immediate visual feedback), HTTP backfill provides **accuracy** (correct OHLC). This hybrid approach is simple and self-correcting!

### 6.3 User Experience Risks

| Risk | Impact | Mitigation |
|------|--------|------------|
| **Choppy charts** | Poor UX | Ensure AmiBroker refresh ≤ 500ms |
| **Delayed updates** | Perceived lag | Optimize ProcessTick() to < 1ms |
| **Backward incompatibility** | Existing users broken | Maintain fallback to interval mode |

---

## 7. Performance Considerations

### 7.1 Bottleneck Analysis

**Critical Path:**
```
Tick arrives (0ms)
  → ProcessWebSocketData() parse JSON (0.5ms)
  → ProcessTick() aggregate (0.2ms)
  → EnterCriticalSection (0.01ms)
  → Update bar (0.05ms)
  → LeaveCriticalSection (0.01ms)
Total: ~0.77ms per tick
```

**Target:** Process 1000 ticks/second = 1ms per tick budget
**Margin:** 0.23ms (23% headroom) ✅

### 7.2 Optimization Strategies

1. **JSON Parsing:**
   - Current: String search with Find()
   - Optimized: Use rapidjson or simdjson library
   - Gain: 70% faster parsing (~0.15ms)

2. **Critical Section:**
   - Use try-lock with timeout to avoid blocking
   - Consider lock-free data structures (if expertise available)

3. **Memory Allocation:**
   - Pre-allocate bar array to maxBars
   - Avoid dynamic allocation in hot path

4. **Timestamp Normalization:**
   - Pre-compute bar boundaries (LUT for today's minutes)
   - Avoid repeated division operations

### 7.3 Scalability

**Maximum Symbols:**
- **Memory limit:** 4 GB / 0.8 MB per symbol = ~5,000 symbols (theoretical)
- **Practical limit:** 100-200 symbols (CPU bound)
- **Recommendation:** Limit to 50 symbols in real-time mode

**Tick Rate:**
- **Average:** 1-5 ticks/second per symbol (low volatility)
- **Peak:** 50-100 ticks/second per symbol (high volatility, e.g., expiry day)
- **Plugin capacity:** 1000 ticks/second total (sufficient for 20 active symbols @ 50 tps)

---

## 8. Testing Strategy

### 8.1 Unit Tests

**Test Cases:**

1. **Single Tick:**
   - Input: 1 tick @ 10:00:00
   - Expected: 1 bar with Open=High=Low=Close=LTP, Volume=qty

2. **Multiple Ticks, Same Bar:**
   - Input: 10 ticks @ 10:00:xx (same minute)
   - Expected: 1 bar with correct OHLC, Volume=sum(qty)

3. **Bar Boundary:**
   - Input: Ticks @ 10:00:59, 10:01:01
   - Expected: 2 bars, correct timestamps (10:00:00, 10:01:00)

4. **High/Low Tracking:**
   - Input: LTP sequence [100, 105, 98, 102]
   - Expected: High=105, Low=98, Close=102

5. **Volume Accumulation:**
   - Input: Qtys [10, 20, 30, 40]
   - Expected: Volume=100

6. **Timestamp Normalization:**
   - Input: Ticks with varying seconds/millis
   - Expected: All bars normalized to :00:00

### 8.2 Integration Tests

1. **WebSocket → Bars:**
   - Mock WebSocket with recorded tick stream
   - Verify bars appear in g_BarBuilders

2. **Bars → AmiBroker:**
   - Call GetQuotesEx() repeatedly
   - Verify chart renders without errors

3. **Backfill Merge:**
   - Load historical data via HTTP
   - Start tick stream
   - Verify seamless transition

4. **Reconnection:**
   - Disconnect WebSocket mid-bar
   - Reconnect after 1 minute
   - Verify no duplicate bars

### 8.3 Stress Tests

1. **High Tick Rate:**
   - Inject 1000 ticks/second
   - Verify no ticks dropped, CPU < 10%

2. **Many Symbols:**
   - 100 symbols, 5 ticks/sec each
   - Run for 8 hours (trading session)
   - Verify no memory leaks, stable performance

3. **Long Duration:**
   - 1 symbol, continuous ticking
   - Run for 7 days (rolling window test)
   - Verify bar array doesn't overflow

### 8.4 Edge Case Tests

1. **Missing Ticks:**
   - Disconnect WebSocket for 30 seconds during active trading
   - Verify: Ticks stopped, HTTP backfill continues every 5 seconds
   - Verify: Bars are accurate (from HTTP), no gaps

2. **Out-of-Order Ticks:**
   - Inject ticks in random order (if server sends them)
   - Verify: HTTP backfill overrides and corrects bars within 5 seconds

3. **Duplicate Ticks:**
   - Send same tick multiple times
   - Verify: Volume may inflate temporarily, but HTTP backfill corrects it

4. **No Ticks for Extended Period:**
   - No ticks for 1 hour (low liquidity symbol)
   - Verify: Fallback to HTTP API, bars still update every 5 seconds

5. **Pre-market/Post-market Ticks:**
   - Ticks arrive outside market hours (e.g., 8:00 AM)
   - Verify: Processed normally (no hardcoded time restrictions)

6. **Midnight Rollover:**
   - Ticks spanning 23:59 to 00:00
   - Verify: Bars assigned to correct date

---

## 9. Success Metrics

### 9.1 Quantitative Metrics

| Metric | Target | Measurement Method |
|--------|--------|--------------------|
| **Tick-to-Display Latency** | < 100ms | Timestamp diff (tick arrival → chart update) |
| **Tick Loss Rate** | < 0.01% | Compare received vs. server sent count |
| **CPU Usage** | < 5% | Task Manager, 50 symbols, 8 hours |
| **Memory Usage** | < 100 MB | Task Manager, 50 symbols, stable over time |
| **Bar Accuracy** | 100% | Compare with broker's historical data |
| **Uptime** | > 99.9% | No crashes in 30-day beta test |

### 9.2 Qualitative Metrics

- **User Satisfaction:** Survey beta testers (NPS > 8/10)
- **Chart Smoothness:** No "choppy" or "jumpy" visual artifacts
- **Ease of Use:** No additional configuration required
- **Reliability:** No emergency patches in first 30 days

### 9.3 Rollout Criteria

**Go/No-Go Checklist:**
- ✅ All unit tests pass
- ✅ All integration tests pass
- ✅ Stress tests show stable performance
- ✅ Beta testers approve (>80% positive feedback)
- ✅ No critical bugs in issue tracker
- ✅ Documentation complete
- ✅ Rollback plan tested

---

## 10. Open Questions

### 10.1 Technical Questions

1. **Q: Does WebSocket server guarantee ordered delivery?**
   - **Impact:** If ticks arrive out-of-order, OHLC may be incorrect
   - **Resolution:** Test with real server; implement sort-on-arrival if needed

2. **Q: What is the maximum WebSocket message rate?**
   - **Impact:** Plugin may not keep up at peak
   - **Resolution:** Benchmark with server team

3. **Q: How does server handle reconnection mid-bar?**
   - **Impact:** Plugin may miss ticks or receive duplicates
   - **Resolution:** Request tick sequence numbers from server

4. **Q: Can we get tick ID for deduplication?**
   - **Impact:** Without tick ID, duplicates may inflate volume
   - **Resolution:** Request server to add unique tick ID field

5. **Q: Does AmiBroker cache bars or re-request every refresh?**
   - **Impact:** If re-requests, may cause performance issues
   - **Resolution:** Test AmiBroker behavior; optimize if needed

### 10.2 Product Questions

1. **Q: Should real-time mode be opt-in or default?**
   - ✅ **ANSWER:** Default ON (checkbox checked by default)

2. **Q: Should we support daily bars in real-time?**
   - ✅ **ANSWER:** No, focus on 1-minute bars ONLY initially

3. **Q: What happens during pre-market/post-market?**
   - ✅ **ANSWER:** Process all ticks 24/7, no hardcoded market timings

4. **Q: Should we expose real-time toggle in config UI?**
   - ✅ **ANSWER:** Yes, add checkbox "Enable Real-Time Candles" (default: checked)

5. **Q: How to handle symbols with low liquidity (< 1 tick/min)?**
   - ✅ **ANSWER:** Fallback to HTTP API every 5 seconds if no ticks

6. **Q: How to handle missed/duplicate/out-of-order ticks?**
   - ✅ **ANSWER:** Don't worry! HTTP backfill overrides every 5 seconds (auto-corrects)

7. **Q: What if ticks are lost during network issues?**
   - ✅ **ANSWER:** Only affects that 1-minute candle temporarily. HTTP backfill replaces it within 5 seconds.

---

## Appendix A: Comparison with Competitors

### Zerodha Kite (Web)
- **Real-time:** Yes, WebSocket
- **Granularity:** Tick-by-tick
- **Update Rate:** < 100ms
- **Volume:** Calculated from trades

### Upstox Pro (Desktop)
- **Real-time:** Yes, WebSocket
- **Granularity:** Tick-by-tick
- **Update Rate:** < 200ms
- **Volume:** Server-provided

### TradingView
- **Real-time:** Yes (paid tier)
- **Granularity:** 1-second bars
- **Update Rate:** 1-second snapshots
- **Volume:** Included in snapshot

### OpenAlgoPlugin (Current)
- **Real-time:** No
- **Granularity:** 5-second snapshots
- **Update Rate:** 5 seconds
- **Volume:** Server cumulative

### OpenAlgoPlugin (Proposed)
- **Real-time:** Yes
- **Granularity:** Tick-by-tick
- **Update Rate:** < 100ms
- **Volume:** Calculated from last_trade_quantity

---

## Appendix B: Code File Modifications

### Files to Modify

1. **Plugin.cpp** (Major changes)
   - Lines 74-90: Add BarBuilder struct
   - Lines 2244-2373: Enhance ProcessWebSocketData()
   - Lines 600-880: Enhance GetQuotesEx()
   - New: ProcessTick() function (~200 lines)

2. **Plugin.h** (Minor changes)
   - Add BarBuilder forward declaration
   - Add ProcessTick() prototype

3. **OpenAlgoGlobals.h** (Minor changes)
   - Add g_BarBuilders global
   - Add g_BarBuilderCriticalSection

4. **OpenAlgoConfigDlg.cpp** (Optional)
   - Add "Enable Real-Time Candles" checkbox
   - Add registry key for setting

### Estimated Code Changes
- **Lines added:** ~500
- **Lines modified:** ~150
- **Lines deleted:** ~20
- **Net change:** +630 lines (~25% growth)

---

## Appendix C: Configuration Options

### New Settings (Registry)

```
HKEY_CURRENT_USER\Software\OpenAlgo\

[New]
EnableRealTimeCandles    REG_DWORD    1 (default: 1 = ON)
MaxBarsPerSymbol         REG_DWORD    10000 (default: 10000)
TickBufferSize           REG_DWORD    2048 (default: 2048 bytes)
RealTimeDebugLog         REG_DWORD    0 (default: 0 = OFF)
```

### UI Controls (OpenAlgoConfigDlg)

```
[New Checkbox]
☑ Enable Real-Time Candle Building
  Aggregate WebSocket ticks into live bars (recommended for day trading)

[New Slider]
Max Bars Per Symbol: [10000]
  (1000 - 50000 bars, ~7 days @ 1-min)
```

---

## Appendix D: Migration Path

### For Existing Users

**Scenario 1: User upgrades to v1.1.0**
1. Plugin detects existing installation
2. Disables real-time mode by default (EnableRealTimeCandles=0)
3. Shows notification: "New real-time mode available. Enable in settings?"
4. User can opt-in via configuration dialog

**Scenario 2: New user installs v1.1.0**
1. Real-time mode enabled by default
2. Starts receiving tick data immediately
3. No action required

**Rollback:**
- If issues arise, user can:
  1. Uncheck "Enable Real-Time Candles" in config
  2. Restart AmiBroker
  3. Plugin reverts to 5-second snapshot mode

---

## Conclusion

This PRD demonstrates the **technical feasibility** of implementing real-time candle building in OpenAlgoPlugin. The proposed solution:

✅ **Achievable** - No fundamental blockers
✅ **Performant** - Meets latency and scalability targets
✅ **Compatible** - Works with AmiBroker's API (push-pull hybrid model)
✅ **Maintainable** - Clean architecture, well-tested
✅ **User-Friendly** - Minimal configuration required
✅ **Event-Driven** - Uses WM_USER_STREAMING_UPDATE for true real-time updates

### Critical Discovery

During the feasibility study, the **WM_USER_STREAMING_UPDATE** message mechanism was discovered in the AmiBroker Plugin API (Plugin.h:268-272). This enables:

- **Event-driven architecture** instead of polling
- **Sub-100ms latency** from tick arrival to chart update
- **Efficient resource usage** (no wasted GetQuotesEx() calls)
- **True real-time experience** competitive with professional trading platforms

This discovery significantly improves the feasibility and performance outlook of the project.

### Recommendation

**Proceed with Phase 1** implementation to validate:
1. Tick-to-bar aggregation logic
2. WM_USER_STREAMING_UPDATE notification mechanism
3. Integration with BarBuilder data structures

Success in Phase 1 will de-risk the project and provide confidence for full-scale development.

### Risk Mitigation

The main risk is **thread safety** between WebSocket receive thread and AmiBroker UI thread. Mitigation:
- Use CRITICAL_SECTION for all shared data access
- Keep lock duration < 1ms
- Test thoroughly under high tick load

**Next Steps:**
1. Review and approve this PRD
2. Allocate development resources (1 developer, 5 weeks)
3. Begin Phase 1: Foundation (Week 1)
4. Weekly check-ins to review progress
5. Beta release after Phase 3 completion

---

**Document Status:** Ready for Review (Updated with API Discovery)
**Approvers:** Product Owner, Lead Developer, QA Lead
**Estimated Effort:** 5 weeks (1 developer)
**Target Release:** v1.1.0 (Q1 2026)
**Confidence Level:** HIGH (API supports push notifications)

---

## Executive Summary for Stakeholders

### What We're Building
Converting OpenAlgoPlugin from 5-second refresh intervals to **true real-time** tick-by-tick chart updates.

### Key Innovation
**Hybrid Architecture:** Ticks provide speed (<100ms updates), HTTP backfill provides accuracy (every 5 seconds).

### Why This Approach Works
1. **Simple:** No complex tick handling - HTTP auto-corrects everything
2. **Fast:** Sub-100ms chart updates (competitive with professional platforms)
3. **Robust:** Self-correcting design handles all edge cases
4. **Achievable:** 5-week timeline, leverages existing AmiBroker API

### Requirements Summary
✅ Focus on 1-minute bars only (daily bars later)
✅ HTTP backfill overrides tick bars every 5 seconds
✅ No hardcoded market timings (process 24/7)
✅ Real-time enabled by default (checkbox provided)
✅ Fallback to HTTP if no ticks for 5+ seconds
✅ No need to handle out-of-order/duplicate ticks (HTTP corrects them)

### Success Metrics
- **Latency:** <100ms tick-to-chart update
- **Accuracy:** 100% (via HTTP backfill override)
- **Performance:** <5% CPU for 50 symbols
- **Reliability:** Self-correcting, no data loss

**Recommendation: PROCEED with implementation - HIGH confidence**
