# OpenAlgo AmiBroker Data Plugin

**Version 1.0.0** | Real-time and Historical Market Data for Indian Brokers

An advanced AmiBroker data plugin that seamlessly connects with OpenAlgo-supported Indian brokers, providing real-time streaming quotes and historical market data with intelligent caching and robust error handling.

---

## ⚠️ IMPORTANT DISCLAIMER

**EDUCATIONAL PURPOSE ONLY - NOT FOR LIVE TRADING**

This plugin is provided **FOR EDUCATIONAL AND RESEARCH PURPOSES ONLY**. It is **NOT** intended, designed, or suitable for live trading or making real-time trading decisions.

### Data Source and Quality

- **Data Source**: All market data is sourced directly from brokers connected through the OpenAlgo platform
- **No Data Guarantee**: OpenAlgo, its maintainers, contributors, and team members make **NO WARRANTIES OR GUARANTEES** regarding:
  - Accuracy of market data
  - Completeness of market data
  - Timeliness of data delivery
  - Data integrity or reliability
  - Continuity of data service

### Liability Disclaimer

- **No Liability**: OpenAlgo, its maintainers, developers, contributors, and team are **NOT LIABLE** for:
  - Any trading losses or financial damages
  - Data errors, delays, or omissions
  - System failures or interruptions
  - Any decisions made based on this data
  - Consequences of using this plugin for trading

### Usage Restrictions

- ✅ **Permitted Use**: Education, learning, backtesting, research, analysis
- ❌ **Not Permitted**: Live trading decisions, automated trading systems, real-money trading

### Your Responsibility

- **Verify All Data**: Always verify data accuracy from official broker sources before any trading decision
- **Use at Your Own Risk**: You assume full responsibility for any use of this plugin
- **No Trading Advice**: This plugin does not provide trading advice or recommendations
- **Consult Professionals**: Consult with licensed financial advisors before trading

**BY USING THIS PLUGIN, YOU ACKNOWLEDGE AND ACCEPT THESE TERMS.**

---

## Overview

The OpenAlgo AmiBroker Plugin bridges AmiBroker with the OpenAlgo platform, enabling traders to access live market data from multiple Indian brokers through a unified API. Built with performance and reliability in mind, it supports both WebSocket-based real-time streaming and HTTP API for historical data retrieval.

## Key Features

### Data Capabilities
- **Real-time Quote Streaming**: WebSocket-based live market data with sub-second latency
- **Historical Data Backfill**: Intelligent data retrieval with automatic gap filling
- **Mixed EOD/Intraday Support**: Seamlessly combines daily and 1-minute data
- **Multiple Exchanges**: NSE, BSE, MCX, NFO, and other Indian exchanges
- **Intraday & EOD Data**: Full support for 1-minute and daily intervals

### Performance & Reliability
- **Intelligent Caching**: 5-second TTL quote cache for optimal performance
- **Auto-Reconnection**: Robust connection management with retry logic
- **Duplicate Prevention**: Smart timestamp matching prevents data duplication
- **Sorted Data**: Automatic chronological sorting for correct chart display
- **Timestamp Normalization**: Fixes "freak candle" issues during live updates

### User Experience
- **Simple Configuration**: Easy-to-use dialog with connection testing
- **Visual Status Indicators**: LED color-coded connection status
- **WebSocket Testing**: Built-in connection and subscription testing
- **Clean Status Messages**: User-friendly success/failure indicators

## Recent Improvements (v1.0.0)

### Critical Bug Fixes
1. **Freak Candles Fix** (2025-01-23)
   - Normalized seconds/milliseconds for 1-minute bars
   - Prevents duplicate bars from appearing during live updates
   - Location: `Plugin.cpp:708-717`

2. **Timestamp Sorting Fix** (2025-01-23)
   - Added automatic sorting after data merge
   - Ensures proper chronological order
   - Fixes mixed-up timestamps in Quote Editor
   - Location: `Plugin.cpp:880-886`

3. **WebSocket Status Simplification** (2025-01-23)
   - Removed problematic Unicode characters
   - Clean "successful" or "failed" messages
   - No more garbled text in status display
   - Location: `OpenAlgoConfigDlg.cpp:503-541`

## System Requirements

### Software Requirements
- **AmiBroker**: Version 6.0 or higher (64-bit recommended)
- **Windows**: 7/8/10/11 (64-bit)
- **OpenAlgo Server**: Latest version installed and running
- **.NET Framework**: 4.7.2 or higher
- **Visual C++ Redistributable**: 2019 or later

### Network Requirements
- Active internet connection
- Firewall access for HTTP/HTTPS (port 5000 or custom)
- Firewall access for WebSocket (ws:// or wss://)

### Broker Requirements
- Active account with OpenAlgo-supported broker
- Valid API credentials from OpenAlgo platform

## Installation

### Step 1: Download Plugin
Download the latest `OpenAlgo.dll` from:
- GitHub Releases: [OpenAlgo Plugin Releases](https://github.com/marketcalls/OpenAlgo-Amibroker-Plugin/releases)
- OpenAlgo Website: [https://openalgo.in](https://openalgo.in)

### Step 2: Install Plugin
1. Locate your AmiBroker installation directory (typically `C:\Program Files\AmiBroker`)
2. Navigate to the `Plugins` subdirectory
3. Copy `OpenAlgo.dll` to the `Plugins` folder
4. Restart AmiBroker

### Step 3: Configure Database
1. Open AmiBroker
2. Go to **File → Database Settings**
3. Click **Configure** next to the data source dropdown
4. Select **OpenAlgo Data Plugin**
5. Enter your configuration details:
   - **Server**: Your OpenAlgo server URL (e.g., `127.0.0.1` or domain)
   - **Port**: API port (default: `5000`)
   - **API Key**: Your OpenAlgo API key
   - **WebSocket URL**: WebSocket endpoint (e.g., `ws://127.0.0.1:8765`)
   - **Refresh Interval**: Data refresh rate in seconds (default: `5`)
   - **Time Shift**: Timezone adjustment in hours (default: `0`)

### Step 4: Test Connection
1. Click **Test Connection** button to verify HTTP API connectivity
2. Click **Test WebSocket** button to verify WebSocket connectivity
3. Both tests should show "successful" status
4. Click **OK** to save settings

### Step 5: Create Database
1. In AmiBroker, go to **File → New → Database**
2. Enter database name (e.g., "OpenAlgo Data")
3. Select **OpenAlgo Data Plugin** as data source
4. Click **Create**

## Configuration Reference

### Server Settings

| Setting | Description | Example | Default |
|---------|-------------|---------|---------|
| **Server** | OpenAlgo server hostname or IP | `127.0.0.1`, `api.example.com` | - |
| **Port** | HTTP API port number | `5000` | `5000` |
| **API Key** | Authentication key from OpenAlgo | `your-api-key-here` | - |
| **WebSocket URL** | Full WebSocket URL including protocol | `ws://127.0.0.1:8765` | - |

### Advanced Settings

| Setting | Description | Range | Default |
|---------|-------------|-------|---------|
| **Refresh Interval** | How often to check for new data (seconds) | 1-3600 | 5 |
| **Time Shift** | Timezone adjustment (hours) | -48 to +48 | 0 |

## Supported Brokers

The plugin supports **all brokers integrated with OpenAlgo**, including but not limited to:

### Major Brokers
- **Zerodha** (Kite API)
- **Angel One** (Angel One API)
- **Upstox** (Upstox API)
- **Dhan** (Dhan HQ API)
- **Fyers** (Fyers API v3)

### Additional Brokers
- 5Paisa
- AliceBlue
- Finvasia (Shoonya)
- Flattrade
- IIFL
- Kotak Securities
- Mastertrust
- Motilal Oswal
- And more...

> **Note**: Check the [OpenAlgo documentation](https://docs.openalgo.in) for the complete list of supported brokers.

## Supported Data Types

### Intervals
- **1-minute** (`1m`): Intraday data for the last 30 days
- **Daily** (`D`): End-of-day data for up to 1 year

### Markets
- **Equity**: NSE, BSE stocks
- **Derivatives**: NSE F&O (Futures & Options)
- **Commodities**: MCX commodities
- **Currency**: Currency futures
- **Indices**: NIFTY, BANKNIFTY, etc.

### Data Fields
- Open, High, Low, Close (OHLC)
- Volume
- Open Interest (for F&O)
- Timestamp (Unix epoch)

## Usage Guide

### Adding Symbols

#### Method 1: Via Symbol Dialog
1. Right-click in the Symbol Tree
2. Select **New Symbol**
3. Enter symbol name in format: `SYMBOL-EXCHANGE`
   - Example: `RELIANCE-NSE`, `NIFTY50-NSE`, `CRUDEOIL-MCX`

#### Method 2: Via Quick Search
1. Press `Ctrl+K` (Quick Search)
2. Type symbol name
3. If not found, AmiBroker will create it automatically

### Symbol Naming Convention

Use the format: `SYMBOL-EXCHANGE`

**Examples:**
- Equity: `TCS-NSE`, `INFY-NSE`
- Futures: `NIFTY25JANFUT-NFO`, `BANKNIFTY25JANFUT-NFO`
- Options: `NIFTY2552524000CE-NFO`
- Commodities: `CRUDEOIL25JANFUT-MCX`, `GOLD25FEBFUT-MCX`

### Viewing Charts

1. Select a symbol from the Symbol Tree
2. Chart will auto-load with historical data
3. Right-click on chart → **Parameters** to change interval
4. Available intervals: 1-minute, Daily
5. AmiBroker will compress 1-minute data for 5m, 15m, 60m views

### Real-time Updates

- Real-time data streams automatically via WebSocket
- Quote window shows live LTP (Last Traded Price)
- Charts update based on **Refresh Interval** setting
- Status LED (bottom-right) indicates connection status:
  - **Green**: Connected and receiving data
  - **Yellow**: Waiting to connect
  - **Red**: Disconnected, will retry
  - **Purple**: Manual reconnection required

## Troubleshooting

### Connection Issues

#### Problem: "Connection failed" error
**Solutions:**
1. Verify OpenAlgo server is running
2. Check server URL and port are correct
3. Ensure firewall allows connections
4. Test with `http://127.0.0.1:5000/api/v1/ping` in browser

#### Problem: "Authentication rejected" error
**Solutions:**
1. Verify API key is correct (copy from OpenAlgo settings)
2. Ensure no extra spaces in API key
3. Check API key is active in OpenAlgo

### Data Issues

#### Problem: No historical data loading
**Solutions:**
1. Check symbol format (must be `SYMBOL-EXCHANGE`)
2. Verify broker supports the symbol
3. Check market hours (some data only available during market)
4. Use correct exchange code (NSE, BSE, MCX, NFO)

#### Problem: Charts show gaps or missing data
**Solutions:**
1. Right-click symbol → **Backfill** to reload data
2. Check if broker API has data for that period
3. Verify date range (1m = 30 days max, Daily = 1 year)

#### Problem: Freak candles or spikes in chart
**Solutions:**
1. **Fixed in v1.0.0** - Update to latest plugin version
2. If issue persists, delete symbol and reload fresh data
3. Check Quote Editor (Ctrl+Q) for timestamp sorting

### WebSocket Issues

#### Problem: WebSocket test fails
**Solutions:**
1. Verify WebSocket URL is correct
2. Check WebSocket server is running
3. Ensure protocol is `ws://` (not `http://`)
4. Try `ws://127.0.0.1:8765` if using localhost

#### Problem: Real-time quotes not updating
**Solutions:**
1. Check WebSocket connection status (LED indicator)
2. Right-click status area → **Reconnect**
3. Verify subscription in Plugin status window
4. Restart AmiBroker to reinitialize connection

### Performance Issues

#### Problem: Slow data loading
**Solutions:**
1. Increase **Refresh Interval** to reduce server load
2. Reduce number of symbols in watchlist
3. Check network latency to OpenAlgo server
4. Use local OpenAlgo installation instead of remote

## API Reference

### HTTP Endpoints

#### Ping Endpoint
```
POST /api/v1/ping
Content-Type: application/json

{
  "apikey": "your-api-key"
}
```

#### Historical Data Endpoint
```
POST /api/v1/history
Content-Type: application/json

{
  "apikey": "your-api-key",
  "symbol": "RELIANCE",
  "exchange": "NSE",
  "interval": "1m",
  "start_date": "2025-01-01",
  "end_date": "2025-01-23"
}
```

#### Quotes Endpoint
```
POST /api/v1/quotes
Content-Type: application/json

{
  "apikey": "your-api-key",
  "symbol": "RELIANCE",
  "exchange": "NSE"
}
```

### WebSocket Protocol

#### Authentication
```json
{
  "action": "authenticate",
  "api_key": "your-api-key"
}
```

#### Subscribe
```json
{
  "action": "subscribe",
  "symbol": "RELIANCE",
  "exchange": "NSE",
  "mode": 2
}
```

#### Unsubscribe
```json
{
  "action": "unsubscribe",
  "symbol": "RELIANCE",
  "exchange": "NSE",
  "mode": 2
}
```

## Building from Source

See [BUILD_GUIDE.md](docs/BUILD_GUIDE.md) for detailed build instructions.

### Quick Start
1. Install Visual Studio 2019 or later
2. Open `OpenAlgoPlugin.sln`
3. Select **Release** configuration
4. Build Solution (F7)
5. Output: `x64\Release\OpenAlgo.dll`

## Documentation

- **[Technical Documentation](docs/TECHNICAL_DOCUMENTATION.md)**: Deep dive into implementation
- **[Architecture](docs/ARCHITECTURE.md)**: System design and components
- **[API Reference](docs/API_REFERENCE.md)**: Complete API documentation
- **[Build Guide](docs/BUILD_GUIDE.md)**: Developer setup and build instructions
- **[Changelog](docs/CHANGELOG.md)**: Version history and fixes
- **[Troubleshooting](docs/TROUBLESHOOTING.md)**: Common issues and solutions

## Support & Community

### Documentation
- **OpenAlgo Docs**: [https://docs.openalgo.in](https://docs.openalgo.in)
- **AmiBroker Docs**: [https://www.amibroker.com/guide/](https://www.amibroker.com/guide/)

### Community Support
- **GitHub Issues**: [Report bugs and request features](https://github.com/marketcalls/OpenAlgo-Amibroker-Plugin/issues)
- **OpenAlgo Forum**: Community discussions and support
- **Discord**: Real-time chat support

### Commercial Support
For commercial support and custom development, contact the OpenAlgo team.

## Contributing

We welcome contributions! Please follow these steps:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

### Development Guidelines
- Follow existing code style
- Add comments for complex logic
- Test thoroughly before submitting
- Update documentation for new features

## License

This project is licensed under the **MIT License** - see [LICENSE](LICENSE) for details.

## Disclaimer

### ⚠️ CRITICAL - READ BEFORE USE

This software is provided **"AS IS"**, without warranty of any kind, express or implied, including but not limited to the warranties of merchantability, fitness for a particular purpose, and noninfringement.

### Educational Purpose Only

**THIS PLUGIN IS FOR EDUCATIONAL PURPOSES ONLY**

- ❌ **NOT for live trading or real-time trading decisions**
- ❌ **NOT a replacement for official broker platforms**
- ❌ **NOT verified or certified for trading accuracy**
- ✅ **ONLY for learning, research, and educational backtesting**

### Data Source Disclaimer

**Data Origin**: All market data is sourced directly from brokers connected through OpenAlgo platform.

**No Guarantee**: OpenAlgo, its maintainers, developers, contributors, and team members:
- Make **NO WARRANTIES** about data accuracy, completeness, or timeliness
- Are **NOT RESPONSIBLE** for data quality, integrity, or reliability
- Do **NOT VERIFY** or validate broker-provided data
- Cannot guarantee continuous or uninterrupted data service
- Are **NOT LIABLE** for any data errors, delays, or omissions

### Trading Risk Disclaimer

- **High Risk**: Trading in financial markets involves substantial risk of loss
- **Capital Loss**: You may lose all your investment capital
- **No Guarantees**: Past performance does not guarantee future results
- **Professional Advice**: Consult licensed financial advisors before trading
- **Own Risk**: Only trade with capital you can afford to lose completely
- **Risk Management**: Always use proper risk management techniques

### Software Liability

**OpenAlgo, its maintainers, developers, contributors, and team are NOT responsible for:**

- Any trading losses or financial damages
- Data inaccuracies, errors, delays, or omissions
- System failures, crashes, or interruptions
- Any decisions made based on data from this plugin
- Consequences of using this plugin for any purpose
- Third-party broker API failures or issues
- Network connectivity problems
- Any direct, indirect, incidental, or consequential damages

### User Responsibilities

**By using this plugin, you agree to:**

1. ✅ Use it for **educational purposes only**
2. ✅ **Verify all data** from official broker sources before any trading decision
3. ✅ **Never rely solely** on this plugin for trading decisions
4. ✅ **Accept full responsibility** for all your trading decisions
5. ✅ **Understand** that data may be delayed, incomplete, or inaccurate
6. ✅ **Test thoroughly** in paper trading before any real use
7. ✅ **Maintain backups** of all important data
8. ✅ **Consult professionals** before making financial decisions

### No Trading Recommendations

- This plugin does **NOT** provide trading advice, signals, or recommendations
- This plugin does **NOT** guarantee profitable trading
- This plugin is **NOT** a financial advisory service
- All trading decisions are your own responsibility

### Legal Disclaimer

- Use of this plugin does not create any advisor-client relationship
- This plugin is not registered with any financial regulatory authority
- Consult your local laws and regulations regarding trading software
- Some features may not be legal in certain jurisdictions

**BY INSTALLING, CONFIGURING, OR USING THIS PLUGIN, YOU ACKNOWLEDGE THAT YOU HAVE READ, UNDERSTOOD, AND ACCEPT ALL TERMS OF THIS DISCLAIMER.**

## Acknowledgments

- **OpenAlgo Team**: For the excellent broker integration platform
- **AmiBroker**: For the powerful charting and analysis platform
- **Contributors**: Everyone who has contributed code and feedback
- **Community**: For testing, bug reports, and feature suggestions

## Version History

### v1.0.0 (2025-01-23)
- Initial release with core functionality
- Fixed freak candles issue (timestamp normalization)
- Fixed timestamp sorting issue
- Simplified WebSocket status messages
- Comprehensive documentation

---

**Made with ❤️ for the Indian Trading Community**

For more information, visit [OpenAlgo.in](https://openalgo.in) or [AmiBroker.com](https://www.amibroker.com)
