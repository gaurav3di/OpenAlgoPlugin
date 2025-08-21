# OpenAlgo Amibroker Data Plugin

An Amibroker data plugin that connects with OpenAlgo-supported Indian brokers for real-time and historical market data streaming.

## About OpenAlgo

OpenAlgo is a self-hosted opensource algo trading platform that makes automating trading orders easy and efficient. Designed with the flexibility to operate from your desktop, laptop, or on servers, OpenAlgo is built using the Python Flask Framework. It features a sleek and user-friendly UI designed with DaisyUI/Tailwind CSS and uses a robust SQLite database for seamless local data management.

## Features

- Real-time data streaming from OpenAlgo-supported Indian brokers
- Historical data backfill capabilities
- Multiple broker support through OpenAlgo API
- Seamless integration with Amibroker's charting and analysis tools
- Support for NSE, BSE, MCX, and other Indian exchanges
- Intraday and EOD data support
- Fast and reliable data updates

## Installation

1. Download the latest release of OpenAlgo Data Plugin
2. Copy the `OpenAlgo.dll` file to your Amibroker `Plugins` directory (typically `C:\Program Files\AmiBroker\Plugins`)
3. Restart Amibroker
4. Configure the plugin through File > Database Settings

## Configuration

1. Open Amibroker
2. Navigate to File > Database Settings
3. Select "OpenAlgo" as your data source
4. Click "Configure" to set up your connection:
   - Enter your OpenAlgo server URL
   - Provide your API credentials
   - Select your preferred broker
   - Configure update intervals
5. Save and apply settings

## Prerequisites

- Amibroker 6.0 or higher
- Windows 7/8/10/11 (64-bit recommended)
- Active internet connection
- OpenAlgo platform installed and configured
- Valid broker account with one of the OpenAlgo-supported Indian brokers

## Supported Brokers

All brokers supported by OpenAlgo platform, including:
- Zerodha
- Angel One
- Upstox
- Dhan
- Fyers
- Groww
- IIFL
- 5Paisa
- And many more Indian brokers

## Data Types Supported

- Real-time tick data
- 1-minute, 5-minute, 15-minute, hourly candles
- End-of-day (EOD) data
- Index data
- Futures & Options data
- Currency and commodity data

## Support & Documentation

- OpenAlgo Documentation: [https://docs.openalgo.in/](https://docs.openalgo.in/)
- Plugin Issues: [GitHub Issues](https://github.com/yourusername/OpenAlgoPlugin/issues)
- OpenAlgo Community: Visit the OpenAlgo forums for community support

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Disclaimer

This software is provided "as is", without warranty of any kind. Trading in financial markets involves risk. Please ensure you understand the risks involved in trading and investing. Use this plugin at your own risk.