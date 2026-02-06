/**
 * @file SerialModemBase.cpp
 * @brief Implementation of common modem logic.
 * @copyright 2026 Circuit Design, Inc.
 */

#include "SerialModemBase.h"
#include <stdio.h>
#include <string.h>

// Define static constexpr members to ensure storage is allocated
constexpr char SerialModemBase::CD_WRITE_OK_RESPONSE[];
constexpr size_t SerialModemBase::CD_WRITE_OK_RESPONSE_LEN;
constexpr char SerialModemBase::CD_VAL_ON[];
constexpr char SerialModemBase::CD_VAL_OFF[];
constexpr char SerialModemBase::CD_CMD_WRITE_SUFFIX[];

SerialModemBase::SerialModemBase()
    : _uart(nullptr), _debugStream(nullptr),
      _rxIndex(0), _oneByteBuf(-1), _debugRxNewLine(true),
      _bTimeout(true), _startTime(0), _timeOutDuration(0)
{
    memset(_rxBuffer, 0, RX_BUFFER_SIZE);
}

void SerialModemBase::setDebugStream(Stream *debugStream)
{
    _debugStream = debugStream;
}

void SerialModemBase::initSerial(Stream &stream)
{
    _uart = &stream;
    clearUnreadByte();
    _debugRxNewLine = true;
    _rxIndex = 0;
}

// --- High-Level Transaction Helpers ---

ModemError SerialModemBase::waitForResponse(uint32_t timeoutMs)
{
    // Reset timeout
    startTimeout(timeoutMs);

    SM_DEBUG_PRINTF("Wait: Waiting up to %lu ms...\n", timeoutMs);

    while (!isTimeout())
    {
        ModemParseResult result = parse();

        switch (result)
        {
        case ModemParseResult::Parsing:
            // Continue waiting
            delay(1);
            break;

        case ModemParseResult::FinishedCmdResponse:
            SM_DEBUG_PRINTF("Wait: CMD Response: '%.*s'\n", _rxIndex, _rxBuffer);
            return ModemError::Ok;

        case ModemParseResult::FinishedDrResponse:
            SM_DEBUG_PRINTLN("Wait: Intervening DR received. Handling callback...");
            // Handle the asynchronous data packet
            onRxDataReceived();
            // Resume waiting for the original command response
            SM_DEBUG_PRINTLN("Wait: Resume waiting for CMD...");
            break;

        case ModemParseResult::Garbage:
        case ModemParseResult::Overflow:
            SM_DEBUG_PRINTLN("Wait: Error (Garbage/Overflow).");
            return ModemError::Fail;
        }
    }

    SM_DEBUG_PRINTLN("Wait: Timeout.");
    return ModemError::Timeout;
}

ModemError SerialModemBase::setByteValue(const char *cmd, uint8_t value, bool save, const char *respPrefix, size_t respLen)
{
    // 1. Send Command
    char buf[32];
    snprintf(buf, sizeof(buf), "%s%02X%s\r\n", cmd, value, save ? CD_CMD_WRITE_SUFFIX : "");
    writeString(buf);

    // 2. Wait for Response
    ModemError err = waitForResponse();

    // 3. Handle Write Response (*WR=PS) if saving
    if (err == ModemError::Ok && save)
    {
        // Check if current buffer is *WR=PS
        if (_rxIndex == CD_WRITE_OK_RESPONSE_LEN &&
            strncmp(reinterpret_cast<char *>(_rxBuffer), CD_WRITE_OK_RESPONSE, CD_WRITE_OK_RESPONSE_LEN) == 0)
        {

            // Wait for the actual value response that follows
            err = waitForResponse();
        }
    }

    // 4. Verify Final Response
    uint8_t parsedValue = 0;
    if (err == ModemError::Ok)
    {
        uint32_t tmpVal = 0;
        err = parseResponseHex(_rxBuffer, _rxIndex, respPrefix, (uint8_t)(respLen - strlen(respPrefix)), &tmpVal);
        if (err == ModemError::Ok)
        {
            parsedValue = static_cast<uint8_t>(tmpVal);
        }
    }

    // 5. Check if value matches
    if (err == ModemError::Ok && parsedValue != value)
    {
        return ModemError::Fail;
    }

    return err;
}

ModemError SerialModemBase::getByteValue(const char *cmd, uint8_t *pValue, const char *respPrefix, size_t respLen)
{
    // Send Command
    char buf[16];
    snprintf(buf, sizeof(buf), "%s\r\n", cmd);
    writeString(buf);

    // Wait
    ModemError err = waitForResponse();

    // Parse
    if (err == ModemError::Ok)
    {
        uint32_t tmpVal = 0;
        err = parseResponseHex(_rxBuffer, _rxIndex, respPrefix, (uint8_t)(respLen - strlen(respPrefix)), &tmpVal);
        if (err == ModemError::Ok && pValue)
        {
            *pValue = static_cast<uint8_t>(tmpVal);
        }
    }
    return err;
}

ModemError SerialModemBase::setBoolValue(const char *baseCmd, bool enabled, bool save, const char *respPrefix)
{
    // 1. Send Command
    char buf[32];
    snprintf(buf, sizeof(buf), "%s%s%s\r\n", baseCmd, enabled ? CD_VAL_ON : CD_VAL_OFF, save ? CD_CMD_WRITE_SUFFIX : "");
    writeString(buf);

    // 2. Wait for Response
    ModemError err = waitForResponse();

    // 3. Handle Write Response (*WR=PS) if saving
    if (err == ModemError::Ok && save)
    {
        if (_rxIndex == CD_WRITE_OK_RESPONSE_LEN &&
            strncmp(reinterpret_cast<char *>(_rxBuffer), CD_WRITE_OK_RESPONSE, CD_WRITE_OK_RESPONSE_LEN) == 0)
        {
            err = waitForResponse();
        }
    }

    // 4. Verify Final Response
    if (err == ModemError::Ok)
    {
        size_t prefixLen = strlen(respPrefix);
        if (_rxIndex < prefixLen + 2 || strncmp(reinterpret_cast<char *>(_rxBuffer), respPrefix, prefixLen) != 0)
        {
            return ModemError::Fail;
        }

        const char *valPtr = reinterpret_cast<char *>(_rxBuffer) + prefixLen;
        const char *expected = enabled ? CD_VAL_ON : CD_VAL_OFF;
        if (strncmp(valPtr, expected, 2) != 0)
        {
            return ModemError::Fail;
        }
    }
    return err;
}

ModemError SerialModemBase::getBoolValue(const char *cmd, bool *pValue, const char *respPrefix)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%s\r\n", cmd);
    writeString(buf);

    ModemError err = waitForResponse();
    if (err == ModemError::Ok)
    {
        size_t prefixLen = strlen(respPrefix);
        if (_rxIndex < prefixLen + 2 || strncmp(reinterpret_cast<char *>(_rxBuffer), respPrefix, prefixLen) != 0)
        {
            return ModemError::Fail;
        }
        const char *valPtr = reinterpret_cast<char *>(_rxBuffer) + prefixLen;
        if (pValue)
            *pValue = (strncmp(valPtr, CD_VAL_ON, 2) == 0);
    }
    return err;
}

ModemError SerialModemBase::sendRawCommand(const char *command, char *responseBuffer, size_t bufferSize, uint32_t timeoutMs)
{
    if (!command || !responseBuffer)
        return ModemError::InvalidArg;

    writeString(command);

    ModemError err = waitForResponse(timeoutMs);

    if (err == ModemError::Ok)
    {
        if (_rxIndex < bufferSize)
        {
            memcpy(responseBuffer, _rxBuffer, _rxIndex);
            responseBuffer[_rxIndex] = '\0';
        }
        else
        {
            responseBuffer[0] = '\0';
            return ModemError::BufferTooSmall;
        }
    }
    else
    {
        responseBuffer[0] = '\0';
    }
    return err;
}

// --- Low-Level I/O ---

void SerialModemBase::writeString(const char *str, bool printPrefix)
{
    if (!_uart)
        return;

    size_t len = strlen(str);

    if (printPrefix)
    {
        SM_DEBUG_PRINT("TX: ");
    }
    SM_DEBUG_WRITE(reinterpret_cast<const uint8_t *>(str), len);
    _uart->write(reinterpret_cast<const uint8_t *>(str), len);
    _debugRxNewLine = true;
}

void SerialModemBase::writeData(const uint8_t *data, size_t len)
{
    if (!_uart)
        return;
    SM_DEBUG_WRITE(data, len);
    _uart->write(data, len);
}

uint8_t SerialModemBase::readByte()
{
    if (!_uart)
        return 0;
    int rcv_int = -1;

    if (_oneByteBuf != -1)
    {
        rcv_int = _oneByteBuf;
        _oneByteBuf = -1;
    }
    else if (_uart->available())
    {
        rcv_int = _uart->read();
    }

    if (rcv_int != -1)
    {
        uint8_t rcv = static_cast<uint8_t>(rcv_int);
        if (_debugRxNewLine)
        {
            SM_DEBUG_PRINT("RX: ");
            _debugRxNewLine = false;
        }
        if (rcv >= 32 && rcv <= 126)
        {
            SM_DEBUG_WRITE(rcv);
        }
        else if (rcv == '\r')
        {
            SM_DEBUG_WRITE("<CR>");
        }
        else if (rcv == '\n')
        {
            SM_DEBUG_WRITE("<LF>\n");
            _debugRxNewLine = true;
        }
        else
        {
            char buf[8];
            snprintf(buf, sizeof(buf), "<%02X>", rcv);
            SM_DEBUG_WRITE(buf);
        }
        return rcv;
    }
    return 0;
}

void SerialModemBase::unreadByte(uint8_t c)
{
    _oneByteBuf = c;
}

void SerialModemBase::clearUnreadByte()
{
    _oneByteBuf = -1;
}

void SerialModemBase::flushGarbage(char keepChar)
{
    if (!_uart)
        return;
    SM_DEBUG_PRINT("Flush: Flushing garbage...");

    if (_oneByteBuf != -1)
    {
        if (_oneByteBuf == keepChar)
        {
            SM_DEBUG_WRITE(" Found keep char in buffer.\r\n");
            return;
        }
        _oneByteBuf = -1;
    }

    while (_uart->available())
    {
        int c = _uart->read();
        if (c == keepChar)
        {
            unreadByte(static_cast<uint8_t>(c));
            SM_DEBUG_WRITE(" Found keep char.");
            break;
        }
    }
    SM_DEBUG_WRITE(" Done.\r\n");
}

// --- Timeout Management ---

void SerialModemBase::startTimeout(uint32_t ms)
{
    _bTimeout = false;
    _startTime = millis();
    _timeOutDuration = ms;
}

bool SerialModemBase::isTimeout()
{
    if (!_bTimeout && (millis() - _startTime > _timeOutDuration))
    {
        _bTimeout = true;
    }
    return _bTimeout;
}

// --- Parsing Helpers ---

bool SerialModemBase::parseHex(const uint8_t *pData, size_t len, uint32_t *pResult)
{
    if (!pData || !pResult)
        return false;
    *pResult = 0;
    for (size_t i = 0; i < len; ++i)
    {
        *pResult <<= 4;
        uint8_t c = pData[i];
        if (c >= '0' && c <= '9')
            *pResult |= (c - '0');
        else if (c >= 'a' && c <= 'f')
            *pResult |= (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            *pResult |= (c - 'A' + 10);
        else
        {
            *pResult = 0;
            return false;
        }
    }
    return true;
}

bool SerialModemBase::parseDec(const uint8_t *pData, size_t len, uint32_t *pResult)
{
    if (!pData || !pResult)
        return false;
    *pResult = 0;
    for (size_t i = 0; i < len; ++i)
    {
        *pResult *= 10;
        uint8_t c = pData[i];
        if (c >= '0' && c <= '9')
            *pResult += (c - '0');
        else
        {
            *pResult = 0;
            return false;
        }
    }
    return true;
}

ModemError SerialModemBase::parseResponseHex(const uint8_t *buffer, size_t length, const char *prefix, uint8_t hexDigits, uint32_t *pResult)
{
    if (!buffer || !prefix || !pResult)
        return ModemError::InvalidArg;
    size_t prefixLen = strlen(prefix);

    if (length != prefixLen + hexDigits)
        return ModemError::Fail;
    if (strncmp(reinterpret_cast<const char *>(buffer), prefix, prefixLen) != 0)
        return ModemError::Fail;

    if (parseHex(buffer + prefixLen, hexDigits, pResult))
        return ModemError::Ok;
    return ModemError::Fail;
}

ModemError SerialModemBase::parseResponseDec(const uint8_t *buffer, size_t length, const char *prefix, const char *suffix, size_t suffixLen, int32_t *pResult)
{
    if (!buffer || !prefix || !pResult)
        return ModemError::InvalidArg;
    size_t prefixLen = strlen(prefix);

    if (length <= prefixLen + suffixLen)
        return ModemError::Fail;
    if (strncmp(reinterpret_cast<const char *>(buffer), prefix, prefixLen) != 0)
        return ModemError::Fail;
    if (suffix && suffixLen > 0)
    {
        if (strncmp(reinterpret_cast<const char *>(buffer + length - suffixLen), suffix, suffixLen) != 0)
            return ModemError::Fail;
    }

    const uint8_t *numStart = buffer + prefixLen;
    size_t numLen = length - prefixLen - suffixLen;
    int32_t val = 0;
    bool negative = false;
    size_t i = 0;

    if (numLen > 0 && numStart[0] == '-')
    {
        negative = true;
        i++;
    }
    else if (numLen > 0 && numStart[0] == '+')
    {
        i++;
    }

    if (i >= numLen)
        return ModemError::Fail;

    for (; i < numLen; ++i)
    {
        uint8_t c = numStart[i];
        if (c >= '0' && c <= '9')
            val = val * 10 + (c - '0');
        else
            return ModemError::Fail;
    }

    if (negative)
        val = -val;
    *pResult = val;
    return ModemError::Ok;
}