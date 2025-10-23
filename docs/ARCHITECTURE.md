# OpenAlgo AmiBroker Plugin - Architecture Documentation

## System Architecture Overview

The OpenAlgo AmiBroker Plugin follows a multi-layered architecture designed for reliability, performance, and maintainability. This document provides an in-depth look at the architectural design, component interactions, and implementation details.

## High-Level Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                     AmiBroker Application                     │
│                                                               │
│  ┌─────────────┐  ┌──────────────┐  ┌──────────────┐       │
│  │   Charts    │  │ Real-time    │  │   Database   │       │
│  │   Engine    │  │ Quote Window │  │   Manager    │       │
│  └──────┬──────┘  └──────┬───────┘  └──────┬───────┘       │
│         │                 │                  │                │
│         └─────────────────┼──────────────────┘                │
│                           │                                   │
│                    Plugin Interface                           │
│                           │                                   │
└───────────────────────────┼───────────────────────────────────┘
                            │
                 ┌──────────▼──────────┐
                 │   OpenAlgo Plugin   │
                 │    (OpenAlgo.dll)   │
                 └──────────┬──────────┘
                            │
         ┌──────────────────┼──────────────────┐
         │                  │                  │
    ┌────▼────┐      ┌─────▼──────┐    ┌─────▼─────┐
    │  HTTP   │      │  WebSocket │    │   Cache   │
    │ Client  │      │   Client   │    │  Manager  │
    └────┬────┘      └─────┬──────┘    └───────────┘
         │                  │
         └──────────┬───────┘
                    │
             ┌──────▼──────┐
             │  OpenAlgo   │
             │   Server    │
             └──────┬──────┘
                    │
          ┌─────────┼─────────┐
          │         │         │
    ┌─────▼───┐ ┌──▼───┐ ┌──▼────┐
    │ Zerodha │ │Angel │ │ Dhan  │
    └─────────┘ └──────┘ └───────┘
```

## Component Architecture

### 1. Plugin Core (Plugin.cpp)

The plugin core is the heart of the system, implementing the AmiBroker Plugin API and coordinating all other components.

#### Key Responsibilities:
- **API Implementation**: Implements all required AmiBroker plugin functions
- **Data Management**: Handles quote and historical data retrieval
- **Connection Management**: Manages HTTP and WebSocket connections
- **State Management**: Tracks plugin state and connection status
- **Timer Management**: Handles refresh and retry timers

#### Critical Functions:

```cpp
// Plugin identification and initialization
GetPluginInfo()     // Provides plugin metadata
Init()              // Initializes plugin components
Release()           // Cleanup and resource deallocation

// Data retrieval functions
GetQuotesEx()       // Historical data retrieval
GetRecentInfo()     // Real-time quote retrieval

// Status and configuration
GetStatus()         // Connection status reporting
Configure()         // Opens configuration dialog
Notify()            // Handles AmiBroker notifications
```

### 2. Configuration Management (OpenAlgoConfigDlg.cpp)

Provides user interface for plugin configuration and connection testing.

#### Components:
- **Configuration Dialog**: User-friendly settings interface
- **Connection Tester**: HTTP API connection validation
- **WebSocket Tester**: WebSocket connection and subscription testing
- **Settings Persistence**: Registry-based configuration storage

#### Configuration Flow:
```
User Input → Validation → Registry Storage → Plugin Reload
```

### 3. Network Layer

#### HTTP Client Architecture
```cpp
CInternetSession → CHttpConnection → CHttpFile → JSON Parser
```

**Features:**
- Connection pooling
- Timeout management
- Error handling
- JSON request/response handling

#### WebSocket Client Architecture
```cpp
WinSock2 Socket → WebSocket Handshake → Frame Encoder/Decoder → Message Handler
```

**Features:**
- Asynchronous I/O
- Auto-reconnection
- Ping/Pong keep-alive
- Binary and text frame support

### 4. Data Management

#### Quote Cache System
```cpp
CMap<CString, QuoteCache> g_QuoteCache
```

**Cache Entry Structure:**
```cpp
struct QuoteCache {
    CString symbol;
    CString exchange;
    float ltp, open, high, low, close;
    float volume, oi;
    DWORD lastUpdate;  // Timestamp for TTL
}
```

**Cache Strategy:**
- TTL: 5 seconds for real-time quotes
- LRU eviction when needed
- Thread-safe access via critical sections

#### Historical Data Management

**Backfill Strategy:**
```
Initial Load:
  - 1-minute data: Last 30 days
  - Daily data: Last 1 year

Refresh:
  - Fetch today's data only
  - Merge with existing data
  - Handle duplicates intelligently
  - Sort chronologically (NEW in v1.0.0)
```

**Duplicate Detection Algorithm:**
```cpp
if (abs(timestamp1 - timestamp2) < 60) {
    // Timestamps within 60 seconds = duplicate
    // Update existing bar instead of adding new
}
```

**Timestamp Normalization (v1.0.0):**
```cpp
// For 1-minute bars, normalize sub-minute fields
if (nPeriodicity == 60) {
    pQuotes[i].DateTime.PackDate.Second = 0;
    pQuotes[i].DateTime.PackDate.MilliSec = 0;
    pQuotes[i].DateTime.PackDate.MicroSec = 0;
}
// Prevents freak candles during live updates
```

**Chronological Sorting (v1.0.0):**
```cpp
// After merging data, sort by timestamp
if (quoteIndex > 0) {
    qsort(pQuotes, quoteIndex, sizeof(struct Quotation), CompareQuotations);
}
// Ensures proper order for Quote Editor display
```

## Data Flow Architecture

### Real-time Quote Flow

```
1. AmiBroker Request
   └─> GetRecentInfo(symbol)
   
2. Subscription Check
   └─> Is symbol subscribed?
       ├─> No: Subscribe via WebSocket
       └─> Yes: Continue
       
3. Data Retrieval
   └─> Check cache
       ├─> Cache hit (< 5 sec): Return cached
       └─> Cache miss: Check WebSocket buffer
           ├─> Data available: Update cache
           └─> No data: Fallback to HTTP API
           
4. Return to AmiBroker
   └─> RecentInfo structure
```

### Historical Data Flow

```
1. AmiBroker Request
   └─> GetQuotesEx(symbol, periodicity, range)
   
2. Range Calculation
   └─> Has existing data?
       ├─> Yes: Fetch today only
       └─> No: Fetch full range
       
3. API Request
   └─> POST /api/v1/history
   
4. Data Processing
   └─> Parse JSON response
   └─> Convert timestamps
   └─> Handle duplicates
   └─> Merge with existing
   
5. Return to AmiBroker
   └─> Quotation array
```

## Threading Model

### Main Thread
- Plugin API calls
- UI interactions
- Configuration dialog

### Timer Callbacks
- Connection retry (TIMER_INIT)
- Data refresh (TIMER_REFRESH)
- WebSocket ping

### WebSocket Processing
- Non-blocking I/O
- Select-based polling
- Critical section protection

## State Management

### Connection State Machine

```
         ┌──────────┐
         │   WAIT   │◄──────────┐
         └────┬─────┘           │
              │                 │
         Connection         Retry Timer
         Successful              │
              │                 │
         ┌────▼─────┐      ┌────┴──────┐
         │CONNECTED │      │DISCONNECTED│
         └────┬─────┘      └───────────┘
              │                 ▲
              │                 │
         Connection        Connection
           Lost              Failed
              │                 │
              └─────────────────┘
```

### Status Codes
```cpp
STATUS_WAIT         // 0x10000000 - Yellow LED
STATUS_CONNECTED    // 0x00000000 - Green LED
STATUS_DISCONNECTED // 0x20000000 - Red LED
STATUS_SHUTDOWN     // 0x30000000 - Purple LED
```

## Memory Management

### Allocation Strategy
- **Static Allocation**: Plugin info structures
- **Dynamic Allocation**: Quote arrays, WebSocket buffers
- **Smart Pointers**: Not used (MFC/Win32 style)
- **Manual Management**: Explicit new/delete pairs

### Resource Cleanup
```cpp
Release() {
    CleanupWebSocket()
    g_QuoteCache.RemoveAll()
    DeleteCriticalSection()
    WSACleanup()
}
```

## Error Handling Architecture

### Error Propagation
```
Network Error → Exception → Error Handler → Status Update → User Notification
```

### Error Recovery Strategies

1. **Connection Errors**
   - Exponential backoff (fixed 15 sec currently)
   - Maximum retry count (8 attempts)
   - Manual reconnection option

2. **Data Errors**
   - Validation before processing
   - Fallback to cached data
   - Silent recovery when possible

3. **WebSocket Errors**
   - Auto-reconnection
   - Fallback to HTTP API
   - Subscription restoration

## Security Architecture

### Authentication Flow
```
1. API Key Storage (Registry)
   └─> Encrypted: No (TODO)
   
2. Authentication Request
   └─> HTTPS/WSS preferred
   └─> API key in JSON body
   
3. Session Management
   └─> WebSocket: Persistent auth
   └─> HTTP: Per-request auth
```

### Input Validation
- Symbol format validation
- Exchange validation
- Buffer overflow protection
- JSON injection prevention

## Performance Optimizations

### Network Optimizations
1. **Connection Reuse**: Keep-alive connections
2. **Compression**: Not implemented (TODO)
3. **Batching**: Multiple symbol subscriptions
4. **Caching**: 5-second TTL for quotes

### Data Processing Optimizations
1. **Lazy Loading**: Data fetched on demand
2. **Incremental Updates**: Only fetch new data
3. **Duplicate Prevention**: Smart merge algorithm
4. **Buffer Management**: Pre-allocated buffers

### AmiBroker Integration Optimizations
1. **Minimal Blocking**: Non-blocking WebSocket I/O
2. **Quick Returns**: Cache-first strategy
3. **Batch Processing**: Handle multiple symbols efficiently
4. **Status Caching**: Avoid redundant status checks

## Scalability Considerations

### Current Limitations
- Single WebSocket connection
- Sequential HTTP requests
- Fixed buffer sizes
- Limited to 2 intervals (1m, D)

### Future Scalability Improvements
1. **Connection Pooling**: Multiple WebSocket connections
2. **Parallel Processing**: Multi-threaded data processing
3. **Dynamic Buffers**: Adaptive buffer sizing
4. **Protocol Optimization**: Binary protocol support

## Platform Integration

### AmiBroker API Compliance
- **API Version**: 5.30.0+
- **Structure Packing**: Default
- **Calling Convention**: WINAPI
- **Character Set**: Unicode (UTF-16)

### Windows Integration
- **Registry**: HKCU\Software\OpenAlgo
- **Networking**: WinSock2 + WinInet
- **UI Framework**: MFC
- **Threading**: Windows threads + timers

## Deployment Architecture

### DLL Structure
```
OpenAlgo.dll
├── Code Sections
│   ├── .text (executable code)
│   ├── .rdata (read-only data)
│   └── .data (initialized data)
├── Import Table
│   ├── MFC libraries
│   ├── WinSock2
│   └── WinInet
└── Export Table
    └── Plugin API functions
```

### Dependencies
```
OpenAlgo.dll
├── MFC140.dll
├── VCRUNTIME140.dll
├── WS2_32.dll
├── WININET.dll
└── KERNEL32.dll
```

## Monitoring and Diagnostics

### Status Reporting
1. **Visual Indicators**: LED color in status bar
2. **Status Messages**: Detailed error descriptions
3. **Right-Click Menu**: Manual control options
4. **Test Functions**: Connection validation

### Debug Points
```cpp
// Key debug locations
TestOpenAlgoConnection()    // Connection testing
ProcessWebSocketData()       // WebSocket data flow
GetOpenAlgoHistory()        // Historical data retrieval
GetOpenAlgoQuote()          // Quote retrieval
```

## Extensibility Architecture

### Plugin Points
1. **New Exchanges**: Add exchange mapping
2. **New Intervals**: Extend GetIntervalString()
3. **New Data Types**: Extend quote structure
4. **New Brokers**: Server-side only

### API Versioning
- Current Version: 1.0
- Endpoint: /api/v1/*
- Backward Compatibility: Maintained

## Best Practices Implementation

### Code Organization
- **Separation of Concerns**: Clear component boundaries
- **Single Responsibility**: Each function has one job
- **DRY Principle**: Reusable helper functions
- **Error Handling**: Consistent try-catch patterns

### Resource Management
- **RAII**: Not fully implemented (C-style)
- **Cleanup Paths**: Explicit cleanup in all paths
- **Leak Prevention**: Matching allocate/free pairs
- **Thread Safety**: Critical sections for shared data

## Recent Improvements (v1.0.0)

### Critical Fixes Implemented

#### 1. Freak Candles Fix
**Issue**: Random spike candles during live updates
**Cause**: Non-normalized seconds in 1-minute timestamps
**Solution**: Timestamp normalization (Plugin.cpp:708-717)
**Status**: ✅ Fixed

#### 2. Timestamp Sorting Fix
**Issue**: Mixed-up timestamps in Quote Editor
**Cause**: No sorting after data merge
**Solution**: Added qsort() implementation (Plugin.cpp:880-886)
**Status**: ✅ Fixed

#### 3. WebSocket Status Display
**Issue**: Garbled Unicode characters
**Cause**: Character encoding problems
**Solution**: Simplified to plain text messages
**Status**: ✅ Fixed

## Known Issues and Limitations

### Current Limitations
1. **Intervals**: Only 1m and Daily supported
2. **Compression**: No data compression
3. **SSL Validation**: Not implemented
4. **Rate Limiting**: No client-side throttling

### Technical Debt
1. **Error Messages**: Some generic error handling
2. **Code Duplication**: WebSocket frame handling
3. **Magic Numbers**: Hardcoded buffer sizes
4. **Global Variables**: Extensive use of globals

### Recently Resolved (v1.0.0)
1. ~~Freak candles during live updates~~ → **FIXED**
2. ~~Timestamp sorting issues~~ → **FIXED**
3. ~~WebSocket status display~~ → **FIXED**

## Future Architecture Enhancements

### Planned Improvements
1. **Microservices**: Separate data and control planes
2. **Event-Driven**: Pub-sub architecture
3. **Async/Await**: Modern async patterns
4. **Dependency Injection**: Testable components
5. **Circuit Breakers**: Resilience patterns

### Performance Goals
- Sub-millisecond quote updates
- Zero data loss during reconnects
- 99.9% uptime availability
- Support for 10,000+ symbols

---

*This architecture documentation provides a comprehensive overview of the OpenAlgo AmiBroker Plugin's design and implementation. It serves as a reference for developers maintaining and extending the plugin.*