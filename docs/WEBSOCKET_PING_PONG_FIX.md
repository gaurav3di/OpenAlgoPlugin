# WebSocket PING/PONG Keepalive Fix

## Problem Summary

The C++ plugin was being disconnected by the OpenAlgo WebSocket server every ~30-60 seconds with:
```
CLOSE_FRAME (Status Code: 1011, Reason: keepalive ping timeout)
```

## Root Cause Analysis

### Server-Side (OpenAlgo Python)

In `websocket_proxy/server.py`, the WebSocket server uses Python's `websockets` library with **default settings**:

```python
self.server = await websockets.serve(
    self.handle_client,
    self.host,
    self.port
)
```

Default behavior:
- `ping_interval=20` seconds → Server sends PING every 20 seconds
- `ping_timeout=20` seconds → Server expects PONG within 20 seconds
- **Total timeout: 40 seconds** (matches observed ~30-60 second disconnects)

### Client-Side (C++ Plugin)

**The Bug:** Plugin was sending PONG with **empty payload**

In `Plugin.cpp` (line ~2750):
```cpp
if (data == _T("PING_FRAME"))
{
    // WRONG: Sends PONG with empty payload (length 0)
    unsigned char pongFrame[6] = {0x8A, 0x80, 0x00, 0x00, 0x00, 0x00};
    GenerateWebSocketMaskKey(&pongFrame[2]);
    send(g_websocket, (char*)pongFrame, 6, 0);
}
```

**RFC 6455 Section 5.5.3 Requirement:**
> "A Pong frame sent in response to a Ping frame must have identical 'Application data' as found in the message body of the Ping frame being replied to."

The Python `websockets` library sends PING with a **4-byte payload** (typically a timestamp or random data), and expects it **echoed back exactly**. Our plugin was discarding the payload and sending PONG with length 0, so the server rejected it as invalid.

## The Fix

### Change 1: Modify `DecodeWebSocketFrame()` to Extract PING Payload

**Location:** Plugin.cpp, `DecodeWebSocketFrame()` function (around line 2100)

**Replace:**
```cpp
else if (opcode == 0x09) // Ping frame
{
    return _T("PING_FRAME");
}
```

**With:**
```cpp
else if (opcode == 0x09) // Ping frame
{
    // Extract PING payload to echo back in PONG (RFC 6455 requirement)
    CString pingResult = _T("PING_FRAME");

    // Handle extended payload length
    int pingPos = pos;
    if (payloadLen == 126)
    {
        if (pingPos + 2 > length) return pingResult;
        payloadLen = ((unsigned char)buffer[pingPos] << 8) | (unsigned char)buffer[pingPos + 1];
        pingPos += 2;
    }

    // Extract masking key if present
    unsigned char pingMaskKey[4] = {0};
    if (masked)
    {
        if (pingPos + 4 > length) return pingResult;
        memcpy(pingMaskKey, &buffer[pingPos], 4);
        pingPos += 4;
    }

    // Extract payload (usually 4 bytes)
    if (payloadLen > 0 && payloadLen <= 125 && pingPos + payloadLen <= length)
    {
        CStringA hexPayload;
        for (int i = 0; i < payloadLen; i++)
        {
            unsigned char byte = masked ? (buffer[pingPos + i] ^ pingMaskKey[i % 4]) : buffer[pingPos + i];
            CStringA hexByte;
            hexByte.Format("%02X", byte);
            hexPayload += hexByte;
        }

        // Return "PING_FRAME:XXXXXXXX" where X is hex-encoded payload
        pingResult = _T("PING_FRAME:") + CString(hexPayload);
    }

    return pingResult;
}
```

### Change 2: Modify PING Handler to Echo Payload in PONG

**Location:** Plugin.cpp, `ProcessWebSocketData()` function (around line 2750)

**Replace:**
```cpp
if (data == _T("PING_FRAME"))
{
    // Send pong response with empty payload
    unsigned char pongFrame[6] = {0x8A, 0x80, 0x00, 0x00, 0x00, 0x00};
    GenerateWebSocketMaskKey(&pongFrame[2]);
    send(g_websocket, (char*)pongFrame, 6, 0);
    OutputDebugString(_T("OpenAlgo: Received PING, sent PONG"));
    continue;
}
```

**With:**
```cpp
if (data.Find(_T("PING_FRAME")) == 0)  // Starts with "PING_FRAME"
{
    // Extract payload from "PING_FRAME:XXXXXXXX" format
    CString payload;
    int colonPos = data.Find(':');
    if (colonPos > 0)
    {
        payload = data.Mid(colonPos + 1);
    }

    // Convert hex payload back to bytes
    int payloadLen = payload.GetLength() / 2;
    unsigned char payloadBytes[125] = {0};  // Max payload size
    for (int i = 0; i < payloadLen && i < 125; i++)
    {
        CString hexByte = payload.Mid(i * 2, 2);
        payloadBytes[i] = (unsigned char)_tcstoul(hexByte, NULL, 16);
    }

    // Build PONG frame with echoed payload
    unsigned char pongFrame[256];
    int frameLen = 0;

    pongFrame[frameLen++] = 0x8A;  // FIN + opcode 0x0A (PONG)
    pongFrame[frameLen++] = 0x80 | payloadLen;  // MASK bit + payload length

    // Generate and add masking key
    unsigned char maskKey[4];
    GenerateWebSocketMaskKey(maskKey);
    memcpy(&pongFrame[frameLen], maskKey, 4);
    frameLen += 4;

    // Add masked payload
    for (int i = 0; i < payloadLen; i++)
    {
        pongFrame[frameLen++] = payloadBytes[i] ^ maskKey[i % 4];
    }

    // Send PONG with echoed payload
    send(g_websocket, (char*)pongFrame, frameLen, 0);

    CString pongLog;
    pongLog.Format(_T("OpenAlgo: Received PING with %d-byte payload, sent PONG with echoed payload"), payloadLen);
    OutputDebugString(pongLog);

    continue;
}
```

## Expected Result

After this fix:
1. Server sends PING with 4-byte payload (e.g., `0x12345678`)
2. Client extracts payload → "PING_FRAME:12345678"
3. Client sends PONG with same 4-byte payload `0x12345678`
4. Server validates PONG payload matches → connection stays alive ✓
5. **No more keepalive ping timeout disconnects!**

## Testing

1. Rebuild plugin with these changes
2. Connect to OpenAlgo WebSocket
3. Monitor DebugView for logs showing:
   - "Received PING with 4-byte payload, sent PONG with echoed payload"
4. Connection should stay alive indefinitely (no more 30-60 second disconnects)
5. Verify continuous tick flow without gaps

## References

- RFC 6455 Section 5.5.2 (Ping): https://datatracker.ietf.org/doc/html/rfc6455#section-5.5.2
- RFC 6455 Section 5.5.3 (Pong): https://datatracker.ietf.org/doc/html/rfc6455#section-5.5.3
- Python websockets library docs: https://websockets.readthedocs.io/
