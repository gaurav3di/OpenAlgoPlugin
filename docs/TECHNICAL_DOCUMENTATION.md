# OpenAlgo AmiBroker Plugin - Technical Documentation

## Table of Contents
1. [Overview](#overview)
2. [Architecture](#architecture)
3. [API Integration](#api-integration)
4. [WebSocket Implementation](#websocket-implementation)
5. [Data Flow](#data-flow)
6. [Plugin Components](#plugin-components)
7. [Configuration Management](#configuration-management)
8. [Error Handling](#error-handling)
9. [Performance Optimization](#performance-optimization)
10. [Building and Deployment](#building-and-deployment)

## Overview

The OpenAlgo AmiBroker Plugin is a sophisticated data plugin that bridges AmiBroker with OpenAlgo's comprehensive broker integration platform. It provides real-time and historical market data from multiple Indian brokers through a unified API interface.

### Key Features
- **Real-time Quote Streaming**: WebSocket-based live market data
- **Historical Data Backfill**: Intelligent data retrieval with caching
- **Multi-Exchange Support**: NSE, BSE, MCX, and other Indian exchanges
- **Authentication**: Secure API key-based authentication
- **Auto-Reconnection**: Robust connection management with retry logic
- **Performance Optimized**: Efficient data caching and minimal latency

## Architecture

### System Components

```
┌─────────────────┐
│   AmiBroker     │
│   Application   │
└────────┬────────┘
         │
    Plugin API
         │
┌────────▼────────┐
│  OpenAlgo       │
│  Plugin DLL     │
├─────────────────┤
│ • Plugin.cpp    │
│ • ConfigDlg.cpp │
│ • WebSocket     │
│ • HTTP Client   │
└────────┬────────┘
         │
    Network Layer
         │
    ┌────┴────┐
    │         │
HTTP API  WebSocket
    │         │
┌───▼─────────▼───┐
│  OpenAlgo       │
│  Server         │
├─────────────────┤
│ • REST API      │
│ • WebSocket Hub │
│ • Broker APIs   │
└─────────────────┘
         │
    Broker APIs
         │
┌────────▼────────┐
│ Indian Brokers  │
│ (Zerodha, etc.) │
└─────────────────┘
```

### Plugin Type
- **Type**: Data Plugin (PLUGIN_TYPE_DATA)
- **Version**: 10003
- **Plugin ID**: PIDCODE('O', 'A', 'L', 'G')
- **Minimum AmiBroker Version**: 5.30.0

## API Integration

### REST API Endpoints

#### 1. Ping Endpoint
**Endpoint**: `/api/v1/ping`  
**Method**: POST  
**Purpose**: Verify server connectivity and authentication

**Request Body**:
```json
{
    "apikey": "your_api_key"
}
```

**Response**:
```json
{
    "status": "success",
    "message": "pong",
    "broker": "zerodha"
}
```

#### 2. Quotes Endpoint
**Endpoint**: `/api/v1/quotes`  
**Method**: POST  
**Purpose**: Fetch real-time Level 1 quotes

**Request Body**:
```json
{
    "apikey": "your_api_key",
    "symbol": "RELIANCE",
    "exchange": "NSE"
}
```

**Response**:
```json
{
    "status": "success",
    "data": {
        "ltp": 2450.50,
        "open": 2440.00,
        "high": 2455.00,
        "low": 2435.00,
        "volume": 1234567,
        "oi": 0,
        "prev_close": 2445.00
    }
}
```

#### 3. Historical Data Endpoint
**Endpoint**: `/api/v1/history`  
**Method**: POST  
**Purpose**: Retrieve historical OHLCV data

**Request Body**:
```json
{
    "apikey": "your_api_key",
    "symbol": "RELIANCE",
    "exchange": "NSE",
    "interval": "1m",
    "start_date": "2024-01-01",
    "end_date": "2024-01-31"
}
```

**Response**:
```json
{
    "status": "success",
    "data": [
        {
            "timestamp": 1704096600,
            "open": 2440.00,
            "high": 2442.00,
            "low": 2438.00,
            "close": 2441.00,
            "volume": 12345,
            "oi": 0
        }
    ]
}
```

### Supported Intervals
- `1m` - 1 minute (intraday)
- `D` - Daily (End of Day)

## WebSocket Implementation

### Connection Flow

1. **TCP Connection**: Establish socket connection to WebSocket server
2. **HTTP Upgrade**: Send WebSocket upgrade request
3. **Authentication**: Send API key for authentication
4. **Subscription**: Subscribe to required symbols
5. **Data Reception**: Receive and process market data frames

### WebSocket Protocol

#### Authentication Message
```json
{
    "action": "authenticate",
    "api_key": "your_api_key"
}
```

#### Subscription Message
```json
{
    "action": "subscribe",
    "symbol": "RELIANCE",
    "exchange": "NSE",
    "mode": 2
}
```

#### Market Data Reception
```json
{
    "type": "market_data",
    "symbol": "RELIANCE",
    "exchange": "NSE",
    "ltp": 2450.50,
    "volume": 1234567
}
```

### WebSocket Frame Structure
- **FIN**: 1 (Final frame)
- **OpCode**: 0x01 (Text frame)
- **Mask**: 1 (Client must mask)
- **Payload**: JSON message

### Connection Management
- **Ping Interval**: 30 seconds
- **Reconnection Delay**: 10 seconds
- **Timeout**: 5 seconds for operations
- **Non-blocking Mode**: After initial handshake

## Data Flow

### Real-time Quote Flow
1. AmiBroker calls `GetRecentInfo()` for symbol
2. Plugin checks WebSocket subscription status
3. If not subscribed, sends subscription request
4. Processes incoming WebSocket data
5. Updates quote cache
6. Returns `RecentInfo` structure to AmiBroker

### Historical Data Flow
1. AmiBroker calls `GetQuotesEx()` with parameters
2. Plugin determines data range:
   - Initial load: 30 days for 1m, 1 year for daily
   - Refresh: Today's data only
3. Sends HTTP request to history endpoint
4. Parses JSON response
5. Converts timestamps to AmiBroker format
6. **NEW (v1.0.0)**: Normalizes timestamps for 1-minute bars
   - Sets Second, MilliSec, MicroSec to 0
   - Prevents "freak candles" during live updates
7. Handles duplicates intelligently
8. **NEW (v1.0.0)**: Sorts quotes chronologically using qsort()
   - Ensures proper timestamp order
   - Fixes mixed-up timestamps in Quote Editor
9. Returns sorted quote array to AmiBroker

## Plugin Components

### Core Files

#### Plugin.cpp
- Main plugin implementation
- AmiBroker API functions
- Data retrieval logic
- WebSocket management
- Quote caching system

#### OpenAlgoConfigDlg.cpp
- Configuration dialog implementation
- Connection testing functionality
- Settings persistence
- WebSocket testing

#### OpenAlgoPlugin.cpp
- DLL initialization
- MFC application setup
- Registry key management

#### OpenAlgoGlobals.h
- Global variable declarations
- Server configuration
- API keys
- Status management

### Key Functions

#### `GetPluginInfo()`
Returns plugin metadata to AmiBroker

#### `Init()`
- Initializes plugin
- Loads configuration
- Sets up WebSocket
- Initializes quote cache

#### `GetQuotesEx()`
- Retrieves historical data
- Manages data backfill
- Handles different periodicities

#### `GetRecentInfo()`
- Provides real-time quotes
- Manages WebSocket subscriptions
- Updates quote window

#### `GetStatus()`
- Reports connection status
- Updates status LED
- Provides error messages

#### `Configure()`
- Opens configuration dialog
- Tests connections
- Saves settings

## Configuration Management

### Registry Storage
**Location**: `HKEY_CURRENT_USER\Software\OpenAlgo`

**Settings**:
- `Server`: OpenAlgo server address
- `ApiKey`: Authentication API key
- `WebSocketUrl`: WebSocket server URL
- `Port`: HTTP API port
- `RefreshInterval`: Data refresh interval
- `TimeShift`: Time zone adjustment

### Configuration Dialog Features
- Server connection testing
- WebSocket connection testing
- Live quote display during test
- Input validation
- Secure API key storage

## Error Handling

### Connection States
1. **STATUS_WAIT** (Yellow): Waiting to connect
2. **STATUS_CONNECTED** (Green): Successfully connected
3. **STATUS_DISCONNECTED** (Red): Connection failed, will retry
4. **STATUS_SHUTDOWN** (Purple): Offline, manual reconnect required

### Retry Logic
- **Initial Retry Count**: 8 attempts
- **Retry Interval**: 15 seconds
- **Exponential Backoff**: No (fixed interval)
- **Manual Reconnect**: Via right-click menu

### Error Recovery
- Automatic reconnection on disconnect
- WebSocket fallback to HTTP API
- Cache preservation during disconnects
- Duplicate data handling

## Performance Optimization

### Caching Strategy
- **Quote Cache**: 5-second TTL for real-time quotes
- **Hash Table Size**: 997 buckets (prime number)
- **WebSocket Priority**: Prefer WebSocket over HTTP
- **Batch Operations**: Process multiple symbols efficiently

### Data Efficiency
- **Intelligent Backfill**: Only fetch missing data
- **Duplicate Detection**: Compare timestamps within 60 seconds
- **Incremental Updates**: Refresh only today's data
- **Non-blocking I/O**: WebSocket uses async operations
- **Timestamp Normalization** (v1.0.0): 1-minute bars normalized to prevent duplicates
- **Chronological Sorting** (v1.0.0): Automatic qsort() after data merge

### Memory Management
- **Dynamic Allocation**: Based on data size
- **Cleanup on Disconnect**: Release resources properly
- **Buffer Limits**: 4KB for WebSocket frames
- **Response Truncation**: 30KB max for HTTP responses

## Building and Deployment

### Prerequisites
- Visual Studio 2019 or later
- Windows SDK
- MFC libraries
- AmiBroker SDK headers

### Build Configuration
- **Platform**: x64 (64-bit)
- **Configuration**: Release
- **Runtime Library**: Multi-threaded DLL
- **Character Set**: Unicode

### Deployment Steps
1. Build the plugin in Release mode
2. Copy `OpenAlgo.dll` to AmiBroker Plugins folder
3. Restart AmiBroker
4. Configure via File > Database Settings
5. Enter API credentials
6. Test connection

### Dependencies
- **WinSock2**: WebSocket implementation
- **WinInet**: HTTP client
- **MFC**: Dialog and UI components
- **CRT**: C++ runtime

## Security Considerations

### API Key Protection
- Stored in Windows Registry
- Never logged or displayed in plain text
- Transmitted over secure channels
- Validated on each request

### Network Security
- Support for WSS (WebSocket Secure)
- HTTPS support for API calls
- Input validation
- Buffer overflow protection

## Troubleshooting

### Common Issues

1. **Connection Failed**
   - Verify server URL and port
   - Check API key validity
   - Ensure firewall allows connections
   - Verify OpenAlgo server is running

2. **No Data Received**
   - Check market hours
   - Verify symbol format (SYMBOL-EXCHANGE)
   - Ensure subscription is active
   - Check WebSocket connection status

3. **WebSocket Disconnects**
   - Network instability
   - Server timeout
   - Authentication failure
   - Check ping/pong mechanism

### Debug Information
- Status LED color indicates connection state
- Right-click status area for manual control
- Check AmiBroker log for errors
- Use configuration dialog test buttons

## Recent Enhancements (v1.0.0)

### Critical Bug Fixes

#### 1. Timestamp Normalization for 1-Minute Bars
**Location**: `Plugin.cpp:708-717`

**Problem**: "Freak candles" appeared during live updates due to varying seconds in timestamps

**Solution**: Normalize Second, MilliSec, MicroSec fields to 0 for 1-minute bars
```cpp
if (nPeriodicity == 60) // 1-minute data
{
    pQuotes[quoteIndex].DateTime.PackDate.Second = 0;
    pQuotes[quoteIndex].DateTime.PackDate.MilliSec = 0;
    pQuotes[quoteIndex].DateTime.PackDate.MicroSec = 0;
}
```

#### 2. Chronological Sorting
**Location**: `Plugin.cpp:123-139, 880-886`

**Problem**: Timestamps mixed up when filling data gaps

**Solution**: Added qsort() with custom comparator after data merge
```cpp
int CompareQuotations(const void* a, const void* b)
{
    const struct Quotation* qa = (const struct Quotation*)a;
    const struct Quotation* qb = (const struct Quotation*)b;

    if (qa->DateTime.Date < qb->DateTime.Date) return -1;
    else if (qa->DateTime.Date > qb->DateTime.Date) return 1;
    else return 0;
}
```

#### 3. WebSocket Status Simplification
**Location**: `OpenAlgoConfigDlg.cpp:503-541`

**Problem**: Unicode characters displayed as garbled text

**Solution**: Removed Unicode icons, use simple text messages

## Future Enhancements

### Planned Features
- Support for more intervals (5m, 15m, 1h)
- Tick-by-tick data support
- Options chain data
- Market depth (Level 2) data
- Compression for large datasets
- SSL/TLS certificate validation

### Performance Improvements
- Connection pooling
- Data compression
- Binary protocol support
- Multi-threaded operations
- Advanced caching strategies

## Support and Resources

### Documentation
- OpenAlgo Documentation: https://docs.openalgo.in/
- AmiBroker Plugin SDK: AmiBroker ADK documentation
- GitHub Repository: Plugin source code and issues

### Community
- OpenAlgo Forums
- GitHub Issues
- Discord/Telegram groups

---

*This documentation is maintained by the OpenAlgo community. For updates and contributions, please visit the GitHub repository.*