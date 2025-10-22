# History

## Endpoint URL

This API Function to fetch historical data from the Broker

```http
Local Host   :  POST http://127.0.0.1:5000/api/v1/history
Ngrok Domain :  POST https://<your-ngrok-domain>.ngrok-free.app/api/v1/history
Custom Domain:  POST https://<your-custom-domain>/api/v1/history
```

## Sample API Request

```json
{
    "apikey": "<your_app_apikey>",
    "symbol": "NIFTY31JUL25FUT",
    "exchange": "NFO",
    "interval": "1m",
    "start_date": "2025-06-26",
    "end_date": "2025-06-27",
  
}

```

###

## Sample API Response

```json
{
  "data": [
    {
      "close": 25292,
      "high": 25302.1,
      "low": 25272.3,
      "oi": 5401650,
      "open": 25302,
      "timestamp": 1750909500,
      "volume": 1042
    },
    {
      "close": 25298,
      "high": 25301,
      "low": 25287.6,
      "oi": 5401650,
      "open": 25288.9,
      "timestamp": 1750909560,
      "volume": 462
    },
    {
      "close": 25303,
      "high": 25310.1,
      "low": 25298.1,
      "oi": 5401650,
      "open": 25298.9,
      "timestamp": 1750909620,
      "volume": 429
    }
  ],
  "status": "success"
}
```

## Request Body

| Parameters  | Description                            | Mandatory/Optional | Default Value |
| ----------- | -------------------------------------- | ------------------ | ------------- |
| apikey      | App API key                            | Mandatory          | -             |
| symbol      | Trading symbol                         | Mandatory          | -             |
| exchange    | Exchange code                          | Mandatory          | -             |
| interval    | candle interval (see supported values) | Mandatory          | -             |
| start\_date | Start date (YYYY-MM-DD)                | Mandatory          | -             |
| end\_date   | End date (YYYY-MM-DD)                  | Mandatory          | -             |

## Response Fields

| Field     | Type   | Description          |
| --------- | ------ | -------------------- |
| timestamp | number | Unix epoch timestamp |
| open      | number | Opening price        |
| high      | number | High price           |
| oi        | number | Open Interest        |
| low       | number | Low price            |
| close     | number | Closing price        |
| volume    | number | Trading volume       |

## Notes

1. Always check supported intervals first using the intervals API
2. Use exact interval strings from intervals API response
3. All timestamps are in Unix epoch format