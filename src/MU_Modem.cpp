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

// @DT (Data Transmission)
static constexpr char MU_TRANSMISSION_PREFIX_STRING[] = "@DT";
static constexpr char MU_TRANSMISSION_RESPONSE_PREFIX[] = "*DT=";
// *IR (Information Response - used for LBT Error)
static constexpr char MU_INFORMATION_RESPONSE_PREFIX[] = "*IR=";

// *DR, *DS, *DC (Data Reception)
static constexpr char MU_RECEPTION_PREFIX_DR[] = "*DR=";
static constexpr char MU_RECEPTION_PREFIX_DS[] = "*DS=";
static constexpr char MU_RECEPTION_PREFIX_DC[] = "*DC=";
static constexpr char MU_LBT_ERROR_RESPONSE[] = "*IR=01";

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
static constexpr char MU_ROUTE_NA_RESPONSE[] = "*RT=NA";
static constexpr size_t MU_MAX_ROUTE_STR_LEN = 33; // 11 nodes * 3 chars

// @PW (Transmission Power)
static constexpr char MU_CMD_POWER[] = "@PW";
static constexpr char MU_SET_POWER_RESPONSE_PREFIX[] = "*PW=";
static constexpr size_t MU_SET_POWER_RESPONSE_LEN = 6;

// @SN (Serial Number)
static constexpr char MU_CMD_SERIAL_NUMBER[] = "@SN";
static constexpr char MU_GET_SERIAL_NUMBER_RESPONSE_PREFIX[] = "*SN=";
static constexpr size_t MU_GET_SERIAL_NUMBER_RESPONSE_MIN_LEN = 12;

// @SI (Enable RSSI reporting with *DR)
static constexpr char MU_CMD_ADD_RSSI[] = "@SI";
static constexpr char MU_SET_ADD_RSSI_RESPONSE_PREFIX[] = "*SI=";

// *ER (Error Response)
static constexpr char MU_ERROR_RESPONSE_PREFIX[] = "*ER=";
static constexpr size_t MU_ERROR_RESPONSE_PREFIX_LEN = 4;

// @SR (Software Reset)
static constexpr char MU_CMD_SOFT_RESET[] = "@SR";
static constexpr char MU_SET_SOFT_RESET_RESPONSE_PREFIX[] = "*SR=";
static constexpr size_t MU_SET_SOFT_RESET_RESPONSE_LEN = 6;

// @RR (Enable usage of route information from route register)
static constexpr char MU_CMD_USR_ROUTE[] = "@RR";
static constexpr char MU_GET_USR_ROUTE_RESPONSE_PREFIX[] = "*RR=";

// @RI (Route Information Add Mode)
static constexpr char MU_CMD_ROUTE_INFO_ADD[] = "@RI";
static constexpr char MU_GET_ROUTE_INFO_ADD_MODE_RESPONSE_PREFIX[] = "*RI=";

// Route Information Option in *DR/*DS/*DC
static constexpr char MU_ROUTE_INFO_OPTION_PREFIX[] = "/R";

// --- Constants for Parser ---
static constexpr size_t MU_DR_PREFIX_LEN = 4;     // "*DR="
static constexpr size_t MU_DS_PREFIX_LEN = 4;     // "*DS="
static constexpr size_t MU_DR_SIZE_TOTAL_LEN = 6; // "*DR=XX"
static constexpr size_t MU_DS_RSSI_TOTAL_LEN = 6; // "*DS=XX"
static constexpr size_t MU_HEX_VAL_OFFSET = 4;    // Index of hex value in "*DR=XX" or "*DS=XX"

// LBT (Listen Before Talk) check timeout after command acceptance
static constexpr uint32_t MU_LBT_CHECK_TIMEOUT_MS = 60;

MU_Modem_Error MU_Modem::begin(Stream &pUart, MU_Modem_FrequencyModel frequencyModel, MU_Modem_AsyncCallback pCallback)
{
    initSerial(pUart);
    m_frequencyModel = frequencyModel;
    m_pCallback = pCallback;
    m_asyncExpectedResponse = MU_Modem_Response::Idle;
    m_lbtErrorDetected = false;
    m_blockAsyncCallback = false;
    m_ResetParser();

    SM_DEBUG_PRINTLN("begin: Waiting for modem stabilization...");
    pUart.print("\r\n");
    uint32_t clearStart = millis();
    while (pUart.available() > 0 && millis() - clearStart < 200)
    {
        pUart.read();
    }

    SM_DEBUG_PRINTLN("begin: Resetting modem...");

    // Perform software reset (Synchronous)
    MU_Modem_Error err = SoftReset();
    if (err != MU_Modem_Error::Ok)
    {
        // retry
        SM_DEBUG_PRINTLN("begin: SoftReset failed, retrying...");
        delay(500);
        err = SoftReset();
        if (err != MU_Modem_Error::Ok)
        {
            SM_DEBUG_PRINTF("begin: SoftReset failed permanently! err=%d\n", (int)err);
            return err;
        }
    }

    delay(200); // Wait for modem restart

    // Enable RSSI appending to *DR messages
    err = SetAddRssiValue();
    if (err != MU_Modem_Error::Ok)
    {
        SM_DEBUG_PRINTF("begin: SetAddRssiValue failed! err=%d\n", (int)err);
        return err;
    }

    SM_DEBUG_PRINTLN("begin: Initialization successful.");
    return MU_Modem_Error::Ok;
}

void MU_Modem::Work()
{
    // Delegate strictly to the base engine's state machine
    SerialModemBase::update();
}

// --- Data Transmission (Synchronous Wrapper) ---

MU_Modem_Error MU_Modem::TransmitData(const uint8_t *pMsg, uint8_t len, bool useRouteRegister)
{
    m_lbtErrorDetected = false;  // Reset LBT error flag
    m_blockAsyncCallback = true; // Suppress async callbacks during sync LBT check

    // 1. Prepare Command Header
    char cmdHeader[16];
    char *p = appendStr(cmdHeader, cmdHeader, MU_TRANSMISSION_PREFIX_STRING);
    appendHex2(cmdHeader, p, len);

    // 2. Queue Async Command (Wait up to 2000ms for *DT response)
    const char *suffix = useRouteRegister ? MU_ROUTE_INFO_OPTION_PREFIX : nullptr;
    MU_Modem_Error err = enqueueTxCommand(cmdHeader, pMsg, len, suffix, 2000);
    if (err != MU_Modem_Error::Ok)
    {
        m_blockAsyncCallback = false;
        return err;
    }

    // 3. Wait for *DT=XX confirmation (Synchronous wait)
    err = waitForSyncComplete(2500);

    // 4. Post-confirmation LBT Check
    // Even if *DT=XX is received (Command Accepted), MU modem might immediately return *IR=01 if LBT fails.
    // We wait a short period to see if an error follows.
    if (err == MU_Modem_Error::Ok)
    {
        uint32_t start = millis();
        // Wait for potential *IR=01 response (LBT Error)
        while (millis() - start < MU_LBT_CHECK_TIMEOUT_MS)
        {
            Work(); // Pump the parser
            if (m_lbtErrorDetected)
            {
                // LBT Error Detected!
                m_blockAsyncCallback = false;
                return MU_Modem_Error::FailLbt;
            }
            delay(1); // Yield
        }
    }
    else if (err == MU_Modem_Error::Fail && m_lbtErrorDetected)
    {
        // If waitForSyncComplete failed genericly but we saw LBT flag
        m_blockAsyncCallback = false;
        return MU_Modem_Error::FailLbt;
    }

    m_blockAsyncCallback = false;
    return err;
}

// --- Data Transmission (Asynchronous) ---

MU_Modem_Error MU_Modem::TransmitDataAsync(const uint8_t *pMsg, uint8_t len, bool useRouteRegister)
{
    m_lbtErrorDetected = false;

    // Minimal safety delay to allow modem to switch states (especially for quick echo response)
    // delay(5);

    char cmdHeader[16];
    char *p = appendStr(cmdHeader, cmdHeader, MU_TRANSMISSION_PREFIX_STRING);
    appendHex2(cmdHeader, p, len);

    // Just queue and return. Result will be delivered via Callback (TxComplete / TxFailed)
    const char *suffix = useRouteRegister ? MU_ROUTE_INFO_OPTION_PREFIX : nullptr;
    return enqueueTxCommand(cmdHeader, pMsg, len, suffix, 2000);
}

// --- Parser Implementation ---

void MU_Modem::m_ResetParser()
{
    m_parserState = MU_Modem_ParserState::Start;
    m_drMessageLen = 0;
    m_drNumRouteNodes = 0;
    m_drMessagePresent = false;
}

ModemParseResult MU_Modem::parse()
{
    while (true)
    {
        int c_int = readByte();
        if (c_int == -1)
            break;
        uint8_t c = static_cast<uint8_t>(c_int);

        switch (m_parserState)
        {
        case MU_Modem_ParserState::Start:
            if (c == '*')
            {
                _rxIndex = 0;
                _rxBuffer[_rxIndex++] = c;
                m_parserState = MU_Modem_ParserState::ReadCmdPrefix;
            }
            break;

        case MU_Modem_ParserState::ReadCmdPrefix:
        {
            ModemParseResult res = m_HandleReadCmdPrefix(c);
            if (res != ModemParseResult::Parsing)
                return res;
        }
        break;

        case MU_Modem_ParserState::RadioDrSize:
        {
            ModemParseResult res = m_HandleRadioDrSize(c);
            if (res != ModemParseResult::Parsing)
                return res;
        }
        break;

        case MU_Modem_ParserState::RadioDsRssi:
        {
            ModemParseResult res = m_HandleRadioDsRssi(c);
            if (res != ModemParseResult::Parsing)
                return res;
        }
        break;

        case MU_Modem_ParserState::RadioDrPayload:
        {
            ModemParseResult res = m_HandleRadioDrPayload(c);
            if (res != ModemParseResult::Parsing)
                return res;
        }
        break;

        case MU_Modem_ParserState::ReadOptionUntilLF:
        {
            ModemParseResult res = m_HandleReadOptionUntilLF(c);
            if (res != ModemParseResult::Parsing)
                return res;
        }
        break;

        default:
            m_ResetParser();
            return ModemParseResult::Garbage;
        }
    }
    return ModemParseResult::Parsing;
}

ModemParseResult MU_Modem::m_HandleReadCmdPrefix(uint8_t c)
{
    if (_rxIndex < RX_BUFFER_SIZE)
        _rxBuffer[_rxIndex++] = c;

    // Check for *DR=, *DS= or *DC= (Data Reception)
    if (_rxIndex == MU_DR_PREFIX_LEN)
    {
        if (strncmp((char *)_rxBuffer, MU_RECEPTION_PREFIX_DR, MU_DR_PREFIX_LEN) == 0)
        {
            m_parserState = MU_Modem_ParserState::RadioDrSize;
            return ModemParseResult::Parsing;
        }
        if (strncmp((char *)_rxBuffer, MU_RECEPTION_PREFIX_DS, MU_DS_PREFIX_LEN) == 0 ||
            strncmp((char *)_rxBuffer, MU_RECEPTION_PREFIX_DC, MU_DS_PREFIX_LEN) == 0)
        {
            m_parserState = MU_Modem_ParserState::RadioDsRssi;
            return ModemParseResult::Parsing;
        }
    }

    // Check for CR/LF (End of standard command response)
    if (c == '\n')
    {
        if (_rxIndex > 0)
        {
            _rxIndex--;
            _rxBuffer[_rxIndex] = 0;
        }
        if (_rxIndex > 0 && _rxBuffer[_rxIndex - 1] == '\r')
        {
            _rxIndex--;
            _rxBuffer[_rxIndex] = 0;
        }

        m_parserState = MU_Modem_ParserState::Start;

        // Check for LBT Error (*IR=01)
        if (strncmp((char *)_rxBuffer, MU_LBT_ERROR_RESPONSE, 6) == 0)
        {
            m_lbtErrorDetected = true;
            if (m_pCallback)
            {
                m_pCallback(MU_Modem_Event(MU_Modem_Error::FailLbt, MU_Modem_Response::TxFailed));
            }
            return ModemParseResult::FinishedCmdResponse;
        }

        return ModemParseResult::FinishedCmdResponse;
    }
    return ModemParseResult::Parsing;
}

ModemParseResult MU_Modem::m_HandleRadioDrSize(uint8_t c)
{
    if (_rxIndex < RX_BUFFER_SIZE)
        _rxBuffer[_rxIndex++] = c;
    if (_rxIndex == MU_DR_SIZE_TOTAL_LEN)
    {
        uint32_t len;
        if (parseHex(&_rxBuffer[MU_HEX_VAL_OFFSET], 2, &len))
        {
            m_drMessageLen = (uint8_t)len;
            m_parserState = MU_Modem_ParserState::RadioDrPayload;
            _rxIndex = 0;
            return ModemParseResult::Parsing;
        }
        else
        {
            m_parserState = MU_Modem_ParserState::Start;
            return ModemParseResult::Garbage;
        }
    }
    return ModemParseResult::Parsing;
}

ModemParseResult MU_Modem::m_HandleRadioDsRssi(uint8_t c)
{
    if (_rxIndex < RX_BUFFER_SIZE)
        _rxBuffer[_rxIndex++] = c;
    if (_rxIndex == MU_DS_RSSI_TOTAL_LEN)
    {
        uint32_t rssiVal;
        if (parseHex(&_rxBuffer[MU_HEX_VAL_OFFSET], 2, &rssiVal))
        {
            m_lastRxRSSI = -static_cast<int16_t>(rssiVal);
            // After RSSI, the format is the same as *DR (size, payload, etc.)
            // So, transition to the size-parsing state.
            m_parserState = MU_Modem_ParserState::RadioDrSize;
            _rxIndex = MU_DR_PREFIX_LEN; // Pretend we just saw "*DR=" to reuse the logic
            return ModemParseResult::Parsing;
        }
        else
        {
            m_parserState = MU_Modem_ParserState::Start;
            return ModemParseResult::Garbage;
        }
    }
    return ModemParseResult::Parsing;
}

ModemParseResult MU_Modem::m_HandleRadioDrPayload(uint8_t c)
{
    if (_rxIndex < m_drMessageLen)
    {
        if (_rxIndex < RX_BUFFER_SIZE)
            _rxBuffer[_rxIndex] = c;
        _rxIndex++;
    }

    if (_rxIndex == m_drMessageLen)
    {
        m_parserState = MU_Modem_ParserState::ReadOptionUntilLF;
    }
    return ModemParseResult::Parsing;
}

ModemParseResult MU_Modem::m_HandleReadOptionUntilLF(uint8_t c)
{
    if (_rxIndex < RX_BUFFER_SIZE)
        _rxBuffer[_rxIndex++] = c;

    if (c == '\n')
    {
        size_t optLen = strlen(MU_ROUTE_INFO_OPTION_PREFIX);
        if (_rxIndex > m_drMessageLen + optLen)
        {
            const char *pOpt = (const char *)&_rxBuffer[m_drMessageLen];
            const char *pEnd = (const char *)&_rxBuffer[_rxIndex];

            while (pOpt <= pEnd - optLen)
            {
                if (strncmp(pOpt, MU_ROUTE_INFO_OPTION_PREFIX, optLen) == 0)
                {
                    pOpt += optLen;
                    m_drNumRouteNodes = 0;
                    while (pOpt < pEnd && m_drNumRouteNodes < MU_MAX_ROUTE_NODES_IN_DR)
                    {
                        uint32_t nodeID;
                        if (parseHex((const uint8_t *)pOpt, 2, &nodeID))
                        {
                            m_drRouteInfo[m_drNumRouteNodes++] = (uint8_t)nodeID;
                            pOpt += 2;
                            if (*pOpt == ',')
                                pOpt++;
                            else
                                break;
                        }
                        else
                        {
                            break;
                        }
                    }
                    break;
                }
                pOpt++;
            }
        }

        m_drMessagePresent = true;
        m_parserState = MU_Modem_ParserState::Start;
        return ModemParseResult::FinishedDrResponse;
    }
    else if (_rxIndex >= RX_BUFFER_SIZE)
    {
        m_parserState = MU_Modem_ParserState::Start;
        return ModemParseResult::Overflow;
    }
    return ModemParseResult::Parsing;
}

// --- Callbacks ---

void MU_Modem::onRxDataReceived()
{
    // Called when parse() returns FinishedDrResponse
    // 1. Fire Async Callback (Zero Copy)
    // Pass the _rxBuffer directly.
    // Note: This buffer is volatile and will be overwritten by the next modem activity.
    if (m_pCallback && !m_blockAsyncCallback)
    {
        MU_Modem_Event ev(MU_Modem_Error::Ok, MU_Modem_Response::DataReceived, m_lastRxRSSI);
        ev.pPayload = _rxBuffer;
        ev.payloadLen = m_drMessageLen;
        ev.pRouteNodes = m_drRouteInfo;
        ev.numRouteNodes = m_drNumRouteNodes;
        m_pCallback(ev);
    }

    // 2. Legacy Support (Copy if buffer provided)
    if (m_pLegacyBuffer != nullptr && m_legacyBufferSize >= m_drMessageLen)
    {
        memcpy(m_pLegacyBuffer, _rxBuffer, m_drMessageLen);
        // Ensure null termination if there is enough space
        if (m_legacyBufferSize > m_drMessageLen)
        {
            m_pLegacyBuffer[m_drMessageLen] = '\0';
        }
        m_drMessagePresent = true;
    }
}

void MU_Modem::onCommandComplete(ModemError result)
{
    // Called by Base when a command (async or sync) finishes
    if (m_pCallback)
    {
        MU_Modem_Event ev(result, MU_Modem_Response::GenericResponse);

        const uint8_t *rxBuf = getRxBuffer();
        uint16_t rxLen = getRxIndex();

        // Check if response is an error (*ER=XX)
        bool isErrorResp = (rxLen >= MU_ERROR_RESPONSE_PREFIX_LEN + 2 && strncmp((const char *)rxBuf, MU_ERROR_RESPONSE_PREFIX, MU_ERROR_RESPONSE_PREFIX_LEN) == 0);
        if (isErrorResp)
        {
            uint32_t errCode;
            if (parseHex(rxBuf + MU_ERROR_RESPONSE_PREFIX_LEN, 2, &errCode))
            {
                ev.value = (int32_t)errCode;
            }
            if (result == ModemError::Ok)
            {
                // If base thought it was OK but it's an error code from modem
                ev.error = ModemError::Fail;
            }
        }

        if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
        {
            ev.type = m_asyncExpectedResponse;

            if (ev.error == ModemError::Ok)
            {
                // Parse specific value based on expected response
                if (ev.type == MU_Modem_Response::RssiCurrentChannel)
                {
                    uint8_t rawRssi;
                    if (parseResponseHex(rxBuf, rxLen, MU_GET_RSSI_CURRENT_CHANNEL_RESPONSE_PREFIX, 2, (uint32_t *)&rawRssi) == ModemError::Ok)
                        ev.value = -(int16_t)rawRssi;
                }
                else if (ev.type == MU_Modem_Response::SerialNumber)
                {
                    const char *pData = strchr((const char *)rxBuf, '=');
                    if (pData)
                    {
                        pData++; // Skip '='
                        if (isalpha((unsigned char)*pData))
                            pData++; // Skip 'E' etc.
                        uint32_t sn;
                        if (parseHex((const uint8_t *)pData, 8, &sn))
                            ev.value = (int32_t)sn;
                    }
                }
                else if (ev.type == MU_Modem_Response::RssiAllChannels)
                {
                    size_t prefixLen = strlen(MU_GET_RSSI_ALL_CHANNELS_RESPONSE_PREFIX);
                    if (rxLen >= prefixLen)
                    {
                        ev.pPayload = rxBuf + prefixLen;
                        ev.payloadLen = rxLen - prefixLen;
                    }
                }
                else if (ev.type == MU_Modem_Response::Channel)
                {
                    uint8_t ch;
                    if (parseResponseHex(rxBuf, rxLen, MU_SET_CHANNEL_RESPONSE_PREFIX, 2, (uint32_t *)&ch) == ModemError::Ok)
                        ev.value = ch;
                }
                else if (ev.type == MU_Modem_Response::GroupID)
                {
                    uint8_t gi;
                    if (parseResponseHex(rxBuf, rxLen, MU_SET_GROUP_RESPONSE_PREFIX, 2, (uint32_t *)&gi) == ModemError::Ok)
                        ev.value = gi;
                }
                else if (ev.type == MU_Modem_Response::EquipmentID)
                {
                    uint8_t ei;
                    if (parseResponseHex(rxBuf, rxLen, MU_SET_EQUIPMENT_RESPONSE_PREFIX, 2, (uint32_t *)&ei) == ModemError::Ok)
                        ev.value = ei;
                }
                else if (ev.type == MU_Modem_Response::DestinationID)
                {
                    uint8_t di;
                    if (parseResponseHex(rxBuf, rxLen, MU_SET_DESTINATION_RESPONSE_PREFIX, 2, (uint32_t *)&di) == ModemError::Ok)
                        ev.value = di;
                }
            }
            m_asyncExpectedResponse = MU_Modem_Response::Idle;
        }
        else
        {
            // If it was a DataTx command, notify Tx status
            if (strncmp((const char *)rxBuf, MU_TRANSMISSION_RESPONSE_PREFIX, strlen(MU_TRANSMISSION_RESPONSE_PREFIX)) == 0)
            {
                ev.type = (ev.error == ModemError::Ok) ? MU_Modem_Response::TxComplete : MU_Modem_Response::TxFailed;
            }
            else if (isErrorResp)
            {
                // If it's an error response and no specific async request,
                // it might be a failure of a simple command or DataTx.
            }
        }

        m_pCallback(ev);
    }
}

// --- Configuration Wrappers (Synchronous) ---

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
    return setByteValue(MU_CMD_BAUD_RATE, baudCode, saveValue, MU_SET_BAUD_RATE_RESPONSE_PREFIX, MU_SET_BAUD_RATE_RESPONSE_LEN);
}

MU_Modem_Error MU_Modem::SetChannel(uint8_t channel, bool saveValue)
{
    uint8_t chMin = (m_frequencyModel == MU_Modem_FrequencyModel::MHz_429) ? MU_CHANNEL_MIN_429 : MU_CHANNEL_MIN_1216;
    uint8_t chMax = (m_frequencyModel == MU_Modem_FrequencyModel::MHz_429) ? MU_CHANNEL_MAX_429 : MU_CHANNEL_MAX_1216;
    if (channel < chMin || channel > chMax)
        return MU_Modem_Error::InvalidArg;

    MU_Modem_Error err = setByteValue(MU_CMD_CHANNEL, channel, saveValue, MU_SET_CHANNEL_RESPONSE_PREFIX, MU_SET_CHANNEL_RESPONSE_LEN);
    if (err == MU_Modem_Error::Ok && saveValue)
    {
        SetAddRssiValue(); // Re-enable RSSI after save
    }
    return err;
}

MU_Modem_Error MU_Modem::GetChannel(uint8_t *pChannel)
{
    return getByteValue(MU_CMD_CHANNEL, pChannel, MU_SET_CHANNEL_RESPONSE_PREFIX, MU_SET_CHANNEL_RESPONSE_LEN);
}

MU_Modem_Error MU_Modem::SetPower(uint8_t power, bool saveValue)
{
    if (power != 0x01 && power != 0x10)
        return MU_Modem_Error::InvalidArg;
    return setByteValue(MU_CMD_POWER, power, saveValue, MU_SET_POWER_RESPONSE_PREFIX, MU_SET_POWER_RESPONSE_LEN);
}

MU_Modem_Error MU_Modem::GetPower(uint8_t *pPower)
{
    return getByteValue(MU_CMD_POWER, pPower, MU_SET_POWER_RESPONSE_PREFIX, MU_SET_POWER_RESPONSE_LEN);
}

MU_Modem_Error MU_Modem::SetDestinationID(uint8_t di, bool saveValue)
{
    return setByteValue(MU_CMD_DESTINATION, di, saveValue, MU_SET_DESTINATION_RESPONSE_PREFIX, MU_SET_DESTINATION_RESPONSE_LEN);
}

MU_Modem_Error MU_Modem::GetDestinationID(uint8_t *pDI)
{
    return getByteValue(MU_CMD_DESTINATION, pDI, MU_SET_DESTINATION_RESPONSE_PREFIX, MU_SET_DESTINATION_RESPONSE_LEN);
}

MU_Modem_Error MU_Modem::SetEquipmentID(uint8_t ei, bool saveValue)
{
    return setByteValue(MU_CMD_EQUIPMENT, ei, saveValue, MU_SET_EQUIPMENT_RESPONSE_PREFIX, MU_SET_EQUIPMENT_RESPONSE_LEN);
}

MU_Modem_Error MU_Modem::GetEquipmentID(uint8_t *pEI)
{
    return getByteValue(MU_CMD_EQUIPMENT, pEI, MU_SET_EQUIPMENT_RESPONSE_PREFIX, MU_SET_EQUIPMENT_RESPONSE_LEN);
}

MU_Modem_Error MU_Modem::SetGroupID(uint8_t gi, bool saveValue)
{
    return setByteValue(MU_CMD_GROUP, gi, saveValue, MU_SET_GROUP_RESPONSE_PREFIX, MU_SET_GROUP_RESPONSE_LEN);
}

MU_Modem_Error MU_Modem::GetGroupID(uint8_t *pGI)
{
    return getByteValue(MU_CMD_GROUP, pGI, MU_SET_GROUP_RESPONSE_PREFIX, MU_SET_GROUP_RESPONSE_LEN);
}

MU_Modem_Error MU_Modem::SetRouteInfoAddMode(bool enabled, bool saveValue)
{
    return setBoolValue(MU_CMD_ROUTE_INFO_ADD, enabled, saveValue, MU_GET_ROUTE_INFO_ADD_MODE_RESPONSE_PREFIX);
}

MU_Modem_Error MU_Modem::GetRouteInfoAddMode(bool *pEnabled)
{
    return getBoolValue(MU_CMD_ROUTE_INFO_ADD, pEnabled, MU_GET_ROUTE_INFO_ADD_MODE_RESPONSE_PREFIX);
}

MU_Modem_Error MU_Modem::SetAutoReplyRoute(bool enabled, bool saveValue)
{
    return setBoolValue(MU_CMD_USR_ROUTE, enabled, saveValue, MU_GET_USR_ROUTE_RESPONSE_PREFIX);
}

MU_Modem_Error MU_Modem::GetAutoReplyRoute(bool *pEnabled)
{
    return getBoolValue(MU_CMD_USR_ROUTE, pEnabled, MU_GET_USR_ROUTE_RESPONSE_PREFIX);
}

MU_Modem_Error MU_Modem::SetAddRssiValue()
{
    return setBoolValue(MU_CMD_ADD_RSSI, true, false, MU_SET_ADD_RSSI_RESPONSE_PREFIX);
}

MU_Modem_Error MU_Modem::SoftReset()
{
    char cmdBuf[8];
    char *p = appendStr(cmdBuf, cmdBuf, MU_CMD_SOFT_RESET);
    appendStr(cmdBuf, p, "\r\n");

    MU_Modem_Error err = enqueueCommand(cmdBuf, CommandType::Simple, 1000);
    if (err != MU_Modem_Error::Ok)
        return err;
    return waitForSyncComplete(1000);
}

MU_Modem_Error MU_Modem::GetUserID(uint16_t *pUI)
{
    char cmd[16];
    char *p = appendStr(cmd, cmd, MU_CMD_USER_ID);
    appendStr(cmd, p, "\r\n");

    MU_Modem_Error err = enqueueCommand(cmd, CommandType::Simple, 1000);
    if (err != MU_Modem_Error::Ok)
        return err;

    err = waitForSyncComplete(1000);
    if (err == MU_Modem_Error::Ok)
    {
        uint32_t val;
        err = parseResponseHex(getRxBuffer(), getRxIndex(), MU_GET_USER_ID_RESPONSE_PREFIX, 4, &val);
        if (err == MU_Modem_Error::Ok && pUI)
        {
            *pUI = static_cast<uint16_t>(val);
        }
    }
    return err;
}

MU_Modem_Error MU_Modem::GetSerialNumber(uint32_t *pSerialNumber)
{
    if (!pSerialNumber)
        return MU_Modem_Error::InvalidArg;

    char cmd[16];
    char *p = appendStr(cmd, cmd, MU_CMD_SERIAL_NUMBER);
    appendStr(cmd, p, "\r\n");

    MU_Modem_Error err = enqueueCommand(cmd, CommandType::Simple, 1000);
    if (err != MU_Modem_Error::Ok)
        return err;

    err = waitForSyncComplete(1000);
    if (err == MU_Modem_Error::Ok)
    {
        const uint8_t *rxBuf = getRxBuffer();
        const char *pEq = strchr((const char *)rxBuf, '=');
        if (pEq)
        {
            uint32_t val;
            const char *pData = pEq + 1;

            // Skip leading alphabet if present (e.g., *SN=E00004056)
            if (isalpha((unsigned char)*pData))
            {
                pData++;
            }

            SM_DEBUG_PRINTF("Parsing SN from: %s\n", pData);

            if (parseHex((const uint8_t *)pData, 8, &val))
            {
                *pSerialNumber = val;
                return MU_Modem_Error::Ok;
            }
        }
    }
    SM_DEBUG_PRINTLN("GetSerialNumber: Parse failed.");
    return MU_Modem_Error::Fail;
}

MU_Modem_Error MU_Modem::GetRssiCurrentChannel(int16_t *pRssi)
{
    uint8_t val;
    MU_Modem_Error err = getByteValue(MU_CMD_RSSI_CURRENT, &val, MU_GET_RSSI_CURRENT_CHANNEL_RESPONSE_PREFIX, MU_GET_RSSI_CURRENT_CHANNEL_RESPONSE_LEN);
    if (err == MU_Modem_Error::Ok)
    {
        *pRssi = -(int16_t)val;
    }
    return err;
}

MU_Modem_Error MU_Modem::GetRssiCurrentChannelAsync()
{
    m_asyncExpectedResponse = MU_Modem_Response::RssiCurrentChannel;
    char cmdBuf[8];
    char *p = appendStr(cmdBuf, cmdBuf, MU_CMD_RSSI_CURRENT);
    appendStr(cmdBuf, p, "\r\n");
    return enqueueCommand(cmdBuf, CommandType::Simple, 1000);
}

MU_Modem_Error MU_Modem::GetAllChannelsRssi(int16_t *pRssiBuffer, size_t bufferSize, uint8_t *pNumRssiValues)
{
    if (!pRssiBuffer || !pNumRssiValues)
        return MU_Modem_Error::InvalidArg;
    *pNumRssiValues = 0;

    char cmdBuf[16];
    char *p = appendStr(cmdBuf, cmdBuf, MU_CMD_RSSI_ALL);
    appendStr(cmdBuf, p, "\r\n");
    MU_Modem_Error err = enqueueCommand(cmdBuf, CommandType::Simple, 20000);
    if (err != MU_Modem_Error::Ok)
        return err;

    err = waitForSyncComplete(20000);
    if (err != MU_Modem_Error::Ok)
        return err;

    const uint8_t *rxBuf = getRxBuffer();
    uint16_t rxLen = getRxIndex();

    size_t expectedNum = (m_frequencyModel == MU_Modem_FrequencyModel::MHz_429) ? 40 : 19;
    size_t prefixLen = strlen(MU_GET_RSSI_ALL_CHANNELS_RESPONSE_PREFIX);

    if (rxLen < prefixLen + (expectedNum * 2) || strncmp((const char *)rxBuf, MU_GET_RSSI_ALL_CHANNELS_RESPONSE_PREFIX, prefixLen) != 0)
    {
        return MU_Modem_Error::Fail;
    }

    const uint8_t *pData = rxBuf + prefixLen;
    uint8_t count = 0;
    for (size_t i = 0; i < expectedNum && i < bufferSize; ++i)
    {
        uint32_t val;
        if (parseHex(pData + (i * 2), 2, &val))
        {
            pRssiBuffer[i] = -static_cast<int16_t>(val);
            count++;
        }
        else
        {
            break;
        }
    }
    *pNumRssiValues = count;
    return (count == expectedNum) ? MU_Modem_Error::Ok : MU_Modem_Error::Fail;
}

MU_Modem_Error MU_Modem::GetAllChannelsRssiAsync()
{
    m_asyncExpectedResponse = MU_Modem_Response::RssiAllChannels;
    char cmdBuf[8];
    char *p = appendStr(cmdBuf, cmdBuf, MU_CMD_RSSI_ALL);
    appendStr(cmdBuf, p, "\r\n");
    return enqueueCommand(cmdBuf, CommandType::Simple, 20000);
}

MU_Modem_Error MU_Modem::SetRouteInfo(const uint8_t *pRouteInfo, uint8_t numNodes, bool saveValue)
{
    // Need to build hex string
    if (numNodes == 0 || numNodes > 11)
        return MU_Modem_Error::InvalidArg;

    char cmdBuffer[128]; // Stack heavy, but Sync command
    char *p = appendStr(cmdBuffer, cmdBuffer, MU_CMD_ROUTE);
    for (uint8_t i = 0; i < numNodes; ++i)
    {
        p = appendHex2(cmdBuffer, p, pRouteInfo[i]);
        if (i < numNodes - 1)
            p = appendStr(cmdBuffer, p, ",");
    }
    if (saveValue)
        p = appendStr(cmdBuffer, p, CD_CMD_WRITE_SUFFIX);
    appendStr(cmdBuffer, p, "\r\n");

    MU_Modem_Error err = enqueueCommand(cmdBuffer, saveValue ? CommandType::NvmSave : CommandType::Simple, 1500);
    if (err != MU_Modem_Error::Ok)
        return err;
    return waitForSyncComplete(1500);
}

MU_Modem_Error MU_Modem::ClearRouteInfo(bool saveValue)
{
    char cmdBuffer[32];
    char *p = appendStr(cmdBuffer, cmdBuffer, MU_CMD_ROUTE);
    p = appendStr(cmdBuffer, p, "NA");
    if (saveValue)
        p = appendStr(cmdBuffer, p, CD_CMD_WRITE_SUFFIX);
    appendStr(cmdBuffer, p, "\r\n");
    MU_Modem_Error err = enqueueCommand(cmdBuffer, saveValue ? CommandType::NvmSave : CommandType::Simple, 1500);
    if (err != MU_Modem_Error::Ok)
        return err;
    return waitForSyncComplete(1500);
}

MU_Modem_Error MU_Modem::GetRouteInfo(uint8_t *pRouteInfoBuffer, size_t bufferSize, uint8_t *pNumNodes)
{
    if (!pRouteInfoBuffer || !pNumNodes)
        return MU_Modem_Error::InvalidArg;
    *pNumNodes = 0;

    char cmd[16];
    char *p = appendStr(cmd, cmd, MU_CMD_ROUTE);
    appendStr(cmd, p, "\r\n");
    MU_Modem_Error err = enqueueCommand(cmd, CommandType::Simple, 1000);
    if (err != MU_Modem_Error::Ok)
        return err;

    err = waitForSyncComplete(1000);
    if (err != MU_Modem_Error::Ok)
        return err;

    const uint8_t *rxBuf = getRxBuffer();
    uint16_t rxLen = getRxIndex();
    size_t prefixLen = strlen(MU_SET_ROUTE_RESPONSE_PREFIX);

    if (rxLen < prefixLen || strncmp((const char *)rxBuf, MU_SET_ROUTE_RESPONSE_PREFIX, prefixLen) != 0)
    {
        return MU_Modem_Error::Fail;
    }

    if (strncmp((const char *)rxBuf, MU_ROUTE_NA_RESPONSE, strlen(MU_ROUTE_NA_RESPONSE)) == 0)
    {
        return MU_Modem_Error::Ok;
    }

    // Parse hex IDs separated by commas: "*RT=01,02,03"
    const char *pData = (const char *)rxBuf + prefixLen;
    uint8_t count = 0;
    while (*pData && count < bufferSize)
    {
        uint32_t val;
        if (parseHex((const uint8_t *)pData, 2, &val))
        {
            pRouteInfoBuffer[count++] = static_cast<uint8_t>(val);
            pData += 2;
            if (*pData == ',')
                pData++;
            else
                break;
        }
        else
        {
            break;
        }
    }
    *pNumNodes = count;
    return MU_Modem_Error::Ok;
}

MU_Modem_Error MU_Modem::CheckCarrierSense()
{
    char cmd[8];
    char *p = appendStr(cmd, cmd, MU_CMD_CHANNEL_STATUS);
    appendStr(cmd, p, "\r\n");

    char resp[16];
    MU_Modem_Error err = SendRawCommand(cmd, resp, sizeof(resp));
    if (err == MU_Modem_Error::Ok)
    {
        if (strncmp(resp, MU_CHANNEL_STATUS_OK_RESPONSE, 6) == 0)
            return MU_Modem_Error::Ok;
        if (strncmp(resp, MU_CHANNEL_STATUS_BUSY_RESPONSE, 6) == 0)
            return MU_Modem_Error::FailLbt;
    }
    return MU_Modem_Error::Fail;
}

MU_Modem_Error MU_Modem::SendRawCommand(const char *command, char *responseBuffer, size_t bufferSize, uint32_t timeoutMs)
{
    return sendRawCommand(command, responseBuffer, bufferSize, timeoutMs);
}

// --- Legacy Packet Accessors ---

MU_Modem_Error MU_Modem::GetPacket(const uint8_t **ppData, uint8_t *len)
{
    if (m_drMessagePresent)
    {
        if (m_pLegacyBuffer)
        {
            *ppData = m_pLegacyBuffer;
        }
        else
        {
            // If no legacy buffer, point to internal buffer
            // Note: This is volatile and may be overwritten by next command
            *ppData = _rxBuffer;
        }
        *len = m_drMessageLen;
        return MU_Modem_Error::Ok;
    }
    return MU_Modem_Error::Fail;
}