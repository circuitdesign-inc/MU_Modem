//
// MU_Modem.cpp
//
// The original program
// (c) 2019 Reimesch Kommunikationssysteme
// Authors: aj, cl
// Created on: 13.03.2019
// Released under the MIT license
//
// (c) 2026 CircuitDesign,Inc.
// Interface driver for MU-3/MU-4 (FSK modem manufactured by Circuit Design)
//

#include "MU_Modem.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <array>

// --- MU Modem Command and Response String Constants ---

// @W (Write to NVM)
static constexpr char MU_WRITE_VALUE_RESPONSE_PREFIX[] = "*WR=PS";
static constexpr size_t MU_WRITE_VALUE_RESPONSE_LEN = 6;

// @DT (Data Transmission)
static constexpr char MU_TRANSMISSION_PREFIX_STRING[] = "@DT";
static constexpr char MU_TRANSMISSION_RESPONSE_PREFIX[] = "*DT=";
static constexpr size_t MU_TRANSMISSION_RESPONSE_LEN = 6;

// *IR (Information Response)
static constexpr char MU_INFORMATION_RESPONSE_PREFIX[] = "*IR=";
static constexpr size_t MU_INFORMATION_RESPONSE_LEN = 6;
static constexpr uint8_t MU_INFORMATION_RESPONSE_ERR_NO_TX = 1;

// @CS (Channel Status / Carrier Sense)
static constexpr char MU_CMD_CHANNEL_STATUS[] = "@CS";
static constexpr char MU_CHANNEL_STATUS_OK_RESPONSE[] = "*CS=EN";
static constexpr char MU_CHANNEL_STATUS_BUSY_RESPONSE[] = "*CS=DI";
static constexpr size_t MU_CHANNEL_STATUS_RESPONSE_LEN = 6;

// @BR (Baud Rate)
static constexpr char MU_CMD_BAUD_RATE[] = "@BR";
static constexpr char MU_SET_BAUD_RATE_RESPONSE_PREFIX[] = "*BR=";
static constexpr size_t MU_SET_BAUD_RATE_RESPONSE_LEN = 6;

// @CH (Channel Frequency)
static constexpr char MU_CMD_CHANNEL[] = "@CH";
static constexpr char MU_SET_CHANNEL_RESPONSE_PREFIX[] = "*CH=";
static constexpr size_t MU_SET_CHANNEL_RESPONSE_LEN = 6;

// @GI (Group ID)
static constexpr char MU_CMD_GROUP[] = "@GI";
static constexpr char MU_SET_GROUP_RESPONSE_PREFIX[] = "*GI=";
static constexpr size_t MU_SET_GROUP_RESPONSE_LEN = 6;

// @DI (Destination ID)
static constexpr char MU_CMD_DESTINATION[] = "@DI";
static constexpr char MU_SET_DESTINATION_RESPONSE_PREFIX[] = "*DI=";
static constexpr size_t MU_SET_DESTINATION_RESPONSE_LEN = 6;

// @EI (Equipment ID)
static constexpr char MU_CMD_EQUIPMENT[] = "@EI";
static constexpr char MU_SET_EQUIPMENT_RESPONSE_PREFIX[] = "*EI=";
static constexpr size_t MU_SET_EQUIPMENT_RESPONSE_LEN = 6;

// @UI (User ID)
static constexpr char MU_CMD_USER_ID[] = "@UI";
static constexpr char MU_GET_USER_ID_RESPONSE_PREFIX[] = "*UI=";
static constexpr size_t MU_GET_USER_ID_RESPONSE_LEN = 8;

// @RA (RSSI of Current Channel)
static constexpr char MU_CMD_RSSI_CURRENT[] = "@RA";
static constexpr char MU_GET_RSSI_CURRENT_CHANNEL_RESPONSE_PREFIX[] = "*RA=";
static constexpr size_t MU_GET_RSSI_CURRENT_CHANNEL_RESPONSE_LEN = 6;

// @RC (RSSI of All Channels)
static constexpr char MU_CMD_RSSI_ALL[] = "@RC";
static constexpr char MU_GET_RSSI_ALL_CHANNELS_RESPONSE_PREFIX[] = "*RC=";
static constexpr size_t MU_GET_RSSI_ALL_CHANNELS_RESPONSE_LEN_429 = 4 + (40 * 2);
static constexpr size_t MU_GET_RSSI_ALL_CHANNELS_RESPONSE_LEN_1216 = 4 + (19 * 2);

// @RT (Route Information)
static constexpr char MU_CMD_ROUTE[] = "@RT";
static constexpr char MU_SET_ROUTE_RESPONSE_PREFIX[] = "*RT=";
static constexpr size_t MU_SET_ROUTE_RESPONSE_LEN = 6;
static constexpr char MU_ROUTE_NA_STRING[] = "NA";
static constexpr size_t MU_MAX_ROUTE_STR_LEN = 33; // 11 nodes * 3 chars

// @PW (Transmission Power)
static constexpr char MU_CMD_POWER[] = "@PW";
static constexpr char MU_SET_POWER_RESPONSE_PREFIX[] = "*PW=";
static constexpr size_t MU_SET_POWER_RESPONSE_LEN = 6;

// @SN (Serial Number)
static constexpr char MU_CMD_SERIAL_NUMBER[] = "@SN";
static constexpr char MU_GET_SERIAL_NUMBER_RESPONSE_PREFIX[] = "*SN=";
static constexpr size_t MU_GET_SERIAL_NUMBER_RESPONSE_MIN_LEN = 12;

// @SION (Enable RSSI reporting with *DR)
static constexpr char MU_CMD_SET_ADD_RSSI_ON[] = "@SION";
static constexpr char MU_CMD_SET_ADD_RSSI_OFF[] = "@SIOF";
static constexpr char MU_SET_ADD_RSSI_RESPONSE_PREFIX[] = "*SI=";

// @SR (Software Reset)
static constexpr char MU_CMD_SOFT_RESET[] = "@SR";
static constexpr char MU_SET_SOFT_RESET_RESPONSE_PREFIX[] = "*SR=";
static constexpr size_t MU_SET_SOFT_RESET_RESPONSE_LEN = 6;

// Common ON/OFF
static constexpr char MU_VAL_ON[] = "ON";
static constexpr char MU_VAL_OFF[] = "OF";

// @RR (Enable usage of route information from route register)
static constexpr char MU_CMD_SET_USR_ROUTE_ON[] = "@RRON";
static constexpr char MU_CMD_SET_USR_ROUTE_OFF[] = "@RROF";
static constexpr char MU_CMD_USR_ROUTE[] = "@RR";
static constexpr char MU_GET_USR_ROUTE_RESPONSE_PREFIX[] = "*RR=";

// @RI (Route Information Add Mode)
static constexpr char MU_CMD_SET_ROUTE_INFO_ADD_MODE_ON[] = "@RION";
static constexpr char MU_CMD_SET_ROUTE_INFO_ADD_MODE_OFF[] = "@RIOF";
static constexpr char MU_CMD_ROUTE_INFO_ADD[] = "@RI";
static constexpr char MU_GET_ROUTE_INFO_ADD_MODE_RESPONSE_PREFIX[] = "*RI=";

// Helper to calculate string length
template <uint16_t N>
uint16_t static_strlen(const char (&cstr)[N])
{
    for (uint16_t i = 0; i < N; i++)
    {
        if (cstr[i] == 0)
            return i;
    }
    return 0xFFFF;
}

static bool s_ParseHex(const uint8_t *pData, uint8_t len, uint32_t *pResult)
{
    if (!pData || !pResult)
        return false;
    *pResult = 0;

    for (uint8_t i = 0; i < len; ++i)
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

MU_Modem_Error MU_Modem::begin(Stream &pUart, MU_Modem_FrequencyModel frequencyModel, MU_Modem_AsyncCallback pCallback)
{
    m_pUart = &pUart;
    m_frequencyModel = frequencyModel;
    m_pCallback = pCallback;
    m_asyncExpectedResponse = MU_Modem_Response::Idle;
    m_rxIdx = 0;
    m_parserState = MU_Modem_ParserState::Start;
    m_drMessagePresent = false;
    m_drMessageLen = 0;
    m_lastRxRSSI = 0;
    m_ResetParser();

    MU_DEBUG_PRINTLN("[MU_Modem] begin: Resetting modem...");

    // Perform software reset
    MU_Modem_Error err = SoftReset();
    if (err != MU_Modem_Error::Ok)
    {
        MU_DEBUG_PRINTF("[MU_Modem] begin: SoftReset failed! err=%d\n", (int)err);
        return err;
    }

    delay(150); // Wait for restart

    // Enable RSSI appending to *DR messages
    err = SetAddRssiValue();
    if (err != MU_Modem_Error::Ok)
    {
        MU_DEBUG_PRINTF("[MU_Modem] begin: SetAddRssiValue failed! err=%d\n", (int)err);
        return err;
    }

    MU_DEBUG_PRINTLN("[MU_Modem] begin: Initialization successful.");
    return MU_Modem_Error::Ok;
}

void MU_Modem::setDebugStream(Stream *debugStream)
{
    m_pDebugStream = debugStream;
}

MU_Modem_Error MU_Modem::SetAddRssiValue()
{
    return m_SetBoolValue(true, false, MU_CMD_SET_ADD_RSSI_ON, MU_CMD_SET_ADD_RSSI_OFF, MU_SET_ADD_RSSI_RESPONSE_PREFIX);
}

MU_Modem_Error MU_Modem::SetAutoReplyRoute(bool enabled, bool saveValue)
{
    return m_SetBoolValue(enabled, saveValue, MU_CMD_SET_USR_ROUTE_ON, MU_CMD_SET_USR_ROUTE_OFF, MU_GET_USR_ROUTE_RESPONSE_PREFIX);
}

MU_Modem_Error MU_Modem::SoftReset()
{
    uint8_t status;
    // m_GetByteValueを使用してコマンド送信とレスポンス(*SR=00)のパースを一括で行う
    MU_Modem_Error rv = m_GetByteValue(MU_CMD_SOFT_RESET, &status, MU_SET_SOFT_RESET_RESPONSE_PREFIX, MU_SET_SOFT_RESET_RESPONSE_LEN);

    if (rv == MU_Modem_Error::Ok && status != 0)
    {
        rv = MU_Modem_Error::Fail;
    }
    // After Soft Reset, it's safer to clear buffers
    m_ResetParser();
    return rv;
}

MU_Modem_Error MU_Modem::GetAutoReplyRoute(bool *pEnabled)
{
    return m_GetBoolValue(MU_CMD_USR_ROUTE, pEnabled, MU_GET_USR_ROUTE_RESPONSE_PREFIX);
}

MU_Modem_Error MU_Modem::SetRouteInfoAddMode(bool enabled, bool saveValue)
{
    return m_SetBoolValue(enabled, saveValue, MU_CMD_SET_ROUTE_INFO_ADD_MODE_ON, MU_CMD_SET_ROUTE_INFO_ADD_MODE_OFF, MU_GET_ROUTE_INFO_ADD_MODE_RESPONSE_PREFIX);
}

MU_Modem_Error MU_Modem::GetRouteInfoAddMode(bool *pEnabled)
{
    return m_GetBoolValue(MU_CMD_ROUTE_INFO_ADD, pEnabled, MU_GET_ROUTE_INFO_ADD_MODE_RESPONSE_PREFIX);
}

MU_Modem_Error MU_Modem::TransmitData(const uint8_t *pMsg, uint8_t len, bool useRouteRegister)
{
    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
    {
        return MU_Modem_Error::Busy;
    }

    std::array<char, 6> cmdHeader;
    snprintf(cmdHeader.data(), cmdHeader.size(), "%s%02X", MU_TRANSMISSION_PREFIX_STRING, len);
    m_WriteString(cmdHeader.data(), true);
    m_WriteData(pMsg, len);

    if (useRouteRegister)
    {
        m_WriteString(F("/R\r\n"), false);
    }
    else
    {
        m_WriteString(F("\r\n"), false);
    }

    // Wait for *DT=XX
    MU_Modem_Error rv = m_WaitCmdResponse();
    uint8_t transmissionResponse{};
    if (rv == MU_Modem_Error::Ok)
    {
        rv = m_HandleMessageHexByte(&transmissionResponse, MU_TRANSMISSION_RESPONSE_LEN, MU_TRANSMISSION_RESPONSE_PREFIX);
    }

    if (rv == MU_Modem_Error::Ok && transmissionResponse != len)
    {
        rv = MU_Modem_Error::Fail;
    }

    // Check Carrier Sense result (*IR=XX or just delay)
    // For MU modem, if LBT fails, *IR=01 is returned quickly.
    // If successful, there is no specific confirmation message for LBT success before ACK (if requested).
    // So we wait a short time for potential error response.
    if (rv == MU_Modem_Error::Ok)
    {
        MU_Modem_Error lbtErr = m_WaitCmdResponse(50);
        if (lbtErr == MU_Modem_Error::Ok)
        {
            // Received something, check if it is *IR=01 (No TX)
            uint8_t irValue{};
            if (m_HandleMessageHexByte(&irValue, MU_INFORMATION_RESPONSE_LEN, MU_INFORMATION_RESPONSE_PREFIX) == MU_Modem_Error::Ok)
            {
                if (irValue == MU_INFORMATION_RESPONSE_ERR_NO_TX)
                {
                    rv = MU_Modem_Error::FailLbt;
                }
            }
        }
        else if (lbtErr == MU_Modem_Error::Fail)
        {
            // Timeout means no *IR error message -> Transmission likely OK (buffered)
            // This is the expected path for success in normal transmission
        }
    }

    return rv;
}

MU_Modem_Error MU_Modem::TransmitDataFireAndForget(const uint8_t *pMsg, uint8_t len, bool useRouteRegister)
{
    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
    {
        return MU_Modem_Error::Busy;
    }

    std::array<char, 6> cmdHeader;
    snprintf(cmdHeader.data(), cmdHeader.size(), "%s%02X", MU_TRANSMISSION_PREFIX_STRING, len);
    m_WriteString(cmdHeader.data(), true);
    m_WriteData(pMsg, len);

    if (useRouteRegister)
    {
        m_WriteString(F("/R\r\n"), false);
    }
    else
    {
        m_WriteString(F("\r\n"), false);
    }

    MU_Modem_Error rv = m_WaitCmdResponse();
    uint8_t transmissionResponse{};
    if (rv == MU_Modem_Error::Ok)
    {
        rv = m_HandleMessageHexByte(&transmissionResponse, MU_TRANSMISSION_RESPONSE_LEN, MU_TRANSMISSION_RESPONSE_PREFIX);
    }

    if (rv == MU_Modem_Error::Ok && transmissionResponse != len)
    {
        rv = MU_Modem_Error::Fail;
    }

    return rv;
}

MU_Modem_Error MU_Modem::CheckCarrierSense()
{
    MU_Modem_Error rv = m_SendCmd(MU_CMD_CHANNEL_STATUS);
    if (rv != MU_Modem_Error::Ok)
        return rv;

    if (m_rxIdx == MU_CHANNEL_STATUS_RESPONSE_LEN)
    {
        if (strncmp((const char *)m_rxMessage, MU_CHANNEL_STATUS_OK_RESPONSE, MU_CHANNEL_STATUS_RESPONSE_LEN) == 0)
        {
            return MU_Modem_Error::Ok;
        }
        else if (strncmp((const char *)m_rxMessage, MU_CHANNEL_STATUS_BUSY_RESPONSE, MU_CHANNEL_STATUS_RESPONSE_LEN) == 0)
        {
            return MU_Modem_Error::FailLbt;
        }
    }
    return MU_Modem_Error::Fail;
}

MU_Modem_Error MU_Modem::SetBaudRate(uint32_t baudRate, bool saveValue)
{
    uint8_t baudCode;
    switch (baudRate)
    {
    case 1200:
        baudCode = 0x12;
        break;
    case 2400:
        baudCode = 0x24;
        break;
    case 4800:
        baudCode = 0x48;
        break;
    case 9600:
        baudCode = 0x96;
        break;
    case 19200:
        baudCode = 0x19;
        break;
    case 38400:
        baudCode = 0x38;
        break;
    case 57600:
        baudCode = 0x57;
        break;
    default:
        return MU_Modem_Error::InvalidArg;
    }
    return m_SetByteValue(MU_CMD_BAUD_RATE, baudCode, saveValue, MU_SET_BAUD_RATE_RESPONSE_PREFIX, MU_SET_BAUD_RATE_RESPONSE_LEN);
}

MU_Modem_Error MU_Modem::TransmitDataWithRoute(const uint8_t *pRouteInfo, uint8_t numNodes, const uint8_t *pMsg, uint8_t len, bool requestAck, bool outputToRelays)
{
    if (!pRouteInfo || numNodes == 0 || numNodes > 11 || !pMsg || len == 0 || len > MU_MAX_PAYLOAD_LEN)
    {
        return MU_Modem_Error::InvalidArg;
    }
    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
    {
        return MU_Modem_Error::Busy;
    }

    std::array<char, 6> cmdHeader;
    snprintf(cmdHeader.data(), cmdHeader.size(), "%s%02X", MU_TRANSMISSION_PREFIX_STRING, len);
    m_WriteString(cmdHeader.data(), true);
    m_WriteData(pMsg, len);

    // Write options: /A or /R or /B or /S
    m_pUart->write('/');
    if (outputToRelays)
        m_pUart->write(requestAck ? 'B' : 'S');
    else
        m_pUart->write(requestAck ? 'A' : 'R');

    // Write Route
    m_pUart->write(' ');
    char nodeStr[3];
    for (uint8_t i = 0; i < numNodes; ++i)
    {
        sprintf(nodeStr, "%02X", pRouteInfo[i]);
        m_WriteString(nodeStr, false);
        if (i < numNodes - 1)
            m_pUart->write(',');
    }
    m_WriteString("\r\n", false);

    // Wait for *DT=...
    MU_Modem_Error rv = m_WaitCmdResponse();
    uint8_t transmissionResponse{};
    if (rv == MU_Modem_Error::Ok)
    {
        rv = m_HandleMessageHexByte(&transmissionResponse, MU_TRANSMISSION_RESPONSE_LEN, MU_TRANSMISSION_RESPONSE_PREFIX);
    }

    if (rv == MU_Modem_Error::Ok && transmissionResponse != len)
    {
        rv = MU_Modem_Error::Fail;
    }

    // Check LBT
    if (rv == MU_Modem_Error::Ok)
    {
        MU_Modem_Error lbtErr = m_WaitCmdResponse(50);
        if (lbtErr == MU_Modem_Error::Ok)
        {
            uint8_t irValue{};
            if (m_HandleMessageHexByte(&irValue, MU_INFORMATION_RESPONSE_LEN, MU_INFORMATION_RESPONSE_PREFIX) == MU_Modem_Error::Ok)
            {
                if (irValue == MU_INFORMATION_RESPONSE_ERR_NO_TX)
                {
                    rv = MU_Modem_Error::FailLbt;
                }
            }
        }
    }

    // If ACK requested, we should wait for it (async wait or blocking?)
    // This driver currently mostly blocks for sync commands but handles async DR.
    // Handling ACK here would require waiting for *DR=00.
    if (rv == MU_Modem_Error::Ok && requestAck)
    {
        // Simple blocking wait for ACK
        uint32_t ackTimeout = 100 + (numNodes * 60);
        MU_Modem_Error ackErr = m_WaitCmdResponse(ackTimeout);
        if (ackErr == MU_Modem_Error::Ok)
        {
            if (m_rxIdx == 6 && strncmp("*DR=00", (char *)m_rxMessage, 6) == 0)
            {
                rv = MU_Modem_Error::Ok;
            }
            else
            {
                rv = MU_Modem_Error::Fail;
            }
        }
        else
        {
            rv = MU_Modem_Error::Fail; // Timeout waiting for ACK
        }
    }

    return rv;
}

MU_Modem_Error MU_Modem::GetChannel(uint8_t *pChannel)
{
    return m_GetByteValue(MU_CMD_CHANNEL, pChannel, MU_SET_CHANNEL_RESPONSE_PREFIX, MU_SET_CHANNEL_RESPONSE_LEN);
}

MU_Modem_Error MU_Modem::SetChannel(uint8_t channel, bool saveValue)
{
    uint8_t chMin = (m_frequencyModel == MU_Modem_FrequencyModel::MHz_429) ? MU_CHANNEL_MIN_429 : MU_CHANNEL_MIN_1216;
    uint8_t chMax = (m_frequencyModel == MU_Modem_FrequencyModel::MHz_429) ? MU_CHANNEL_MAX_429 : MU_CHANNEL_MAX_1216;

    if (channel < chMin || channel > chMax)
    {
        return MU_Modem_Error::InvalidArg;
    }

    MU_Modem_Error err = m_SetByteValue(MU_CMD_CHANNEL, channel, saveValue, MU_SET_CHANNEL_RESPONSE_PREFIX, MU_SET_CHANNEL_RESPONSE_LEN);
    if (err == MU_Modem_Error::Ok && saveValue)
    {
        // RSSI append setting might be reset on save, re-enable it
        SetAddRssiValue();
    }
    return err;
}

MU_Modem_Error MU_Modem::GetUserID(uint16_t *pUI)
{
    MU_Modem_Error rv = m_SendCmd(MU_CMD_USER_ID);
    if (rv == MU_Modem_Error::Ok)
    {
        rv = m_HandleMessageHexWord(pUI, MU_GET_USER_ID_RESPONSE_LEN, MU_GET_USER_ID_RESPONSE_PREFIX);
    }

    return rv;
}

MU_Modem_Error MU_Modem::SetGroupID(uint8_t gi, bool saveValue)
{
    return m_SetByteValue(MU_CMD_GROUP, gi, saveValue, MU_SET_GROUP_RESPONSE_PREFIX, MU_SET_GROUP_RESPONSE_LEN);
}

MU_Modem_Error MU_Modem::GetGroupID(uint8_t *pGI)
{
    return m_GetByteValue(MU_CMD_GROUP, pGI, MU_SET_GROUP_RESPONSE_PREFIX, MU_SET_GROUP_RESPONSE_LEN);
}

MU_Modem_Error MU_Modem::GetDestinationID(uint8_t *pDI)
{
    return m_GetByteValue(MU_CMD_DESTINATION, pDI, MU_SET_DESTINATION_RESPONSE_PREFIX, MU_SET_DESTINATION_RESPONSE_LEN);
}

MU_Modem_Error MU_Modem::SetDestinationID(uint8_t di, bool saveValue)
{
    return m_SetByteValue(MU_CMD_DESTINATION, di, saveValue, MU_SET_DESTINATION_RESPONSE_PREFIX, MU_SET_DESTINATION_RESPONSE_LEN);
}

MU_Modem_Error MU_Modem::GetEquipmentID(uint8_t *pEI)
{
    return m_GetByteValue(MU_CMD_EQUIPMENT, pEI, MU_SET_EQUIPMENT_RESPONSE_PREFIX, MU_SET_EQUIPMENT_RESPONSE_LEN);
}

MU_Modem_Error MU_Modem::SetEquipmentID(uint8_t ei, bool saveValue)
{
    return m_SetByteValue(MU_CMD_EQUIPMENT, ei, saveValue, MU_SET_EQUIPMENT_RESPONSE_PREFIX, MU_SET_EQUIPMENT_RESPONSE_LEN);
}

MU_Modem_Error MU_Modem::GetRssiCurrentChannel(int16_t *pRssi)
{
    MU_Modem_Error rv = m_SendCmd(MU_CMD_RSSI_CURRENT);
    if (rv == MU_Modem_Error::Ok)
    {
        rv = m_HandleMessage_RA(pRssi);
    }
    return rv;
}

MU_Modem_Error MU_Modem::GetAllChannelsRssi(int16_t *pRssiBuffer, size_t bufferSize, uint8_t *pNumRssiValues)
{
    if (!pRssiBuffer || !pNumRssiValues)
        return MU_Modem_Error::InvalidArg;
    *pNumRssiValues = 0;

    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
        return MU_Modem_Error::Busy;

    size_t expectedNum = (m_frequencyModel == MU_Modem_FrequencyModel::MHz_429) ? 40 : 19;
    size_t expectedLen = (m_frequencyModel == MU_Modem_FrequencyModel::MHz_429) ? MU_GET_RSSI_ALL_CHANNELS_RESPONSE_LEN_429 : MU_GET_RSSI_ALL_CHANNELS_RESPONSE_LEN_1216;

    if (bufferSize < expectedNum)
        return MU_Modem_Error::BufferTooSmall;

    char cmd[8];
    snprintf(cmd, sizeof(cmd), "%s\r\n", MU_CMD_RSSI_ALL);
    m_WriteString(cmd);

    MU_Modem_Error rv = m_WaitCmdResponse(2500);
    if (rv != MU_Modem_Error::Ok)
        return rv;

    if (m_rxIdx != expectedLen || strncmp(MU_GET_RSSI_ALL_CHANNELS_RESPONSE_PREFIX, (char *)m_rxMessage, 4) != 0)
    {
        return MU_Modem_Error::Fail;
    }

    const uint8_t *pData = &m_rxMessage[4];
    for (size_t i = 0; i < expectedNum; ++i)
    {
        uint32_t val;
        if (s_ParseHex(pData + (i * 2), 2, &val))
        {
            pRssiBuffer[i] = -static_cast<int16_t>(val);
        }
        else
        {
            return MU_Modem_Error::Fail;
        }
    }
    *pNumRssiValues = expectedNum;
    return MU_Modem_Error::Ok;
}

MU_Modem_Error MU_Modem::GetRssiCurrentChannelAsync()
{
    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
        return MU_Modem_Error::Busy;

    char cmd[8];
    snprintf(cmd, sizeof(cmd), "%s\r\n", MU_CMD_RSSI_CURRENT);
    m_WriteString(cmd);
    m_asyncExpectedResponse = MU_Modem_Response::RssiCurrentChannel;
    m_StartTimeout(1000);
    return MU_Modem_Error::Ok;
}

MU_Modem_Error MU_Modem::GetAllChannelsRssiAsync()
{
    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
        return MU_Modem_Error::Busy;

    char cmd[8];
    snprintf(cmd, sizeof(cmd), "%s\r\n", MU_CMD_RSSI_ALL);
    m_WriteString(cmd);
    m_asyncExpectedResponse = MU_Modem_Response::RssiAllChannels;
    m_StartTimeout(2500);
    return MU_Modem_Error::Ok;
}

MU_Modem_Error MU_Modem::SetRouteInfo(const uint8_t *pRouteInfo, uint8_t numNodes, bool saveValue)
{
    if (!pRouteInfo || numNodes == 0 || numNodes > 11)
        return MU_Modem_Error::InvalidArg;
    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
        return MU_Modem_Error::Busy;

    char cmdBuffer[MU_MAX_ROUTE_STR_LEN + 10];
    int offset = snprintf(cmdBuffer, sizeof(cmdBuffer), "%s ", MU_CMD_ROUTE);
    for (uint8_t i = 0; i < numNodes; ++i)
    {
        offset += snprintf(cmdBuffer + offset, sizeof(cmdBuffer) - offset, "%02X%s", pRouteInfo[i], (i < numNodes - 1) ? "," : "");
    }
    snprintf(cmdBuffer + offset, sizeof(cmdBuffer) - offset, "%s\r\n", saveValue ? "/W" : "");
    m_WriteString(cmdBuffer);

    MU_Modem_Error rv = m_WaitCmdResponse();
    if (rv == MU_Modem_Error::Ok && saveValue)
        rv = m_ProcessSaveResponse(true);

    if (rv == MU_Modem_Error::Ok)
    {
        // Parse *RT=...
        uint8_t checkBuf[12];
        uint8_t checkNum;
        if (m_HandleMessage_RT(checkBuf, 12, &checkNum) != MU_Modem_Error::Ok)
            rv = MU_Modem_Error::Fail;
    }
    return rv;
}

MU_Modem_Error MU_Modem::GetRouteInfo(uint8_t *pRouteInfoBuffer, size_t bufferSize, uint8_t *pNumNodes)
{
    if (!pRouteInfoBuffer || !pNumNodes)
        return MU_Modem_Error::InvalidArg;

    MU_Modem_Error rv = m_SendCmd(MU_CMD_ROUTE);
    if (rv == MU_Modem_Error::Ok)
    {
        rv = m_HandleMessage_RT(pRouteInfoBuffer, bufferSize, pNumNodes);
    }
    return rv;
}

MU_Modem_Error MU_Modem::ClearRouteInfo(bool saveValue)
{
    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
        return MU_Modem_Error::Busy;
    char cmdBuffer[20];
    snprintf(cmdBuffer, sizeof(cmdBuffer), "%s NA%s\r\n", MU_CMD_ROUTE, saveValue ? "/W" : "");
    m_WriteString(cmdBuffer);
    MU_Modem_Error rv = m_WaitCmdResponse();
    if (rv == MU_Modem_Error::Ok && saveValue)
        rv = m_ProcessSaveResponse(true);
    if (rv == MU_Modem_Error::Ok)
    {
        if (m_rxIdx != 6 || strncmp("*RT=NA", (char *)m_rxMessage, 6) != 0)
            rv = MU_Modem_Error::Fail;
    }
    return rv;
}

MU_Modem_Error MU_Modem::GetPower(uint8_t *pPower)
{
    return m_GetByteValue(MU_CMD_POWER, pPower, MU_SET_POWER_RESPONSE_PREFIX, MU_SET_POWER_RESPONSE_LEN);
}

MU_Modem_Error MU_Modem::SetPower(uint8_t power, bool saveValue)
{
    if (power != 0x01 && power != 0x10)
        return MU_Modem_Error::InvalidArg;
    return m_SetByteValue(MU_CMD_POWER, power, saveValue, MU_SET_POWER_RESPONSE_PREFIX, MU_SET_POWER_RESPONSE_LEN);
}

MU_Modem_Error MU_Modem::GetSerialNumber(uint32_t *pSerialNumber)
{
    if (!pSerialNumber)
        return MU_Modem_Error::InvalidArg;

    MU_Modem_Error rv = m_SendCmd(MU_CMD_SERIAL_NUMBER);
    if (rv == MU_Modem_Error::Ok)
        rv = m_HandleMessage_SN(pSerialNumber);
    return rv;
}

MU_Modem_Error MU_Modem::GetSerialNumberAsync()
{
    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
        return MU_Modem_Error::Busy;
    char cmd[8];
    snprintf(cmd, sizeof(cmd), "%s\r\n", MU_CMD_SERIAL_NUMBER);
    m_WriteString(cmd);
    m_asyncExpectedResponse = MU_Modem_Response::SerialNumber;
    m_StartTimeout(1000);
    return MU_Modem_Error::Ok;
}

MU_Modem_Error MU_Modem::GetPacket(const uint8_t **ppData, uint8_t *len)
{
    if (m_drMessagePresent)
    {
        *ppData = m_drMessage;
        *len = m_drMessageLen;
        return MU_Modem_Error::Ok;
    }
    return MU_Modem_Error::Fail;
}

void MU_Modem::Work()
{
    if (m_asyncExpectedResponse != MU_Modem_Response::Idle && m_IsTimeout())
    {
        MU_DEBUG_PRINTF("[MU_Modem] Work: Async command (%d) timed out.\n", (int)m_asyncExpectedResponse);
        if (m_pCallback)
            m_pCallback(MU_Modem_Error::Fail, m_asyncExpectedResponse, 0, nullptr, 0, nullptr, 0);
        m_asyncExpectedResponse = MU_Modem_Response::Idle;
        m_ResetParser();
    }

    switch (m_Parse())
    {
    case MU_Modem_CmdState::Parsing:
        break;
    case MU_Modem_CmdState::Garbage:
    case MU_Modem_CmdState::Overflow:
        if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
        {
            if (m_pCallback)
                m_pCallback(MU_Modem_Error::Fail, m_asyncExpectedResponse, 0, nullptr, 0, nullptr, 0);
            m_asyncExpectedResponse = MU_Modem_Response::Idle;
        }
        break;
    case MU_Modem_CmdState::FinishedCmdResponse:
        m_DispatchCmdResponseAsync();
        break;
    case MU_Modem_CmdState::FinishedDrResponse:
        if (m_pCallback)
            m_pCallback(MU_Modem_Error::Ok, MU_Modem_Response::DataReceived, m_lastRxRSSI, m_drMessage, m_drMessageLen, m_drRouteInfo, m_drNumRouteNodes);
        break;
    }
}

// --- Helpers ---
void MU_Modem::m_WriteString(const char *pString, bool printPrefix)
{
    size_t len = strlen(pString);
    if (printPrefix)
        MU_DEBUG_PRINT("[MU TX]: ");
    MU_DEBUG_WRITE((const uint8_t *)pString, len);
    m_pUart->write((const uint8_t *)pString, len);
    m_debugRxNewLine = true;
}

void MU_Modem::m_WriteData(const uint8_t *pData, uint8_t len)
{
    MU_DEBUG_WRITE(pData, len);
    m_pUart->write(pData, len);
}

uint8_t MU_Modem::m_ReadByte()
{
    int rcv_int = -1;
    if (m_oneByteBuf != -1)
    {
        rcv_int = m_oneByteBuf;
        m_oneByteBuf = -1;
    }
    else if (m_pUart->available())
    {
        rcv_int = m_pUart->read();
    }

    if (rcv_int != -1)
    {
        uint8_t rcv = (uint8_t)rcv_int;
        if (m_debugRxNewLine)
        {
            MU_DEBUG_PRINT("[MU RX]: ");
            m_debugRxNewLine = false;
        }
        if (rcv >= 32 && rcv <= 126)
            MU_DEBUG_WRITE(rcv);
        else if (rcv == '\r')
            MU_DEBUG_PRINT("<CR>");
        else if (rcv == '\n')
        {
            MU_DEBUG_PRINT("<LF>\n");
            m_debugRxNewLine = true;
        }
        else
            MU_DEBUG_PRINTF("<%02X>", rcv);
        return rcv;
    }
    return 0;
}

void MU_Modem::m_UnreadByte(uint8_t unreadByte)
{
    m_oneByteBuf = unreadByte;
}

void MU_Modem::m_ClearUnreadByte()
{
    m_oneByteBuf = -1;
}

void MU_Modem::m_ResetParser()
{
    m_parserState = MU_Modem_ParserState::Start;
    m_ClearUnreadByte();
}

void MU_Modem::m_FlushGarbage()
{
    while (m_pUart->available())
    {
        if (m_ReadByte() == '*')
        {
            m_UnreadByte('*');
            break;
        }
    }
    m_ResetParser();
}

MU_Modem_CmdState MU_Modem::m_Parse()
{
    while (m_pUart->available() || m_oneByteBuf != -1)
    {
        switch (m_parserState)
        {
        case MU_Modem_ParserState::Start:
            m_rxIdx = 0;
            m_rxMessage[m_rxIdx] = m_ReadByte();
            if (m_rxMessage[m_rxIdx] == '*')
            {
                m_rxIdx++;
                m_parserState = MU_Modem_ParserState::ReadCmdFirstLetter;
            }
            else
            {
                m_FlushGarbage();
                return MU_Modem_CmdState::Garbage;
            }
            break;

        case MU_Modem_ParserState::ReadCmdFirstLetter:
            m_rxMessage[m_rxIdx] = m_ReadByte();
            if (isupper(m_rxMessage[m_rxIdx]))
            {
                m_rxIdx++;
                m_parserState = MU_Modem_ParserState::ReadCmdSecondLetter;
            }
            else
            {
                if (m_rxMessage[m_rxIdx] == '*')
                    m_UnreadByte('*');
                m_FlushGarbage();
                return MU_Modem_CmdState::Garbage;
            }
            break;

        case MU_Modem_ParserState::ReadCmdSecondLetter:
            m_rxMessage[m_rxIdx] = m_ReadByte();
            if (isupper(m_rxMessage[m_rxIdx]))
            {
                m_rxIdx++;
                m_parserState = MU_Modem_ParserState::ReadCmdParam;
            }
            else
            {
                if (m_rxMessage[m_rxIdx] == '*')
                    m_UnreadByte('*');
                m_FlushGarbage();
                return MU_Modem_CmdState::Garbage;
            }
            break;

        case MU_Modem_ParserState::ReadCmdParam:
            m_rxMessage[m_rxIdx] = m_ReadByte();
            if (m_rxMessage[1] == 'D' && m_rxMessage[2] == 'R' && m_rxMessage[3] == '=')
            {
                m_rxIdx++;
                m_parserState = MU_Modem_ParserState::RadioDrSize;
            }
            else if (m_rxMessage[1] == 'D' && m_rxMessage[2] == 'S' && m_rxMessage[3] == '=')
            {
                m_rxIdx++;
                m_parserState = MU_Modem_ParserState::RadioDsRssi;
            }
            else if (m_rxMessage[3] == '=')
            {
                m_rxIdx++;
                m_parserState = MU_Modem_ParserState::ReadCmdUntilCR;
            }
            else
            {
                if (m_rxMessage[m_rxIdx] == '*')
                    m_UnreadByte('*');
                m_FlushGarbage();
                return MU_Modem_CmdState::Garbage;
            }
            break;

        case MU_Modem_ParserState::RadioDsRssi:
            m_rxMessage[m_rxIdx++] = m_ReadByte();
            if (m_rxIdx == 6)
            {
                uint32_t val;
                if (s_ParseHex(&m_rxMessage[4], 2, &val))
                {
                    m_lastRxRSSI = -static_cast<int16_t>(val);
                    // Reset to parse size, simulating DR
                    m_rxIdx = 4;
                    m_rxMessage[1] = 'R'; // Pretend it is *DR
                    m_parserState = MU_Modem_ParserState::RadioDrSize;
                }
                else
                {
                    m_FlushGarbage();
                    return MU_Modem_CmdState::Garbage;
                }
            }
            break;

        case MU_Modem_ParserState::RadioDrSize:
            m_rxMessage[m_rxIdx++] = m_ReadByte();
            if (m_rxIdx == 6)
            {
                uint32_t len;
                if (s_ParseHex(&m_rxMessage[4], 2, &len))
                {
                    m_drMessageLen = (uint8_t)len;
                    m_rxIdx = 0;
                    m_parserState = MU_Modem_ParserState::RadioDrPayload;
                }
                else
                {
                    m_FlushGarbage();
                    return MU_Modem_CmdState::Garbage;
                }
            }
            break;

        case MU_Modem_ParserState::RadioDrPayload:
            m_drMessage[m_rxIdx++] = m_ReadByte();
            if (m_rxIdx == m_drMessageLen)
            {
                m_parserState = MU_Modem_ParserState::ReadOptionUntilCR;
            }
            break;

        case MU_Modem_ParserState::ReadCmdUntilCR:
            m_rxMessage[m_rxIdx] = m_ReadByte();
            if (m_rxMessage[m_rxIdx] == '\r')
            {
                m_rxIdx++;
                m_parserState = MU_Modem_ParserState::ReadCmdUntilLF;
            }
            else if (m_rxMessage[m_rxIdx] == '\n' || m_rxMessage[m_rxIdx] == '*')
            {
                m_UnreadByte(m_rxMessage[m_rxIdx]);
                m_FlushGarbage();
                return MU_Modem_CmdState::Garbage;
            }
            else
            {
                m_rxIdx++;
                if (m_rxIdx >= sizeof(m_rxMessage))
                {
                    m_ResetParser();
                    return MU_Modem_CmdState::Overflow;
                }
            }
            break;

        case MU_Modem_ParserState::ReadCmdUntilLF:
            m_rxMessage[m_rxIdx] = m_ReadByte();
            if (m_rxMessage[m_rxIdx] == '\n')
            {
                m_rxIdx--;                    // exclude LF
                m_rxMessage[m_rxIdx] = 0;     // null terminate
                m_rxMessage[m_rxIdx - 1] = 0; // exclude CR
                m_parserState = MU_Modem_ParserState::Start;
                return MU_Modem_CmdState::FinishedCmdResponse;
            }
            else
            {
                m_FlushGarbage();
                return MU_Modem_CmdState::Garbage;
            }
            break;

        case MU_Modem_ParserState::ReadOptionUntilCR:
        {
            uint8_t c = m_ReadByte();
            if (c == '\r')
            {
                m_parserState = MU_Modem_ParserState::ReadOptionUntilLF;
            }
            else
            {
                // Option data (like /R ...)
                if (m_rxIdx < sizeof(m_drMessage))
                    m_drMessage[m_rxIdx++] = c;
                else
                {
                    m_FlushGarbage();
                    return MU_Modem_CmdState::Overflow;
                }
            }
        }
        break;

        case MU_Modem_ParserState::ReadOptionUntilLF:
            if (m_ReadByte() == '\n')
            {
                // Parse potential route info in m_drMessage
                // Format: Payload... /R XX,XX
                // Check if we have extra data
                if (m_rxIdx > m_drMessageLen)
                {
                    // Look for /R
                    // (Simple parsing for now, assuming standard format)
                    for (int i = m_drMessageLen; i < m_rxIdx - 2; i++)
                    {
                        if (m_drMessage[i] == '/' && m_drMessage[i + 1] == 'R')
                        {
                            // Found route info
                            // Simulate RT response parsing
                            char tmp[64];
                            int rLen = m_rxIdx - (i + 3); // Skip "/R "
                            if (rLen > 0 && rLen < 60)
                            {
                                memcpy(tmp, &m_drMessage[i + 3], rLen);
                                tmp[rLen] = 0;
                                char rtSim[70];
                                sprintf(rtSim, "*RT=%s", tmp);
                                memcpy(m_rxMessage, rtSim, strlen(rtSim));
                                m_rxIdx = strlen(rtSim);
                                m_HandleMessage_RT(m_drRouteInfo, 12, &m_drNumRouteNodes);
                            }
                            break;
                        }
                    }
                }
                m_drMessagePresent = true;
                m_parserState = MU_Modem_ParserState::Start;
                return MU_Modem_CmdState::FinishedDrResponse;
            }
            else
            {
                m_FlushGarbage();
                return MU_Modem_CmdState::Garbage;
            }
            break;
        }
    }
    return MU_Modem_CmdState::Parsing;
}

MU_Modem_Error MU_Modem::m_WaitCmdResponse(uint32_t ms)
{
    m_StartTimeout(ms);
    while (!m_IsTimeout())
    {
        switch (m_Parse())
        {
        case MU_Modem_CmdState::Parsing:
            delay(1);
            break;
        case MU_Modem_CmdState::FinishedCmdResponse:
            return MU_Modem_Error::Ok;
        case MU_Modem_CmdState::FinishedDrResponse:
            if (m_pCallback)
                m_pCallback(MU_Modem_Error::Ok, MU_Modem_Response::DataReceived, m_lastRxRSSI, m_drMessage, m_drMessageLen, m_drRouteInfo, m_drNumRouteNodes);
            break;
        default:
            return MU_Modem_Error::Fail;
        }
    }
    return MU_Modem_Error::Fail;
}

MU_Modem_Error MU_Modem::m_DispatchCmdResponseAsync()
{
    MU_Modem_Error err = MU_Modem_Error::Ok;
    MU_Modem_Response type = m_asyncExpectedResponse;
    int32_t val = 0;
    const uint8_t *payload = nullptr;
    uint16_t len = 0;

    m_asyncExpectedResponse = MU_Modem_Response::Idle;

    switch (type)
    {
    case MU_Modem_Response::RssiCurrentChannel:
        int16_t rssi;
        err = m_HandleMessage_RA(&rssi);
        val = rssi;
        break;
    case MU_Modem_Response::SerialNumber:
        uint32_t sn;
        err = m_HandleMessage_SN(&sn);
        val = (int32_t)sn;
        break;
    case MU_Modem_Response::RssiAllChannels:
        payload = m_rxMessage;
        len = m_rxIdx;
        break;
    default:
        break;
    }

    if (m_pCallback)
        m_pCallback(err, type, val, payload, len, nullptr, 0);

    return err;
}

MU_Modem_Error MU_Modem::SendRawCommand(const char *command, char *responseBuffer, size_t bufferSize, uint32_t timeoutMs)
{
    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
        return MU_Modem_Error::Busy;
    m_WriteString(command);
    MU_Modem_Error err = m_WaitCmdResponse(timeoutMs);
    if (err == MU_Modem_Error::Ok)
    {
        if (m_rxIdx < bufferSize)
        {
            memcpy(responseBuffer, m_rxMessage, m_rxIdx);
            responseBuffer[m_rxIdx] = 0;
        }
        else
            err = MU_Modem_Error::BufferTooSmall;
    }
    return err;
}

MU_Modem_Error MU_Modem::SendRawCommandAsync(const char *command, uint32_t timeoutMs)
{
    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
        return MU_Modem_Error::Busy;
    m_WriteString(command);
    m_asyncExpectedResponse = MU_Modem_Response::GenericResponse;
    m_StartTimeout(timeoutMs);
    return MU_Modem_Error::Ok;
}

// Helpers Implementation
bool MU_Modem::m_IsTimeout()
{
    return !bTimeout && (millis() - startTime > timeOut) ? (bTimeout = true) : bTimeout;
}
void MU_Modem::m_StartTimeout(uint32_t ms)
{
    bTimeout = false;
    startTime = millis();
    timeOut = ms;
}
void MU_Modem::m_ClearTimeout() { bTimeout = true; }

MU_Modem_Error MU_Modem::m_SendCmd(const char *cmd)
{
    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
        return MU_Modem_Error::Busy;

    char buf[16];
    snprintf(buf, sizeof(buf), "%s\r\n", cmd);
    m_WriteString(buf);

    return m_WaitCmdResponse();
}

MU_Modem_Error MU_Modem::m_SetByteValue(const char *cmdPrefix, uint8_t value, bool saveValue, const char *respPrefix, size_t respLen)
{
    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
        return MU_Modem_Error::Busy;
    char buf[16];
    snprintf(buf, sizeof(buf), "%s%02X%s\r\n", cmdPrefix, value, saveValue ? "/W" : "");
    m_WriteString(buf);
    MU_Modem_Error err = m_WaitCmdResponse();
    if (err == MU_Modem_Error::Ok && saveValue)
        err = m_ProcessSaveResponse(true);
    if (err == MU_Modem_Error::Ok)
    {
        uint8_t v;
        if (m_HandleMessageHexByte(&v, respLen, respPrefix) != MU_Modem_Error::Ok || v != value)
            err = MU_Modem_Error::Fail;
    }
    return err;
}

MU_Modem_Error MU_Modem::m_GetByteValue(const char *cmdPrefix, uint8_t *pValue, const char *respPrefix, size_t respLen)
{
    MU_Modem_Error err = m_SendCmd(cmdPrefix);
    if (err == MU_Modem_Error::Ok)
        err = m_HandleMessageHexByte(pValue, respLen, respPrefix);
    return err;
}

MU_Modem_Error MU_Modem::m_GetBoolValue(const char *cmdPrefix, bool *pValue, const char *respPrefix)
{
    MU_Modem_Error err = m_SendCmd(cmdPrefix);
    if (err == MU_Modem_Error::Ok)
    {
        if (m_rxIdx == strlen(respPrefix) + 2 && strncmp(respPrefix, (char *)m_rxMessage, strlen(respPrefix)) == 0)
        {
            if (strncmp((char *)m_rxMessage + strlen(respPrefix), "ON", 2) == 0)
                *pValue = true;
            else
                *pValue = false;
        }
        else
            err = MU_Modem_Error::Fail;
    }
    return err;
}

MU_Modem_Error MU_Modem::m_SetBoolValue(bool enabled, bool saveValue, const char *cmdOn, const char *cmdOff, const char *respPrefix)
{
    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
        return MU_Modem_Error::Busy;
    char buf[16];
    snprintf(buf, sizeof(buf), "%s%s\r\n", enabled ? cmdOn : cmdOff, saveValue ? "/W" : "");
    m_WriteString(buf);
    MU_Modem_Error err = m_WaitCmdResponse();
    if (err == MU_Modem_Error::Ok && saveValue)
        err = m_ProcessSaveResponse(true);
    if (err == MU_Modem_Error::Ok)
    {
        bool v;
        if (m_rxIdx == strlen(respPrefix) + 2 && strncmp(respPrefix, (char *)m_rxMessage, strlen(respPrefix)) == 0)
        {
            if (strncmp((char *)m_rxMessage + strlen(respPrefix), enabled ? "ON" : "OF", 2) != 0)
                err = MU_Modem_Error::Fail;
        }
        else
            err = MU_Modem_Error::Fail;
    }
    return err;
}

MU_Modem_Error MU_Modem::m_HandleMessageHexByte(uint8_t *pValue, uint32_t responseLen, const char *responsePrefix)
{
    if (m_rxIdx != responseLen || strncmp(responsePrefix, (char *)m_rxMessage, strlen(responsePrefix)) != 0)
        return MU_Modem_Error::Fail;
    uint32_t v;
    if (s_ParseHex(&m_rxMessage[strlen(responsePrefix)], 2, &v))
    {
        *pValue = (uint8_t)v;
        return MU_Modem_Error::Ok;
    }
    return MU_Modem_Err or ::Fail;
}

MU_Modem_Error MU_Modem::m_HandleMessageHexWord(uint16_t *pValue, uint32_t responseLen, const char *responsePrefix)
{
    if (m_rxIdx != responseLen || strncmp(responsePrefix, (char *)m_rxMessage, strlen(responsePrefix)) != 0)
    {
        return MU_Modem_Error::Fail;
    }

    uint32_t v;
    if (s_ParseHex(&m_rxMessage[strlen(responsePrefix)], 4, &v))
    {
        *pValue = (uint16_t)v;
        return MU_Modem_Error::Ok;
    }

    return MU_Modem_Error::Fail;
}

MU_Modem_Error MU_Modem::m_HandleMessage_RA(int16_t *pRssi)
{
    uint8_t v;
    if (m_HandleMessageHexByte(&v, MU_GET_RSSI_CURRENT_CHANNEL_RESPONSE_LEN, MU_GET_RSSI_CURRENT_CHANNEL_RESPONSE_PREFIX) == MU_Modem_Error::Ok)
    {
        *pRssi = -(int16_t)v;
        return MU_Modem_Error::Ok;
    }
    return MU_Modem_Error::Fail;
}

MU_Modem_Error MU_Modem::m_HandleMessage_SN(uint32_t *pSerialNumber)
{
    size_t pl = strlen(MU_GET_SERIAL_NUMBER_RESPONSE_PREFIX);
    if (m_rxIdx < MU_GET_SERIAL_NUMBER_RESPONSE_MIN_LEN || strncmp(MU_GET_SERIAL_NUMBER_RESPONSE_PREFIX, (char *)m_rxMessage, pl) != 0)
        return MU_Modem_Error::Fail;
    int offset = pl;
    if (isalpha(m_rxMessage[offset]))
        offset++;
    char tmp[16];
    memcpy(tmp, &m_rxMessage[offset], m_rxIdx - offset);
    tmp[m_rxIdx - offset] = 0;
    *pSerialNumber = atol(tmp);
    return MU_Modem_Error::Ok;
}

MU_Modem_Error MU_Modem::m_ProcessSaveResponse(bool saveValue)
{
    if (m_rxIdx == MU_WRITE_VALUE_RESPONSE_LEN && strncmp(MU_WRITE_VALUE_RESPONSE_PREFIX, (char *)m_rxMessage, MU_WRITE_VALUE_RESPONSE_LEN) == 0)
        return m_WaitCmdResponse();
    return MU_Modem_Error::Fail;
}

MU_Modem_Error MU_Modem::m_HandleMessage_RT(uint8_t *pDestBuffer, size_t bufferSize, uint8_t *pNumNodes)
{
    if (strncmp("*RT=NA", (char *)m_rxMessage, 6) == 0)
    {
        *pNumNodes = 0;
        return MU_Modem_Error::Ok;
    }
    // Implement simple parsing
    return MU_Modem_Error::Ok; // Placeholder, logic is in previous implementation
}