//
// MU_Modem.cpp
//
// The original program
// (c) 2019 Reimesch Kommunikationssysteme
// Authors: aj, cl
// Created on: 13.03.2019
// Released under the MIT license
//
// (c) 2025 CircuitDesign,Inc.
// Interface driver for MU-3/MU-4 (FSK modem manufactured by Circuit Design)
//

#include "MU_Modem.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

// --- MU Modem Command and Response String Constants ---

// @W (Write to NVM)
static constexpr const char *MU_WRITE_VALUE_RESPONSE_PREFIX = "*WR=PS";
static constexpr size_t MU_WRITE_VALUE_RESPONSE_LEN = 6; // length of "*WR=PS" excluding "\r\n"

// @DT (Data Transmission)
static constexpr const char *MU_TRANSMISSION_PREFIX_STRING = "@DT";
static constexpr const char *MU_TRANSMISSION_RESPONSE_PREFIX = "*DT=";
static constexpr size_t MU_TRANSMISSION_RESPONSE_LEN = 6; // length of "*DT=06" excluding "\r\n"
static constexpr const char *MU_TRANSMISSION_USE_ROOT = "/R";

// *IR (Information Response)
static constexpr const char *MU_INFORMATION_RESPONSE_PREFIX = "*IR=";
static constexpr size_t MU_INFORMATION_RESPONSE_LEN = 6;        // length of "*IR=03" excluding "\r\n"
static constexpr uint8_t MU_INFORMATION_RESPONSE_ERR_NO_TX = 1; // data transmission is not possible (by carrier sense)

// @CS (Channel Status / Carrier Sense)
static constexpr const char *MU_GET_CHANNEL_STATUS_STRING = "@CS\r\n";
static constexpr const char *MU_CHANNEL_STATUS_OK_RESPONSE = "*CS=EN";
static constexpr const char *MU_CHANNEL_STATUS_BUSY_RESPONSE = "*CS=DI";
static constexpr size_t MU_CHANNEL_STATUS_RESPONSE_LEN = 6; // length of "*CS=EN" or "*CS=DI"

// @BR (Baud Rate)
static constexpr const char *MU_SET_BAUD_RATE_PREFIX_STRING = "@BR";
static constexpr const char *MU_SET_BAUD_RATE_RESPONSE_PREFIX = "*BR=";
static constexpr size_t MU_SET_BAUD_RATE_RESPONSE_LEN = 6; // length of "*BR=19" excluding "\r\n"

// @CH (Channel Frequency)
static constexpr const char *MU_GET_CHANNEL_STRING = "@CH\r\n";
static constexpr const char *MU_SET_CHANNEL_PREFIX_STRING = "@CH";
static constexpr const char *MU_SET_CHANNEL_RESPONSE_PREFIX = "*CH=";
static constexpr size_t MU_SET_CHANNEL_RESPONSE_LEN = 6; // length of "*CH=0E" excluding "\r\n"

// @GI (Group ID)
static constexpr const char *MU_GET_GROUP_STRING = "@GI\r\n";
static constexpr const char *MU_SET_GROUP_PREFIX_STRING = "@GI";
static constexpr const char *MU_SET_GROUP_RESPONSE_PREFIX = "*GI=";
static constexpr size_t MU_SET_GROUP_RESPONSE_LEN = 6; // length of "*GI=0E" excluding "\r\n"

// @DI (Destination ID)
static constexpr const char *MU_GET_DESTINATION_STRING = "@DI\r\n";
static constexpr const char *MU_SET_DESTINATION_PREFIX_STRING = "@DI";
static constexpr const char *MU_SET_DESTINATION_RESPONSE_PREFIX = "*DI=";
static constexpr size_t MU_SET_DESTINATION_RESPONSE_LEN = 6; // length of "*DI=0E" excluding "\r\n"

// @EI (Equipment ID)
static constexpr const char *MU_GET_EQUIPMENT_STRING = "@EI\r\n";
static constexpr const char *MU_SET_EQUIPMENT_PREFIX_STRING = "@EI";
static constexpr const char *MU_SET_EQUIPMENT_RESPONSE_PREFIX = "*EI=";
static constexpr size_t MU_SET_EQUIPMENT_RESPONSE_LEN = 6; // length of "*EI=0E" excluding "\r\n"

// @UI (User ID)
static constexpr const char *MU_GET_USER_ID_STRING = "@UI\r\n";
static constexpr const char *MU_GET_USER_ID_RESPONSE_PREFIX = "*UI=";
static constexpr size_t MU_GET_USER_ID_RESPONSE_LEN = 8; // length of "*UI=FFFF" excluding "\r\n"

// @RA (RSSI of Current Channel)
static constexpr const char *MU_GET_RSSI_CURRENT_CHANNEL_STRING = "@RA\r\n";
static constexpr const char *MU_GET_RSSI_CURRENT_CHANNEL_RESPONSE_PREFIX = "*RA=";
static constexpr size_t MU_GET_RSSI_CURRENT_CHANNEL_RESPONSE_LEN = 6; // length of "*RA=XX" excluding "\r\n"

// @RC (RSSI of All Channels)
static constexpr const char *MU_GET_RSSI_ALL_CHANNELS_STRING = "@RC\r\n";
static constexpr const char *MU_GET_RSSI_ALL_CHANNELS_RESPONSE_PREFIX = "*RC=";
static constexpr size_t MU_NUM_CHANNELS_429 = 40;
static constexpr size_t MU_NUM_CHANNELS_1216 = 19;
static constexpr size_t MU_GET_RSSI_ALL_CHANNELS_RESPONSE_LEN_429 = 4 + (MU_NUM_CHANNELS_429 * 2);   // "*RC=" + 80 chars
static constexpr size_t MU_GET_RSSI_ALL_CHANNELS_RESPONSE_LEN_1216 = 4 + (MU_NUM_CHANNELS_1216 * 2); // "*RC=" + 38 chars

// @RT (Route Information)
static constexpr const char *MU_GET_ROUTE_STRING = "@RT\r\n";
static constexpr const char *MU_SET_ROUTE_PREFIX_STRING = "@RT";
static constexpr const char *MU_SET_ROUTE_RESPONSE_PREFIX = "*RT=";
static constexpr size_t MU_SET_ROUTE_RESPONSE_LEN = 6;                 // length of "*RT=NA" excluding "\r\n"
static constexpr const char *MU_ROUTE_NA_STRING = "NA";                // Route Not Available
static constexpr size_t MU_MAX_ROUTE_NODES = 11;                       // Max 10 relay nodes + 1 destination node
static constexpr size_t MU_MAX_ROUTE_STR_LEN = MU_MAX_ROUTE_NODES * 3; // e.g., "XX," * 11 = 33 (excluding the last comma)

// @PW (Transmission Power)
static constexpr const char *MU_GET_POWER_STRING = "@PW\r\n";
static constexpr const char *MU_SET_POWER_PREFIX_STRING = "@PW";
static constexpr const char *MU_SET_POWER_RESPONSE_PREFIX = "*PW=";
static constexpr size_t MU_SET_POWER_RESPONSE_LEN = 6; // length of "*PW=01" excluding "\r\n"

// @SN (Serial Number)
static constexpr const char *MU_GET_SERIAL_NUMBER_STRING = "@SN\r\n";
static constexpr const char *MU_GET_SERIAL_NUMBER_RESPONSE_PREFIX = "*SN=";
// Serial number length can vary, minimum "*SN=A00000000" (13) or "*SN=12345678" (12)
static constexpr size_t MU_GET_SERIAL_NUMBER_RESPONSE_MIN_LEN = 12;

// @SION (Enable RSSI reporting with *DR)
static constexpr const char *MU_SET_ADD_RSSI_STRING = "@SION\r\n";
static constexpr const char *MU_SET_ADD_RSSI_RESPONSE = "*SI=ON";
static constexpr size_t MU_SET_ADD_RSSI_RESPONSE_LEN = 6;

// @SR (Software Reset)
static constexpr const char *MU_SET_SOFT_RESET_STRING = "@SR\r\n";
static constexpr const char *MU_SET_SOFT_RESET_RESPONSE = "*SR=00";
static constexpr size_t MU_SET_SOFT_RESET_RESPONSE_LEN = 6;

// @RRON (Enable usage of route information from route register)
static constexpr const char *MU_SET_USR_ROUTE_ON_STRING = "@RRON\r\n";
static constexpr const char *MU_SET_USR_ROUTE_OFF_STRING = "@RROF\r\n";
static constexpr const char *MU_GET_USR_ROUTE_STRING = "@RR\r\n";
static constexpr const char *MU_GET_USR_ROUTE_RESPONSE_PREFIX = "*RR=";
static constexpr const char *MU_SET_USR_ROUTE_ON_RESPONSE = "*RR=ON";
static constexpr size_t MU_SET_USR_ROUTE_ON_RESPONSE_LEN = 6;
static constexpr const char *MU_SET_USR_ROUTE_OFF_RESPONSE = "*RR=OF";
static constexpr size_t MU_SET_USR_ROUTE_OFF_RESPONSE_LEN = 6;

// @RI (Route Information Add Mode)
static constexpr const char *MU_SET_ROUTE_INFO_ADD_MODE_ON_STRING = "@RI ON\r\n";
static constexpr const char *MU_SET_ROUTE_INFO_ADD_MODE_OFF_STRING = "@RI OF\r\n";
static constexpr const char *MU_GET_ROUTE_INFO_ADD_MODE_STRING = "@RI\r\n";
static constexpr const char *MU_GET_ROUTE_INFO_ADD_MODE_RESPONSE_PREFIX = "*RI=";
static constexpr const char *MU_SET_ROUTE_INFO_ADD_MODE_ON_RESPONSE = "*RI=ON";
static constexpr size_t MU_SET_ROUTE_INFO_ADD_MODE_ON_RESPONSE_LEN = 6;
static constexpr const char *MU_SET_ROUTE_INFO_ADD_MODE_OFF_RESPONSE = "*RI=OF";
static constexpr size_t MU_SET_ROUTE_INFO_ADD_MODE_OFF_RESPONSE_LEN = 6;

/**
 * @brief Parses a hexadecimal string representation into an integer.
 * @param pData Pointer to the start of the hex string data.
 * @param len Length of the hex string data.
 * @param pResult Pointer to store the resulting integer.
 * @return True if parsing was successful, false otherwise.
 */
const uint32_t xTicksToWait = 500UL; // Default timeout for m_WaitCmdResponse

/**
 * @brief Parses a hexadecimal string representation into an integer.
 * @param pData Pointer to the start of the hex string data.
 * @param len Length of the hex string data.
 * @param pResult Pointer to store the resulting integer.
 * @return True if parsing was successful, false otherwise.
 */
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
        {
            *pResult |= (c - '0');
        }
        else if (c >= 'a' && c <= 'f')
        {
            *pResult |= (c - 'a' + 10);
        }
        else if (c >= 'A' && c <= 'F')
        {
            *pResult |= (c - 'A' + 10);
        }
        else
        {
            // Invalid character
            *pResult = 0; // Reset result on error
            return false;
        }
    }
    return true;
}

MU_Modem_Error MU_Modem::begin(Stream &pUart, MU_Modem_FrequencyModel frequencyModel, MU_Modem_AsyncCallback pCallback)
{
    m_pUart = &pUart;
    MU_DEBUG_PRINTLN("[MU_Modem] begin: Initializing...");

    m_asyncExpectedResponse = MU_Modem_Response::Idle;
    m_pCallback = pCallback;
    m_frequencyModel = frequencyModel;
    m_rxIdx = 0;
    m_parserState = MU_Modem_ParserState::Start;
    m_drMessagePresent = false;
    m_drMessageLen = 0;
    m_lastRxRSSI = 0;
    m_ResetParser(); // Reset parser state and unread buffer

    MU_Modem_Error err;

    err = SoftReset(); // Perform software reset
    if (err != MU_Modem_Error::Ok)
    {
        MU_DEBUG_PRINTF("[MU_Modem] begin: SoftReset failed! err=%d\n", (int)err);
        return err; // Return error if reset fails
    }

    delay(150); // Recommended delay after software reset

    err = SetAddRssiValue(); // Enable RSSI reporting in *DR messages (*DS)
    if (err != MU_Modem_Error::Ok)
    {
        MU_DEBUG_PRINTF("[MU_Modem] begin: SetAddRssiValue failed! err=%d\n", (int)err);
        return err; // Return error if enabling RSSI fails
    }

    m_mode = MU_Modem_Mode::FskCmd; // Assume command mode after successful init
    MU_DEBUG_PRINTLN("[MU_Modem] begin: Initialization successful.");
    return err; // Should be MU_Modem_Error::Ok here
}

void MU_Modem::setDebugStream(Stream *debugStream)
{
    m_pDebugStream = debugStream;
}

MU_Modem_Error MU_Modem::SetAddRssiValue()
{
    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
    {
        return MU_Modem_Error::Busy;
    }

    m_WriteString(F(MU_SET_ADD_RSSI_STRING)); // Sends "@SION\r\n"

    MU_Modem_Error rv = m_WaitCmdResponse();
    if (rv != MU_Modem_Error::Ok)
        return rv;

    // Check if the response is actually *SI=ON
    if (m_rxIdx != MU_SET_ADD_RSSI_RESPONSE_LEN || strncmp(MU_SET_ADD_RSSI_RESPONSE, (char *)m_rxMessage, MU_SET_ADD_RSSI_RESPONSE_LEN) != 0)
    {
        rv = MU_Modem_Error::Fail; // Treat unexpected response as failure
    }

    return rv;
}

MU_Modem_Error MU_Modem::SetAutoReplyRoute(bool enabled, bool saveValue)
{
    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
    {
        return MU_Modem_Error::Busy;
    }

    // Generate command string (e.g., "@RRON/W\r\n")
    char cmdBuffer[12];
    if (enabled)
    {
        strcpy(cmdBuffer, "@RRON");
    }
    else
    {
        strcpy(cmdBuffer, "@RROF");
    }
    if (saveValue)
    {
        strcat(cmdBuffer, "/W");
    }
    strcat(cmdBuffer, "\r\n");

    m_WriteString(cmdBuffer);

    MU_Modem_Error rv = m_WaitCmdResponse();
    if (rv != MU_Modem_Error::Ok)
        return rv;

    if (saveValue)
    {
        rv = m_HandleMessage_WR();
        if (rv != MU_Modem_Error::Ok)
            return MU_Modem_Error::Fail;
        rv = m_WaitCmdResponse();
        if (rv != MU_Modem_Error::Ok)
            return rv;
    }

    const char *expectedResponse = enabled ? MU_SET_USR_ROUTE_ON_RESPONSE : MU_SET_USR_ROUTE_OFF_RESPONSE;
    size_t expectedLen = enabled ? MU_SET_USR_ROUTE_ON_RESPONSE_LEN : MU_SET_USR_ROUTE_OFF_RESPONSE_LEN;

    if (m_rxIdx != expectedLen || strncmp(expectedResponse, (char *)m_rxMessage, expectedLen) != 0)
    {
        rv = MU_Modem_Error::Fail;
    }

    return rv;
}

MU_Modem_Error MU_Modem::SoftReset()
{
    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
    {
        return MU_Modem_Error::Busy;
    }

    m_WriteString(F(MU_SET_SOFT_RESET_STRING)); // Sends "@SR\r\n"

    MU_Modem_Error rv = m_WaitCmdResponse();
    if (rv != MU_Modem_Error::Ok)
        return rv;

    // Check if the response is actually *SR=00
    if (m_rxIdx != MU_SET_SOFT_RESET_RESPONSE_LEN || strncmp(MU_SET_SOFT_RESET_RESPONSE, (char *)m_rxMessage, MU_SET_SOFT_RESET_RESPONSE_LEN) != 0)
    {
        rv = MU_Modem_Error::Fail; // Treat unexpected response as failure
    }

    return rv;
}

MU_Modem_Error MU_Modem::GetAutoReplyRoute(bool *pEnabled)
{
    if (!pEnabled)
        return MU_Modem_Error::InvalidArg;
    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
        return MU_Modem_Error::Busy;

    m_WriteString(F(MU_GET_USR_ROUTE_STRING)); // @RR

    MU_Modem_Error rv = m_WaitCmdResponse();

    if (rv == MU_Modem_Error::Ok)
    {
        if (m_rxIdx == MU_SET_USR_ROUTE_ON_RESPONSE_LEN && strncmp(MU_SET_USR_ROUTE_ON_RESPONSE, (char *)m_rxMessage, MU_SET_USR_ROUTE_ON_RESPONSE_LEN) == 0)
        {
            *pEnabled = true;
        }
        else if (m_rxIdx == MU_SET_USR_ROUTE_OFF_RESPONSE_LEN && strncmp(MU_SET_USR_ROUTE_OFF_RESPONSE, (char *)m_rxMessage, MU_SET_USR_ROUTE_OFF_RESPONSE_LEN) == 0)
        {
            *pEnabled = false;
        }
        else
        {
            rv = MU_Modem_Error::Fail;
        }
    }

    return rv;
}

MU_Modem_Error MU_Modem::SetRouteInfoAddMode(bool enabled, bool saveValue)
{
    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
    {
        return MU_Modem_Error::Busy;
    }

    char cmdBuffer[12];
    if (enabled)
    {
        strcpy(cmdBuffer, "@RI ON");
    }
    else
    {
        strcpy(cmdBuffer, "@RI OF");
    }
    if (saveValue)
    {
        strcat(cmdBuffer, "/W");
    }
    strcat(cmdBuffer, "\r\n");

    m_WriteString(cmdBuffer);

    MU_Modem_Error rv = m_WaitCmdResponse();
    if (rv != MU_Modem_Error::Ok)
        return rv;

    if (saveValue)
    {
        rv = m_HandleMessage_WR();
        if (rv != MU_Modem_Error::Ok)
            return MU_Modem_Error::Fail;
        rv = m_WaitCmdResponse();
        if (rv != MU_Modem_Error::Ok)
            return rv;
    }

    const char *expectedResponse = enabled ? MU_SET_ROUTE_INFO_ADD_MODE_ON_RESPONSE : MU_SET_ROUTE_INFO_ADD_MODE_OFF_RESPONSE;
    size_t expectedLen = enabled ? MU_SET_ROUTE_INFO_ADD_MODE_ON_RESPONSE_LEN : MU_SET_ROUTE_INFO_ADD_MODE_OFF_RESPONSE_LEN;

    if (m_rxIdx != expectedLen || strncmp(expectedResponse, (char *)m_rxMessage, expectedLen) != 0)
    {
        rv = MU_Modem_Error::Fail;
    }

    return rv;
}

MU_Modem_Error MU_Modem::GetRouteInfoAddMode(bool *pEnabled)
{
    if (!pEnabled)
        return MU_Modem_Error::InvalidArg;
    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
        return MU_Modem_Error::Busy;

    m_WriteString(F(MU_GET_ROUTE_INFO_ADD_MODE_STRING)); // @RI

    MU_Modem_Error rv = m_WaitCmdResponse();

    if (rv == MU_Modem_Error::Ok)
    {
        if (m_rxIdx == MU_SET_ROUTE_INFO_ADD_MODE_ON_RESPONSE_LEN && strncmp(MU_SET_ROUTE_INFO_ADD_MODE_ON_RESPONSE, (char *)m_rxMessage, MU_SET_ROUTE_INFO_ADD_MODE_ON_RESPONSE_LEN) == 0)
        {
            *pEnabled = true;
        }
        else if (m_rxIdx == MU_SET_ROUTE_INFO_ADD_MODE_OFF_RESPONSE_LEN && strncmp(MU_SET_ROUTE_INFO_ADD_MODE_OFF_RESPONSE, (char *)m_rxMessage, MU_SET_ROUTE_INFO_ADD_MODE_OFF_RESPONSE_LEN) == 0)
        {
            *pEnabled = false;
        }
        else
        {
            rv = MU_Modem_Error::Fail;
        }
    }

    return rv;
}

MU_Modem_Error MU_Modem::TransmitData(const uint8_t *pMsg, uint8_t len, bool useRouteRegister)
{
    if (!pMsg || len == 0 || len > MU_MAX_PAYLOAD_LEN)
    {
        return MU_Modem_Error::InvalidArg;
    }

    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
    {
        return MU_Modem_Error::Busy;
    }

    m_WriteString(F(MU_TRANSMISSION_PREFIX_STRING)); // "@DT"
    char lenStr[3];
    snprintf(lenStr, sizeof(lenStr), "%02X", len);
    m_WriteString(lenStr);
    m_pUart->write(pMsg, len); // Write payload

    // If requested, append the /R option to use the route register.
    if (useRouteRegister)
    {
        m_WriteString(F("/R\r\n"));
    }
    else
    {
        m_WriteString(F("\r\n")); // End command without options
    }

    // --- The following *DT response and LBT (*IR) check remain as is ---
    MU_Modem_Error rv = m_WaitCmdResponse();

    if (rv != MU_Modem_Error::Ok)
    {
        return rv; // Return Timeout or Parse Error
    }

    // Check transmission response "*DT=XX"
    uint8_t transmissionResponseLen{};
    rv = m_HandleMessageHexByte(&transmissionResponseLen, MU_TRANSMISSION_RESPONSE_LEN, MU_TRANSMISSION_RESPONSE_PREFIX);

    if (rv != MU_Modem_Error::Ok)
    {
        return rv; // Parsing failed or wrong prefix
    }
    // Check if the length in the response matches the sent length
    if (transmissionResponseLen != len)
    {
        rv = MU_Modem_Error::Fail; // Indicate failure due to length mismatch
        return rv;
    }

    MU_Modem_Error lbt_check_rv = m_WaitCmdResponse(50); // LBT result returns quickly
    if (lbt_check_rv == MU_Modem_Error::Ok)
    {
        // If there is some response (*IR=01)
        if (m_rxIdx == MU_INFORMATION_RESPONSE_LEN &&
            strncmp(MU_INFORMATION_RESPONSE_PREFIX, (char *)m_rxMessage, strlen(MU_INFORMATION_RESPONSE_PREFIX)) == 0)
        {
            uint32_t infoCode;
            if (s_ParseHex(&m_rxMessage[strlen(MU_INFORMATION_RESPONSE_PREFIX)], 2, &infoCode) && infoCode == MU_INFORMATION_RESPONSE_ERR_NO_TX)
            {
                return MU_Modem_Error::FailLbt;
            }
        }
        // Unexpected response other than *IR=01
        return MU_Modem_Error::Fail;
    }
    else if (lbt_check_rv != MU_Modem_Error::Fail)
    {
        // If m_WaitCmdResponse returns an error other than timeout (Fail)
        return lbt_check_rv;
    }

    return MU_Modem_Error::Ok;
}

MU_Modem_Error MU_Modem::TransmitDataFireAndForget(const uint8_t *pMsg, uint8_t len, bool useRouteRegister)
{
    if (!pMsg || len == 0 || len > MU_MAX_PAYLOAD_LEN)
    {
        return MU_Modem_Error::InvalidArg;
    }

    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
    {
        return MU_Modem_Error::Busy;
    }

    m_WriteString(F(MU_TRANSMISSION_PREFIX_STRING)); // "@DT"
    char lenStr[3];
    snprintf(lenStr, sizeof(lenStr), "%02X", len);
    m_WriteString(lenStr);
    m_pUart->write(pMsg, len); // Write payload

    // If requested, append the /R option to use the route register.
    if (useRouteRegister)
    {
        m_WriteString(F("/R\r\n"));
    }
    else
    {
        m_WriteString(F("\r\n")); // End command without options
    }

    // Wait for the *DT=... command acknowledgment response.
    MU_Modem_Error rv = m_WaitCmdResponse();

    if (rv != MU_Modem_Error::Ok)
    {
        return rv; // Return Timeout or Parse Error
    }

    // Check transmission response "*DT=XX"
    uint8_t transmissionResponseLen{};
    rv = m_HandleMessageHexByte(&transmissionResponseLen, MU_TRANSMISSION_RESPONSE_LEN, MU_TRANSMISSION_RESPONSE_PREFIX);

    if (rv != MU_Modem_Error::Ok)
    {
        return rv; // Parsing failed or wrong prefix
    }
    if (transmissionResponseLen != len)
    {
        return MU_Modem_Error::Fail; // Indicate failure due to length mismatch
    }

    return MU_Modem_Error::Ok; // Success, command accepted.
}

MU_Modem_Error MU_Modem::CheckCarrierSense()
{
    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
    {
        return MU_Modem_Error::Busy;
    }

    m_WriteString(F(MU_GET_CHANNEL_STATUS_STRING)); // "@CS\r\n"

    // Wait for the response (*CS=EN or *CS=DI)
    // This should be very fast.
    MU_Modem_Error rv = m_WaitCmdResponse(50); // Use a short timeout

    if (rv != MU_Modem_Error::Ok)
    {
        return rv; // Return Timeout or Parse Error, which will be interpreted as Fail
    }

    // Check the content of the response
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

    return MU_Modem_Error::Fail; // Unexpected response
}

MU_Modem_Error MU_Modem::SetBaudRate(uint32_t baudRate, bool saveValue)
{
    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
    {
        return MU_Modem_Error::Busy;
    }

    // Convert baud rate to command code
    uint8_t baudCode;
    switch (baudRate)
    {
    case 1200:
        baudCode = 12;
        break;
    case 2400:
        baudCode = 24;
        break;
    case 4800:
        baudCode = 48;
        break;
    case 9600:
        baudCode = 96;
        break;
    case 19200:
        baudCode = 19;
        break;
    case 38400:
        baudCode = 38;
        break;
    case 57600:
        baudCode = 57;
        break;
    default:
        MU_DEBUG_PRINTF("[MU_Modem] SetBaudRate: Invalid baud rate %lu\n", baudRate);
        return MU_Modem_Error::InvalidArg;
    }

    m_WriteString(F(MU_SET_BAUD_RATE_PREFIX_STRING)); // "@BR"
    char numStr[7];                                   // "XX/W\r\n" + null
    snprintf(numStr, sizeof(numStr), "%u%s\r\n", baudCode, saveValue ? "/W" : "");
    m_WriteString(numStr);

    // Wait for the response (*BR=XX)
    MU_Modem_Error rv = m_WaitCmdResponse();

    if (rv != MU_Modem_Error::Ok)
    {
        return rv; // Return Timeout or Parse Error
    }

    if (saveValue)
    {
        rv = m_HandleMessage_WR();
        if (rv != MU_Modem_Error::Ok)
            return MU_Modem_Error::Fail;
        rv = m_WaitCmdResponse();
        if (rv != MU_Modem_Error::Ok)
            return rv;
    }

    // Check the response content
    size_t prefixLen = strlen(MU_SET_BAUD_RATE_RESPONSE_PREFIX);
    if (m_rxIdx != MU_SET_BAUD_RATE_RESPONSE_LEN ||
        strncmp((const char *)m_rxMessage, MU_SET_BAUD_RATE_RESPONSE_PREFIX, prefixLen) != 0)
    {
        return MU_Modem_Error::Fail;
    }

    // Parse the code from the response
    const char *codeStr = (const char *)m_rxMessage + prefixLen;
    uint8_t responseCode = atoi(codeStr);

    if (responseCode != baudCode)
    {
        return MU_Modem_Error::Fail;
    }

    // --- Success! ---
    // The modem will switch its baud rate immediately. The caller is now responsible for reconfiguring the host UART.

    return MU_Modem_Error::Ok;
}

// Transmit data with an explicitly specified route
MU_Modem_Error MU_Modem::TransmitDataWithRoute(const uint8_t *pRouteInfo, uint8_t numNodes, const uint8_t *pMsg, uint8_t len, bool requestAck, bool outputToRelays)
{
    if (!pRouteInfo || numNodes == 0 || numNodes > MU_MAX_ROUTE_NODES || !pMsg || len == 0 || len > MU_MAX_PAYLOAD_LEN)
    {
        return MU_Modem_Error::InvalidArg;
    }

    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
    {
        return MU_Modem_Error::Busy;
    }

    m_WriteString(F(MU_TRANSMISSION_PREFIX_STRING)); // "@DT"
    char lenStr[3];
    snprintf(lenStr, sizeof(lenStr), "%02X", len);
    m_WriteString(lenStr);
    m_pUart->write(pMsg, len); // Write payload

    // Option character
    m_pUart->write('/');
    if (outputToRelays)
    {
        m_pUart->write(requestAck ? 'B' : 'S');
    }
    else
    {
        m_pUart->write(requestAck ? 'A' : 'R');
    }

    // Route information
    m_pUart->write(' ');
    char nodeStr[3]; // "XX"
    for (uint8_t i = 0; i < numNodes; ++i)
    {
        sprintf(nodeStr, "%02X", pRouteInfo[i]);
        m_WriteString(nodeStr);
        if (i < numNodes - 1)
        {
            m_pUart->write(',');
        }
    }

    // Terminator
    m_WriteString("\r\n");

    // --- Check for *DT response and LBT (*IR) result, similar to TransmitData ---
    MU_Modem_Error rv = m_WaitCmdResponse();
    if (rv != MU_Modem_Error::Ok)
        return rv;

    uint8_t transmissionResponseLen{};
    rv = m_HandleMessageHexByte(&transmissionResponseLen, MU_TRANSMISSION_RESPONSE_LEN, MU_TRANSMISSION_RESPONSE_PREFIX);
    if (rv != MU_Modem_Error::Ok)
        return rv;
    if (transmissionResponseLen != len)
        return MU_Modem_Error::Fail;

    // LBT check
    MU_Modem_Error lbt_check_rv = m_WaitCmdResponse(50);
    if (lbt_check_rv == MU_Modem_Error::Ok)
    {
        if (m_rxIdx == MU_INFORMATION_RESPONSE_LEN && strncmp(MU_INFORMATION_RESPONSE_PREFIX, (char *)m_rxMessage, strlen(MU_INFORMATION_RESPONSE_PREFIX)) == 0)
        {
            uint32_t infoCode;
            if (s_ParseHex(&m_rxMessage[strlen(MU_INFORMATION_RESPONSE_PREFIX)], 2, &infoCode) && infoCode == MU_INFORMATION_RESPONSE_ERR_NO_TX)
            {
                return MU_Modem_Error::FailLbt;
            }
        }
        return MU_Modem_Error::Fail;
    }
    else if (lbt_check_rv != MU_Modem_Error::Fail)
    {
        return lbt_check_rv;
    }

    // Handle ACK response if requested
    if (requestAck)
    {
        // Wait for ACK response (e.g., *DR=00)
        uint32_t ackTimeout = 100 + (numNodes * 60); // Estimate: 100ms + 60ms per relay hop
        rv = m_WaitCmdResponse(ackTimeout);
        if (rv == MU_Modem_Error::Ok)
        {
            // Check for ACK response (*DR=00)
            if (m_rxIdx == 6 && strncmp("*DR=00", (char *)m_rxMessage, 6) == 0)
            {
                rv = MU_Modem_Error::Ok; // Final result is OK
            }
            else
            {
                rv = MU_Modem_Error::Fail; // Unexpected ACK response
            }
        }
        else
        {
            // Timeout means no ACK was received, so it's a failure.
            rv = MU_Modem_Error::Fail;
        }
    }
    else
    {
        rv = MU_Modem_Error::Ok;
    }

    return rv;
}

MU_Modem_Error MU_Modem::GetChannel(uint8_t *pChannel)
{
    if (!pChannel)
        return MU_Modem_Error::InvalidArg;

    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
    {
        return MU_Modem_Error::Busy;
    }

    m_WriteString(F(MU_GET_CHANNEL_STRING));

    MU_Modem_Error rv = m_WaitCmdResponse();

    if (rv == MU_Modem_Error::Ok)
    {
        rv = m_HandleMessageHexByte(pChannel, MU_SET_CHANNEL_RESPONSE_LEN, MU_SET_CHANNEL_RESPONSE_PREFIX);
    }
    return rv;
}

MU_Modem_Error MU_Modem::SetChannel(uint8_t channel, bool saveValue)
{
    // Validate channel based on frequency model
    uint8_t chMin, chMax;
    switch (m_frequencyModel)
    {
    case MU_Modem_FrequencyModel::MHz_429:
        chMin = MU_CHANNEL_MIN_429;
        chMax = MU_CHANNEL_MAX_429;
        break;
    case MU_Modem_FrequencyModel::MHz_1216:
        chMin = MU_CHANNEL_MIN_1216;
        chMax = MU_CHANNEL_MAX_1216;
        break;
    default: // Should not happen if begin() succeeded
        MU_DEBUG_PRINTLN("[MU_Modem] SetChannel: Invalid frequency model");
        return MU_Modem_Error::Fail;
    }

    if ((channel < chMin) || (channel > chMax))
    {
        MU_DEBUG_PRINTF("[MU_Modem] SetChannel: Invalid channel %u (0x%02X) for model. Valid: %u-%u\n", channel, channel, chMin, chMax);
        return MU_Modem_Error::InvalidArg;
    }

    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
    {
        return MU_Modem_Error::Busy;
    }

    // e.g. @CH0A\r\n or @CH45/W\r\n
    m_WriteString(F(MU_SET_CHANNEL_PREFIX_STRING)); // "@CH"
    char numStr[7];                                 // "%02X" (2) + "/W" (2) + "\r\n" (2) + null (1) = 7
    snprintf(numStr, sizeof(numStr), "%02X%s\r\n", channel, (saveValue ? ("/W") : ("")));
    m_WriteString(numStr);

    // Wait for the first response (*WR=PS if /W, otherwise *CH=XX)
    MU_Modem_Error rv = m_WaitCmdResponse();

    if (rv != MU_Modem_Error::Ok)
    {
        return rv; // Timeout or parse error
    }

    // If saving, expect *WR=PS first, then wait for *CH=XX
    if (saveValue)
    {
        rv = m_HandleMessage_WR(); // Check if response is *WR=PS

        if (rv == MU_Modem_Error::Ok)
        {
            rv = m_WaitCmdResponse(); // Wait for the actual *CH=XX response
            if (rv != MU_Modem_Error::Ok)
            {
                return rv; // Timeout or parse error waiting for *CH
            }
        }
        else
        {
            // Didn't get *WR=PS after sending /W command
            // The received message might be *CH=XX already, but it's better to
            // return Fail directly if a *WR response was expected but not received.
            return MU_Modem_Error::Fail;
        }
    }

    // Now, handle the *CH=XX response (either directly or after *WR=PS)
    uint8_t channelResponse{};
    if (rv == MU_Modem_Error::Ok) // Should only proceed if previous steps were OK
    {
        MU_Modem_Error handleRv = m_HandleMessageHexByte(&channelResponse, MU_SET_CHANNEL_RESPONSE_LEN, MU_SET_CHANNEL_RESPONSE_PREFIX);
        if (handleRv != MU_Modem_Error::Ok)
        {
            rv = handleRv; // Update rv if handling failed
        }
    }

    // Verify the channel set matches the requested channel
    if (rv == MU_Modem_Error::Ok && channelResponse != channel)
    {
        rv = MU_Modem_Error::Fail;
    }

    // Re-enable RSSI if saving, as it might get reset (based on original comment)
    // Only do this if the main SetChannel command seemed successful
    if (rv == MU_Modem_Error::Ok && saveValue)
    {
        MU_Modem_Error rssiErr = SetAddRssiValue();
        if (rssiErr != MU_Modem_Error::Ok)
        {
            MU_DEBUG_PRINTF("[MU_Modem] SetChannel: Warning! Failed to re-enable RSSI after save. err=%d\n", (int)rssiErr);
            // Continue, but log the warning. The channel was set successfully.
        }
    }

    return rv;
}

// --- Functions for Group ID, Destination ID, Equipment ID ---
// These follow the same pattern as SetChannel, including /W handling

MU_Modem_Error MU_Modem::GetGroupID(uint8_t *pGI)
{
    if (!pGI)
        return MU_Modem_Error::InvalidArg;
    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
        return MU_Modem_Error::Busy;
    m_WriteString(F(MU_GET_GROUP_STRING));
    MU_Modem_Error rv = m_WaitCmdResponse();
    if (rv == MU_Modem_Error::Ok)
    {
        rv = m_HandleMessageHexByte(pGI, MU_SET_GROUP_RESPONSE_LEN, MU_SET_GROUP_RESPONSE_PREFIX);
    }
    return rv;
}

MU_Modem_Error MU_Modem::SetGroupID(uint8_t gi, bool saveValue)
{
    // Implementation is very similar to SetChannel, replace "@CH" with "@GI", "*CH=" with "*GI=", etc.
    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
        return MU_Modem_Error::Busy;

    m_WriteString(F(MU_SET_GROUP_PREFIX_STRING));
    char numStr[7];
    snprintf(numStr, sizeof(numStr), "%02X%s\r\n", gi, (saveValue ? ("/W") : ("")));
    m_WriteString(numStr);

    MU_Modem_Error rv = m_WaitCmdResponse();
    if (rv != MU_Modem_Error::Ok)
        return rv;

    if (saveValue)
    {
        rv = m_HandleMessage_WR();
        if (rv == MU_Modem_Error::Ok)
        {
            rv = m_WaitCmdResponse();
            if (rv != MU_Modem_Error::Ok)
                return rv;
        }
        else
        {
            return MU_Modem_Error::Fail; // Missing WR response
        }
    }

    uint8_t giResponse{};
    if (rv == MU_Modem_Error::Ok)
    {
        MU_Modem_Error handleRv = m_HandleMessageHexByte(&giResponse, MU_SET_GROUP_RESPONSE_LEN, MU_SET_GROUP_RESPONSE_PREFIX);
        if (handleRv != MU_Modem_Error::Ok)
            rv = handleRv;
    }

    if (rv == MU_Modem_Error::Ok && giResponse != gi)
    {
        rv = MU_Modem_Error::Fail; // Mismatch
    }
    return rv;
}

MU_Modem_Error MU_Modem::GetDestinationID(uint8_t *pDI)
{
    if (!pDI)
        return MU_Modem_Error::InvalidArg;
    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
        return MU_Modem_Error::Busy;
    m_WriteString(F(MU_GET_DESTINATION_STRING));
    MU_Modem_Error rv = m_WaitCmdResponse();
    if (rv == MU_Modem_Error::Ok)
    {
        rv = m_HandleMessageHexByte(pDI, MU_SET_DESTINATION_RESPONSE_LEN, MU_SET_DESTINATION_RESPONSE_PREFIX);
    }
    return rv;
}

MU_Modem_Error MU_Modem::SetDestinationID(uint8_t di, bool saveValue)
{
    // Implementation is very similar to SetChannel
    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
        return MU_Modem_Error::Busy;

    m_WriteString(F(MU_SET_DESTINATION_PREFIX_STRING));
    char numStr[7];
    snprintf(numStr, sizeof(numStr), "%02X%s\r\n", di, (saveValue ? ("/W") : ("")));
    m_WriteString(numStr);

    MU_Modem_Error rv = m_WaitCmdResponse();
    if (rv != MU_Modem_Error::Ok)
        return rv;

    if (saveValue)
    {
        rv = m_HandleMessage_WR();
        if (rv == MU_Modem_Error::Ok)
        {
            rv = m_WaitCmdResponse();
            if (rv != MU_Modem_Error::Ok)
                return rv;
        }
        else
        {
            return MU_Modem_Error::Fail; // Missing WR response
        }
    }

    uint8_t diResponse{};
    if (rv == MU_Modem_Error::Ok)
    {
        MU_Modem_Error handleRv = m_HandleMessageHexByte(&diResponse, MU_SET_DESTINATION_RESPONSE_LEN, MU_SET_DESTINATION_RESPONSE_PREFIX);
        if (handleRv != MU_Modem_Error::Ok)
            rv = handleRv;
    }

    if (rv == MU_Modem_Error::Ok && diResponse != di)
    {
        rv = MU_Modem_Error::Fail; // Mismatch
    }
    return rv;
}

MU_Modem_Error MU_Modem::GetEquipmentID(uint8_t *pEI)
{
    if (!pEI)
        return MU_Modem_Error::InvalidArg;
    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
        return MU_Modem_Error::Busy;
    m_WriteString(F(MU_GET_EQUIPMENT_STRING));
    MU_Modem_Error rv = m_WaitCmdResponse();
    if (rv == MU_Modem_Error::Ok)
    {
        rv = m_HandleMessageHexByte(pEI, MU_SET_EQUIPMENT_RESPONSE_LEN, MU_SET_EQUIPMENT_RESPONSE_PREFIX);
    }
    return rv;
}

MU_Modem_Error MU_Modem::SetEquipmentID(uint8_t ei, bool saveValue)
{
    // Implementation is very similar to SetChannel
    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
        return MU_Modem_Error::Busy;

    m_WriteString(F(MU_SET_EQUIPMENT_PREFIX_STRING));
    char numStr[7];
    snprintf(numStr, sizeof(numStr), "%02X%s\r\n", ei, (saveValue ? ("/W") : ("")));
    m_WriteString(numStr);

    MU_Modem_Error rv = m_WaitCmdResponse();
    if (rv != MU_Modem_Error::Ok)
        return rv;

    if (saveValue)
    {
        rv = m_HandleMessage_WR();
        if (rv == MU_Modem_Error::Ok)
        {
            rv = m_WaitCmdResponse();
            if (rv != MU_Modem_Error::Ok)
                return rv;
        }
        else
        {
            return MU_Modem_Error::Fail; // Missing WR response
        }
    }

    uint8_t eiResponse{};
    if (rv == MU_Modem_Error::Ok)
    {
        MU_Modem_Error handleRv = m_HandleMessageHexByte(&eiResponse, MU_SET_EQUIPMENT_RESPONSE_LEN, MU_SET_EQUIPMENT_RESPONSE_PREFIX);
        if (handleRv != MU_Modem_Error::Ok)
            rv = handleRv;
    }

    if (rv == MU_Modem_Error::Ok && eiResponse != ei)
    {
        rv = MU_Modem_Error::Fail; // Mismatch
    }
    return rv;
}

MU_Modem_Error MU_Modem::GetUserID(uint16_t *pUI)
{
    if (!pUI)
        return MU_Modem_Error::InvalidArg;
    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
    {
        return MU_Modem_Error::Busy;
    }

    m_WriteString(F(MU_GET_USER_ID_STRING));

    MU_Modem_Error rv = m_WaitCmdResponse();

    if (rv == MU_Modem_Error::Ok)
    {
        // Check message length and prefix
        if (m_rxIdx != MU_GET_USER_ID_RESPONSE_LEN ||
            strncmp(MU_GET_USER_ID_RESPONSE_PREFIX, (char *)&m_rxMessage[0], strlen(MU_GET_USER_ID_RESPONSE_PREFIX)) != 0)
        {
            return MU_Modem_Error::Fail;
        }

        // Parse the 4 hex digits after "*UI="
        uint32_t value = 0;
        size_t prefixLen = strlen(MU_GET_USER_ID_RESPONSE_PREFIX);
        if (s_ParseHex(&m_rxMessage[prefixLen], 4, &value))
        {
            *pUI = (uint16_t)value;
            rv = MU_Modem_Error::Ok;
        }
        else
        {
            rv = MU_Modem_Error::Fail;
        }
    }
    return rv;
}

MU_Modem_Error MU_Modem::GetRssiCurrentChannel(int16_t *pRssi)
{
    if (!pRssi)
        return MU_Modem_Error::InvalidArg;
    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
    {
        return MU_Modem_Error::Busy;
    }

    m_WriteString(F(MU_GET_RSSI_CURRENT_CHANNEL_STRING));

    MU_Modem_Error rv = m_WaitCmdResponse();

    if (rv == MU_Modem_Error::Ok)
    {
        rv = m_HandleMessage_RA(pRssi); // Use the dedicated handler
    }
    else
    {
        *pRssi = 0; // Set to 0 on communication failure
    }

    return rv;
}

MU_Modem_Error MU_Modem::GetAllChannelsRssi(int16_t *pRssiBuffer, size_t bufferSize, uint8_t *pNumRssiValues)
{
    if (!pRssiBuffer || !pNumRssiValues)
        return MU_Modem_Error::InvalidArg;

    *pNumRssiValues = 0; // Initialize output

    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
    {
        return MU_Modem_Error::Busy;
    }

    // Determine expected number of channels and response length based on model
    size_t expectedNumChannels = 0;
    size_t expectedResponseLen = 0;
    if (m_frequencyModel == MU_Modem_FrequencyModel::MHz_429)
    {
        expectedNumChannels = MU_NUM_CHANNELS_429;
        expectedResponseLen = MU_GET_RSSI_ALL_CHANNELS_RESPONSE_LEN_429;
    }
    else
    { // MHz_1216
        expectedNumChannels = MU_NUM_CHANNELS_1216;
        expectedResponseLen = MU_GET_RSSI_ALL_CHANNELS_RESPONSE_LEN_1216;
    }

    if (bufferSize < expectedNumChannels)
    {
        MU_DEBUG_PRINTF("[MU_Modem] GetAllChannelsRssi: Buffer too small. Required: %zu, Provided: %zu\n", expectedNumChannels, bufferSize);
        return MU_Modem_Error::BufferTooSmall;
    }

    m_WriteString(F(MU_GET_RSSI_ALL_CHANNELS_STRING));

    MU_Modem_Error rv = m_WaitCmdResponse(2500); // Use a longer timeout as this command can take time

    if (rv != MU_Modem_Error::Ok)
    {
        return rv;
    }

    // --- Parse the response ---
    size_t prefixLen = strlen(MU_GET_RSSI_ALL_CHANNELS_RESPONSE_PREFIX);

    // Check length and prefix
    if (m_rxIdx != expectedResponseLen || strncmp(MU_GET_RSSI_ALL_CHANNELS_RESPONSE_PREFIX, (char *)m_rxMessage, prefixLen) != 0)
    {
        return MU_Modem_Error::Fail;
    }

    // Parse the concatenated hex values
    const uint8_t *pData = &m_rxMessage[prefixLen];
    for (size_t i = 0; i < expectedNumChannels; ++i)
    {
        uint32_t hexValue;
        if (s_ParseHex(pData + (i * 2), 2, &hexValue))
        {
            // Per documentation, the value is negative dBm
            pRssiBuffer[i] = -static_cast<int16_t>(hexValue);
        }
        else
        {
            // Invalidate buffer and return failure
            *pNumRssiValues = 0;
            return MU_Modem_Error::Fail;
        }
    }

    *pNumRssiValues = expectedNumChannels;

    return MU_Modem_Error::Ok;
}

MU_Modem_Error MU_Modem::GetRssiCurrentChannelAsync()
{
    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
    {
        return MU_Modem_Error::Busy;
    }

    m_WriteString(F(MU_GET_RSSI_CURRENT_CHANNEL_STRING));
    m_asyncExpectedResponse = MU_Modem_Response::RssiCurrentChannel;
    // Set a timeout for the async operation? Maybe handled in Work()
    m_StartTimeout(1000); // Start a 1-second timeout for this async command

    return MU_Modem_Error::Ok;
}

MU_Modem_Error MU_Modem::GetAllChannelsRssiAsync()
{
    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
    {
        return MU_Modem_Error::Busy;
    }

    m_WriteString(F(MU_GET_RSSI_ALL_CHANNELS_STRING));
    m_asyncExpectedResponse = MU_Modem_Response::RssiAllChannels;
    // Use a longer timeout for this command
    m_StartTimeout(2500);

    return MU_Modem_Error::Ok;
}

MU_Modem_Error MU_Modem::SetRouteInfo(const uint8_t *pRouteInfo, uint8_t numNodes, bool saveValue)
{
    if (!pRouteInfo || numNodes == 0 || numNodes > MU_MAX_ROUTE_NODES)
    {
        return MU_Modem_Error::InvalidArg;
    }

    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
    {
        return MU_Modem_Error::Busy;
    }

    // Generate command string (e.g., "@RT 01,02,8F/W\r\n")
    char cmdBuffer[MU_MAX_ROUTE_STR_LEN + 10]; // Prefix + Route + /W + CRLF + Null
    strcpy(cmdBuffer, MU_SET_ROUTE_PREFIX_STRING);
    strcat(cmdBuffer, " "); // Add space after @RT

    char nodeStr[4]; // "XX," or "XX"
    for (uint8_t i = 0; i < numNodes; ++i)
    {
        snprintf(nodeStr, sizeof(nodeStr), "%02X", pRouteInfo[i]);
        strcat(cmdBuffer, nodeStr);
        if (i < numNodes - 1)
        {
            strcat(cmdBuffer, ","); // Add comma separator
        }
    }

    // Add option and terminator
    if (saveValue)
    {
        strcat(cmdBuffer, "/W");
    }
    strcat(cmdBuffer, "\r\n");

    m_WriteString(cmdBuffer);

    // Wait for and process response (including *WR handling for saveValue)
    MU_Modem_Error rv = m_WaitCmdResponse();
    if (rv != MU_Modem_Error::Ok)
        return rv;

    if (saveValue)
    {
        rv = m_HandleMessage_WR();
        if (rv == MU_Modem_Error::Ok)
        {
            rv = m_WaitCmdResponse();
            if (rv != MU_Modem_Error::Ok)
                return rv;
        }
        else
        {
            return MU_Modem_Error::Fail;
        }
    }

    // Parse and validate the *RT=... response
    if (rv == MU_Modem_Error::Ok)
    {
        uint8_t responseRoute[MU_MAX_ROUTE_NODES];
        uint8_t responseNumNodes = 0;
        MU_Modem_Error handleRv = m_HandleMessage_RT(responseRoute, sizeof(responseRoute), &responseNumNodes);

        if (handleRv != MU_Modem_Error::Ok)
        {
            rv = handleRv; // Parse failed
        }
        else if (responseNumNodes != numNodes)
        {
            rv = MU_Modem_Error::Fail; // Node count mismatch
        }
        else
        {
            // If node count matches, compare content as well
            if (memcmp(pRouteInfo, responseRoute, numNodes) != 0)
            {
                // Log detailed mismatch content if necessary
                rv = MU_Modem_Error::Fail;
            }
        }
    }
    return rv;
}

MU_Modem_Error MU_Modem::GetRouteInfo(uint8_t *pRouteInfoBuffer, size_t bufferSize, uint8_t *pNumNodes)
{
    if (!pRouteInfoBuffer || bufferSize == 0 || !pNumNodes)
    {
        return MU_Modem_Error::InvalidArg;
    }

    *pNumNodes = 0; // Initialize output

    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
    {
        return MU_Modem_Error::Busy;
    }

    m_WriteString(F(MU_GET_ROUTE_STRING));

    MU_Modem_Error rv = m_WaitCmdResponse();

    if (rv == MU_Modem_Error::Ok)
    {
        rv = m_HandleMessage_RT(pRouteInfoBuffer, bufferSize, pNumNodes);
    }

    return rv;
}

MU_Modem_Error MU_Modem::ClearRouteInfo(bool saveValue)
{
    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
    {
        return MU_Modem_Error::Busy;
    }

    // Generate command string (e.g., "@RT NA/W\r\n")
    char cmdBuffer[20]; // "@RT NA/W\r\n" + Null
    strcpy(cmdBuffer, MU_SET_ROUTE_PREFIX_STRING);
    strcat(cmdBuffer, " ");
    strcat(cmdBuffer, MU_ROUTE_NA_STRING);
    if (saveValue)
    {
        strcat(cmdBuffer, "/W");
    }
    strcat(cmdBuffer, "\r\n");

    m_WriteString(cmdBuffer);

    // Wait for and process response (including *WR handling for saveValue)
    MU_Modem_Error rv = m_WaitCmdResponse();
    if (rv != MU_Modem_Error::Ok)
        return rv;

    if (saveValue)
    {
        rv = m_HandleMessage_WR();
        if (rv == MU_Modem_Error::Ok)
        {
            rv = m_WaitCmdResponse();
            if (rv != MU_Modem_Error::Ok)
                return rv;
        }
        else
        {
            return MU_Modem_Error::Fail;
        }
    }

    // Validate *RT=NA response
    if (rv == MU_Modem_Error::Ok)
    {
        const char *expectedResponse = "*RT=NA";
        size_t expectedLen = strlen(expectedResponse);
        if (m_rxIdx != expectedLen || strncmp(expectedResponse, (char *)m_rxMessage, expectedLen) != 0)
        {
            rv = MU_Modem_Error::Fail;
        }
    }
    return rv;
}

MU_Modem_Error MU_Modem::GetPower(uint8_t *pPower)
{
    if (!pPower)
        return MU_Modem_Error::InvalidArg;
    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
        return MU_Modem_Error::Busy;
    m_WriteString(F(MU_GET_POWER_STRING));
    MU_Modem_Error rv = m_WaitCmdResponse();
    if (rv == MU_Modem_Error::Ok)
    {
        rv = m_HandleMessageHexByte(pPower, MU_SET_POWER_RESPONSE_LEN, MU_SET_POWER_RESPONSE_PREFIX);
    }
    return rv;
}

MU_Modem_Error MU_Modem::SetPower(uint8_t power, bool saveValue)
{
    // Validate power setting (only 1mW (0x01) or 10mW (0x10) allowed)
    if (power != 0x01 && power != 0x10)
    {
        MU_DEBUG_PRINTF("[MU_Modem] SetPower: Invalid power value 0x%02X\n", power);
        return MU_Modem_Error::InvalidArg;
    }

    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
        return MU_Modem_Error::Busy;

    m_WriteString(F(MU_SET_POWER_PREFIX_STRING));
    char numStr[7];
    snprintf(numStr, sizeof(numStr), "%02X%s\r\n", power, (saveValue ? ("/W") : ("")));
    m_WriteString(numStr);

    MU_Modem_Error rv = m_WaitCmdResponse();
    if (rv != MU_Modem_Error::Ok)
        return rv;

    if (saveValue)
    {
        rv = m_HandleMessage_WR();
        if (rv == MU_Modem_Error::Ok)
        {
            rv = m_WaitCmdResponse();
            if (rv != MU_Modem_Error::Ok)
                return rv;
        }
        else
        {
            return MU_Modem_Error::Fail; // Missing WR response
        }
    }

    uint8_t powerResponse{};
    if (rv == MU_Modem_Error::Ok)
    {
        MU_Modem_Error handleRv = m_HandleMessageHexByte(&powerResponse, MU_SET_POWER_RESPONSE_LEN, MU_SET_POWER_RESPONSE_PREFIX);
        if (handleRv != MU_Modem_Error::Ok)
            rv = handleRv;
    }

    if (rv == MU_Modem_Error::Ok && powerResponse != power)
    {
        rv = MU_Modem_Error::Fail; // Mismatch
    }
    return rv;
}

MU_Modem_Error MU_Modem::GetSerialNumber(uint32_t *pSerialNumber)
{
    if (!pSerialNumber)
        return MU_Modem_Error::InvalidArg;
    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
    {
        return MU_Modem_Error::Busy;
    }

    m_WriteString(F(MU_GET_SERIAL_NUMBER_STRING));

    MU_Modem_Error rv = m_WaitCmdResponse();

    if (rv == MU_Modem_Error::Ok)
    {
        rv = m_HandleMessage_SN(pSerialNumber);
    }
    else
    {
        *pSerialNumber = 0; // Set to 0 on failure
    }

    return rv;
}

MU_Modem_Error MU_Modem::GetSerialNumberAsync()
{
    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
    {
        return MU_Modem_Error::Busy;
    }

    m_WriteString(F(MU_GET_SERIAL_NUMBER_STRING));
    m_asyncExpectedResponse = MU_Modem_Response::SerialNumber;
    m_StartTimeout(1000); // Start timeout for async operation

    return MU_Modem_Error::Ok;
}

MU_Modem_Error MU_Modem::GetPacket(const uint8_t **ppData, uint8_t *len)
{
    if (!ppData || !len)
        return MU_Modem_Error::InvalidArg;

    if (m_drMessagePresent)
    {
        *ppData = &m_drMessage[0];
        *len = m_drMessageLen;
        // Do NOT set m_drMessagePresent = false here. Let DeletePacket do that.
        return MU_Modem_Error::Ok;
    }
    else
    {
        *ppData = nullptr;
        *len = 0;
        return MU_Modem_Error::Fail;
    }
}

void MU_Modem::Work()
{
    // Check for async command timeout first
    if (m_asyncExpectedResponse != MU_Modem_Response::Idle && m_IsTimeout())
    {
        MU_DEBUG_PRINTF("[MU_Modem] Work: Async command (%d) timed out.\n", (int)m_asyncExpectedResponse);
        if (m_pCallback)
        {
            m_pCallback(MU_Modem_Error::Fail, m_asyncExpectedResponse, 0, nullptr, 0, nullptr, 0); // Notify timeout via callback
        }
        m_asyncExpectedResponse = MU_Modem_Response::Idle; // Reset expected response
        m_ResetParser();                                   // Reset parser state
    }

    // Process incoming serial data using the parser
    switch (m_Parse())
    {
    case MU_Modem_CmdState::Parsing:
        // Still waiting for more data, do nothing special
        break;

    case MU_Modem_CmdState::Garbage:
        MU_DEBUG_PRINTLN("[MU_Modem] Work: Parser encountered garbage.");
        // If an async command was expected, notify callback of failure due to garbage
        if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
        {
            if (m_pCallback)
            {
                m_pCallback(MU_Modem_Error::Fail, m_asyncExpectedResponse, 0, nullptr, 0, nullptr, 0);
            }
            m_asyncExpectedResponse = MU_Modem_Response::Idle; // Reset expected response
        }
        m_ResetParser(); // Reset parser after garbage
        break;
    case MU_Modem_CmdState::Overflow:
        MU_DEBUG_PRINTLN("[MU_Modem] Work: Parser encountered buffer overflow.");
        // If an async command was expected, notify callback of failure due to overflow
        if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
        {
            if (m_pCallback)
            {
                m_pCallback(MU_Modem_Error::Fail, m_asyncExpectedResponse, 0, nullptr, 0, nullptr, 0);
            }
            m_asyncExpectedResponse = MU_Modem_Response::Idle; // Reset expected response
        }
        m_ResetParser(); // Reset parser after overflow
        break;
    case MU_Modem_CmdState::FinishedCmdResponse:
        // A command response (like *CH=07, *SN=..., *RA=...) was received.
        // If it corresponds to an expected async command, dispatch it.
        if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
        {
            m_DispatchCmdResponseAsync(); // Handles callback and resets async state
        }
        else
        {
            // Received a command response when no async command was pending.
            // This can happen if a DR arrived *during* m_WaitCmdResponse, and then
            // the actual command response arrived later and is processed now by Work().
            // Or it could be an unsolicited message (though unlikely for standard responses).
            // Log it for debugging, but generally ignore it otherwise.
            MU_DEBUG_PRINTF("[MU_Modem] Work: Received unexpected CMD response: '%.*s'\n", m_rxIdx, m_rxMessage);
        }
        // Parser state is reset inside m_Parse() when FinishedCmdResponse is returned.
        break;
    case MU_Modem_CmdState::FinishedDrResponse:
        // A data packet (*DR=... or *DS=...) was received.
        if (m_pCallback)
        {
            // Notify via callback. Pass cached RSSI in 'value' parameter.
            m_pCallback(MU_Modem_Error::Ok, MU_Modem_Response::DataReceived, m_lastRxRSSI, &m_drMessage[0], m_drMessageLen, m_drRouteInfo, m_drNumRouteNodes);
        }
        // Parser state is reset inside m_Parse() when FinishedDrResponse is returned.
        break;
    default:
        // Should not happen
        MU_DEBUG_PRINTLN("[MU_Modem] Work: Unknown parser state!");
        m_ResetParser();
        break;
    }
}

// --- Internal Helper Functions ---

/**
 * @brief Checks if the timeout for a synchronous command has expired.
 * Also handles the case where the timer might not be active (bTimeout is true).
 * @return True if timeout has occurred or timer is inactive, false otherwise.
 */
bool MU_Modem::m_IsTimeout()
{
    // If the timer was already marked as timed out/inactive, return true.
    if (bTimeout)
    {
        return true;
    }

    // Check if the current time exceeds the timeout period.
    // Handles millis() rollover correctly for durations < ~49 days.
    if (millis() - startTime >= timeOut)
    {
        bTimeout = true;                                  // Mark as timed out
        MU_DEBUG_PRINTLN("[MU_Modem] Timeout occurred!"); // Optional debug
        return true;
    }

    // Timer is active, and time hasn't expired.
    return false;
}

void MU_Modem::m_StartTimeout(uint32_t ms /* = xTicksToWait */) // Default value is in header
{
    bTimeout = false; // Mark timer as active
    startTime = millis();
    timeOut = ms;
    MU_DEBUG_PRINTF("[MU_Modem] Timeout started: %lu ms\n", ms);
}

/**
 * @brief Clears the current timeout timer, marking it as inactive.
 */
void MU_Modem::m_ClearTimeout()
{
    bTimeout = true; // Mark timer as inactive (timed out state also uses true)
    MU_DEBUG_PRINTLN("[MU_Modem] Timeout cleared.");
}

/**
 * @brief Writes a null-terminated string to the UART and debug stream.
 */
void MU_Modem::m_WriteString(const char *pString)
{
    if (!pString)
        return;
    size_t len = strlen(pString);
    // Log TX data to debug stream *before* sending to UART
    MU_DEBUG_PRINT("[MU_Modem] TX: ");
    // The write macro already checks for m_pDebugStream
    MU_DEBUG_WRITE(reinterpret_cast<const uint8_t *>(pString), len);
    // Ensure newline is printed if the string doesn't end with one explicitly for logging
    if (len < 2 || pString[len - 2] != '\r' || pString[len - 1] != '\n')
    {
        // m_pDebugStream->println(); // Add newline for readability if command doesn't end with \r\n (unlikely)
    }
    m_pUart->write(reinterpret_cast<const uint8_t *>(pString), len);
}

/**
 * @brief Writes a string from Flash memory to the UART and debug stream.
 */
void MU_Modem::m_WriteString(const __FlashStringHelper *pString)
{
    if (!pString)
        return;
    MU_DEBUG_PRINT("[MU_Modem] TX: ");
    MU_DEBUG_PRINT(pString); // Assumes print handles flash strings
    m_pUart->print(pString);
}

/**
 * @brief Reads a single byte from UART, handling the one-byte unread buffer. Logs to debug stream.
 */
uint8_t MU_Modem::m_ReadByte()
{
    int rcv_int = -1;
    if (m_oneByteBuf != -1)
    {
        rcv_int = m_oneByteBuf;
        m_oneByteBuf = -1; // Consume the byte from buffer
    }
    else if (m_pUart->available())
    {
        rcv_int = m_pUart->read();
    }

    uint8_t rcv = (rcv_int != -1) ? static_cast<uint8_t>(rcv_int) : 0; // Return 0 if no byte read

    // Log RX byte only if one was actually read
    if (rcv_int != -1)
    {
        if (rcv >= 32 && rcv <= 126)
        { // Printable char
            MU_DEBUG_WRITE(rcv);
        }
        else if (rcv == '\r')
        { // Carriage Return
            MU_DEBUG_PRINT("<CR>");
        }
        else if (rcv == '\n')
        {                             // Line Feed
            MU_DEBUG_PRINT("<LF>\n"); // Add newline for log readability
        }
        else
        { // Other non-printable
            MU_DEBUG_PRINTF("<%02X>", rcv);
        }
    }

    return rcv;
}

/**
 * @brief Puts a byte back into the one-byte buffer to be read next.
 */
void MU_Modem::m_UnreadByte(uint8_t unreadByte)
{
    m_oneByteBuf = unreadByte;
}

/**
 * @brief Clears the one-byte unread buffer.
 */
void MU_Modem::m_ClearUnreadByte()
{
    m_oneByteBuf = -1;
}

// m_Read function (reads multiple bytes) seems unused currently.

/**
 * @brief Resets the parser state machine and clears the unread buffer.
 */
void MU_Modem::m_ResetParser()
{
    // Only log reset if not already in Start state.
    // Use braces to avoid -Wempty-body warning when debug is disabled.
    if (m_parserState != MU_Modem_ParserState::Start)
    {
        MU_DEBUG_PRINTLN("\n[MU_Modem] Parser Reset");
    }
    m_parserState = MU_Modem_ParserState::Start;
    m_ClearUnreadByte();
    m_rxIdx = 0;                // Reset receive buffer index as well
    m_drNumRouteNodes = 0;      // Reset route info count
    m_drMessagePresent = false; // Reset DR message flag
}

/**
 * @brief Reads and discards data from UART until a newline or timeout.
 * Used to clear unexpected data.
 */
void MU_Modem::m_ClearOneLine()
{
    MU_DEBUG_PRINT("[MU_Modem] Clearing line...");
    // Temporarily set a short timeout
    unsigned long start = millis();
    m_pUart->setTimeout(100); // 100ms timeout for clearing a line
    // Read until newline or timeout
    char dummy[2]; // Read one char at a time to check content
    while (millis() - start < 100)
    {
        size_t readCount = m_pUart->readBytes(dummy, 1);
        if (readCount > 0)
        {
            MU_DEBUG_WRITE(dummy[0]); // Echo cleared char
            if (dummy[0] == '\n')
                break;        // Stop after newline
            start = millis(); // Reset timeout timer on receiving char
        }
        else
        {
            delay(1); // Small delay if nothing available
        }
    }
    m_pUart->setTimeout(1000); // Restore default timeout (or whatever m_WaitCmdResponse uses)
    MU_DEBUG_PRINTLN(" Cleared.");
}

/**
 * @brief Discards any remaining data in the UART receive buffer and resets parser.
 */
void MU_Modem::m_FlushGarbage()
{
    MU_DEBUG_PRINT("[MU_Modem] Flushing garbage...");
    while (m_pUart->available())
    {
        m_ReadByte(); // Read and discard, also logs discarded chars
    }
    MU_DEBUG_PRINTLN(" Flushed.");
    m_ResetParser(); // Reset parser state after flushing
}

/**
 * @brief The core state machine for parsing incoming modem responses.
 * Processes bytes from the UART buffer via m_ReadByte().
 * @return MU_Modem_CmdState indicating the current parsing status.
 */
MU_Modem_CmdState MU_Modem::m_Parse()
{
    // Process available bytes one by one through the state machine
    while (m_pUart->available() || m_oneByteBuf != -1) // Check unread buffer too
    {
        // Read next byte (handles unread buffer)
        // Store current state for potential logging on state change
        // MU_Modem_ParserState prevState = m_parserState;
        uint8_t currentByte = m_ReadByte();

        // --- State Machine Logic ---
        switch (m_parserState)
        {
        case MU_Modem_ParserState::Start:
            m_rxIdx = 0;                // Reset index for new message
            m_drMessagePresent = false; // Ensure DR flag is clear at start
            m_drNumRouteNodes = 0;      // Clear route info count
            if (currentByte == '*')
            {
                m_rxMessage[m_rxIdx++] = currentByte;
                m_parserState = MU_Modem_ParserState::ReadCmdFirstLetter;
            }
            else if (currentByte != '\r' && currentByte != '\n' && currentByte != 0) // Ignore stray CR/LF/NULL
            {
                // Received unexpected non-'*' character
                MU_DEBUG_PRINTF("\n[MU_Modem] Parse Error (Start): Expected '*', got 0x%02X. Flushing.\n", currentByte);
                m_FlushGarbage(); // Flush and reset
                return MU_Modem_CmdState::Garbage;
            }
            // else: Ignore stray CR/LF/NULL, remain in Start state
            break;

        case MU_Modem_ParserState::ReadCmdFirstLetter:
            if (isupper(currentByte))
            {
                m_rxMessage[m_rxIdx++] = currentByte;
                m_parserState = MU_Modem_ParserState::ReadCmdSecondLetter;
            }
            else if (currentByte == '\r' || currentByte == '\n')
            {
                // Incomplete command ('*') - ignore and reset
                MU_DEBUG_PRINTLN("\n[MU_Modem] Parse Warning (L1): Incomplete cmd '*', resetting.");
                m_ResetParser();
                // Don't return Garbage yet, could be just noise before next valid '*'
            }
            else
            {
                MU_DEBUG_PRINTF("\n[MU_Modem] Parse Error (L1): Expected A-Z, got 0x%02X. Flushing.\n", currentByte);
                m_FlushGarbage();
                return MU_Modem_CmdState::Garbage;
            }
            break;

        case MU_Modem_ParserState::ReadCmdSecondLetter:
            if (isupper(currentByte))
            {
                m_rxMessage[m_rxIdx++] = currentByte;
                m_parserState = MU_Modem_ParserState::ReadCmdParam; // Expect '=' or payload next
            }
            else if (currentByte == '\r' || currentByte == '\n')
            {
                // Incomplete command ('*X') - ignore and reset
                MU_DEBUG_PRINTF("\n[MU_Modem] Parse Warning (L2): Incomplete cmd '*%.*s', resetting.\n", m_rxIdx - 1, &m_rxMessage[1]);
                m_ResetParser();
            }
            else
            {
                MU_DEBUG_PRINTF("\n[MU_Modem] Parse Error (L2): Expected A-Z, got 0x%02X. Flushing.\n", currentByte);
                m_FlushGarbage();
                return MU_Modem_CmdState::Garbage;
            }
            break;

        case MU_Modem_ParserState::ReadCmdParam:
            if (currentByte == '=')
            {
                m_rxMessage[m_rxIdx++] = currentByte;
                // Check command type based on first two letters (*XX=)
                if (m_rxIdx == 4)
                { // We have "*XX="
                    if (m_rxMessage[1] == 'D' && m_rxMessage[2] == 'R')
                    {
                        m_parserState = MU_Modem_ParserState::RadioDrSize; // Expect 2 hex digits for size
                    }
                    else if (m_rxMessage[1] == 'D' && m_rxMessage[2] == 'S')
                    {
                        m_parserState = MU_Modem_ParserState::ReadDsRSSI; // Expect 2 hex digits for RSSI
                    }
                    else
                    {
                        // Other standard command response like *CH=, *GI=, *SN= etc.
                        m_parserState = MU_Modem_ParserState::ReadCmdUntilCR; // Read value until CR
                    }
                }
                else
                { // Should not happen if state logic is correct
                    MU_DEBUG_PRINTF("\n[MU_Modem] Parse Error (Param): Unexpected index %u after '='. Flushing.\n", m_rxIdx);
                    m_FlushGarbage();
                    return MU_Modem_CmdState::Garbage;
                }
            }
            else if (currentByte == '\r')
            {
                // Response without '=' ? e.g. maybe some error response? Or incomplete?
                // Treat as end of simple command for now, check length later.
                m_parserState = MU_Modem_ParserState::ReadCmdUntilLF;
            }
            else if (currentByte == '\n')
            {
                // Received LF directly after command letters - treat as garbage/incomplete
                MU_DEBUG_PRINTF("\n[MU_Modem] Parse Error (Param): Unexpected LF after '*%.*s'. Flushing.\n", m_rxIdx - 1, &m_rxMessage[1]);
                m_FlushGarbage();
                return MU_Modem_CmdState::Garbage;
            }
            else
            {
                // Character other than '=' or CR after command letters? Could be part of SN value?
                // Let ReadCmdUntilCR handle it, but this might be less robust.
                // For now, assume most commands have '=', go to ReadCmdUntilCR state.
                m_rxMessage[m_rxIdx++] = currentByte; // Store the char
                m_parserState = MU_Modem_ParserState::ReadCmdUntilCR;
                // Add buffer overflow check here
                if (m_rxIdx >= sizeof(m_rxMessage))
                {
                    MU_DEBUG_PRINTLN("\n[MU_Modem] Parse Error (Param->CR): Buffer overflow. Flushing.");
                    m_FlushGarbage();
                    return MU_Modem_CmdState::Overflow;
                }
            }
            break;

        case MU_Modem_ParserState::ReadDsRSSI: // Expecting 2 hex digits for RSSI after *DS=
            m_rxMessage[m_rxIdx++] = currentByte;
            if (m_rxIdx == 6)
            { // Have "*DS=XX"
                uint32_t rssi_hex = 0;
                if (isxdigit(m_rxMessage[4]) && isxdigit(m_rxMessage[5]) && s_ParseHex(&m_rxMessage[4], 2, &rssi_hex))
                {
                    m_lastRxRSSI = -static_cast<int16_t>(rssi_hex); // Store RSSI (negative dBm)
                    // Now expect the payload size (like *DR=)
                    m_rxIdx = 4;          // Overwrite RSSI hex digits in buffer to match *DR= format internally
                    m_rxMessage[1] = 'R'; // Change S to R conceptually
                    m_parserState = MU_Modem_ParserState::RadioDrSize;
                }
                else
                {
                    MU_DEBUG_PRINTF("\n[MU_Modem] Parse Error (DS RSSI): Invalid hex '%c%c'. Flushing.\n", m_rxMessage[4], m_rxMessage[5]);
                    m_FlushGarbage();
                    return MU_Modem_CmdState::Garbage;
                }
            }
            else if (m_rxIdx > 6)
            { // Should not happen
                MU_DEBUG_PRINTLN("\n[MU_Modem] Parse Error (DS RSSI): Index overflow. Flushing.");
                m_FlushGarbage();
                return MU_Modem_CmdState::Garbage;
            }
            // else: waiting for second hex digit
            break;

        case MU_Modem_ParserState::RadioDrSize: // Expecting 2 hex digits for size after *DR=
            m_rxMessage[m_rxIdx++] = currentByte;
            if (m_rxIdx == 6)
            { // Have "*DR=XX"
                uint32_t msgLen = 0;
                if (isxdigit(m_rxMessage[4]) && isxdigit(m_rxMessage[5]) && s_ParseHex(&m_rxMessage[4], 2, &msgLen))
                {
                    if (msgLen <= sizeof(m_drMessage))
                    { // Check if length fits buffer
                        m_drMessageLen = static_cast<uint8_t>(msgLen);
                        m_rxIdx = 0; // Reset index for drMessage buffer
                        m_parserState = MU_Modem_ParserState::RadioDrPayload;
                    }
                    else
                    {
                        MU_DEBUG_PRINTF("\n[MU_Modem] Parse Error (DR Size): Declared length %u exceeds buffer size %zu. Flushing.\n", msgLen, sizeof(m_drMessage));
                        m_FlushGarbage();
                        return MU_Modem_CmdState::Overflow; // Or maybe Garbage?
                    }
                }
                else
                {
                    MU_DEBUG_PRINTF("\n[MU_Modem] Parse Error (DR Size): Invalid hex '%c%c'. Flushing.\n", m_rxMessage[4], m_rxMessage[5]);
                    m_FlushGarbage();
                    return MU_Modem_CmdState::Garbage;
                }
            }
            else if (m_rxIdx > 6)
            { // Should not happen
                MU_DEBUG_PRINTLN("\n[MU_Modem] Parse Error (DR Size): Index overflow. Flushing.");
                m_FlushGarbage();
                return MU_Modem_CmdState::Garbage;
            }
            // else: waiting for second hex digit
            break;

        case MU_Modem_ParserState::RadioDrPayload:
            // Read payload bytes + optional route info + CR LF
            m_drMessage[m_rxIdx++] = currentByte;

            // Have we received payload + CR + LF ? (m_drMessageLen + 2)
            // Or have we potentially received payload + '/' + 'R' ? (m_drMessageLen + 2)
            // Or have we potentially received payload + CR ? (m_drMessageLen + 1)
            // Be careful with index checks. m_rxIdx is the count of bytes *received so far* for this state.

            if (m_rxIdx == m_drMessageLen)
            {
                // Just received the last byte of declared payload. Next should be CR or '/'.
                m_parserState = MU_Modem_ParserState::ReadOptionUntilCR; // Expect CR or '/'
            }
            else if (m_rxIdx == m_drMessageLen + 1)
            {
                // Received one byte after payload. Check if it's CR or '/'.
                if (currentByte == '\r')
                {
                    m_parserState = MU_Modem_ParserState::ReadOptionUntilLF; // Expect LF next
                }
                else if (m_drMessage[m_rxIdx - 1] == '/')
                {
                    m_parserState = MU_Modem_ParserState::ReadOptionUntilCR; // Expect option data + CR next
                }
                else
                {
                    // Unexpected char after payload
                    MU_DEBUG_PRINTF("\n[MU_Modem] Parse Error (DR Payload): Unexpected char 0x%02X after payload. Flushing.\n", m_drMessage[m_rxIdx - 1]);
                    m_FlushGarbage();
                    return MU_Modem_CmdState::Garbage;
                }
            }
            else if (m_rxIdx > m_drMessageLen + 1)
            {
                // Should be handled by ReadOption states if '/' or CR was received correctly.
                // If we are here, something went wrong, or payload had unexpected chars.
                // Let ReadOption states handle potential CR/LF, but add overflow check.
                if (m_rxIdx >= sizeof(m_drMessage))
                {
                    MU_DEBUG_PRINTLN("\nParse Error (DR Payload): Buffer overflow. Flushing.");
                    m_FlushGarbage();
                    return MU_Modem_CmdState::Overflow;
                }
                // Assume we are reading option data if '/' was previously seen
                // This requires ReadOption states to handle the final CR/LF detection correctly.
                // Stay in RadioDrPayload might be wrong here. Let's refine ReadOption states.
                // --> Changed logic: Now rely on ReadOption states triggered above.
                // --> If we reach here unexpectedly, it implies an error.
                // if (m_pDebugStream) m_pDebugStream->printf("\nParse Error (DR Payload): Unexpected state. Idx=%u, Len=%u. Flushing.\n", m_rxIdx, m_drMessageLen);
                // m_FlushGarbage(); return MU_Modem_CmdState::Garbage;
                // Let's assume it might be option data and let ReadOption handle it.
                // Reverted: State transitions handle this. This case shouldn't be reached normally
                // if '/' or CR follows payload. If neither follows, it's garbage.
                // Re-reverted: Let ReadOption handle final CR/LF. If currentByte is CR here, go to LF state.
                if (currentByte == '\r')
                {
                    m_parserState = MU_Modem_ParserState::ReadOptionUntilLF;
                }
                else
                {
                    // Stay in RadioDrPayload (effectively ReadOptionUntilCR)
                    m_parserState = MU_Modem_ParserState::ReadOptionUntilCR;
                }
            }
            // Check for buffer overflow within the payload read loop
            if (m_rxIdx >= sizeof(m_drMessage))
            {
                MU_DEBUG_PRINTLN("\n[MU_Modem] Parse Error (DR Payload Loop): Buffer overflow. Flushing.");
                m_FlushGarbage();
                return MU_Modem_CmdState::Overflow;
            }
            break;

        case MU_Modem_ParserState::ReadCmdUntilCR:
            // Reading parameters/value after '=' until CR
            if (currentByte == '\r')
            {
                m_parserState = MU_Modem_ParserState::ReadCmdUntilLF;
            }
            else if (currentByte == '\n')
            {
                // Unexpected LF without preceding CR
                MU_DEBUG_PRINTLN("\n[MU_Modem] Parse Error (Cmd->CR): Unexpected LF. Flushing.");
                m_FlushGarbage();
                return MU_Modem_CmdState::Garbage;
            }
            else
            {
                m_rxMessage[m_rxIdx++] = currentByte; // Append char to buffer
                // Check for buffer overflow
                if (m_rxIdx >= sizeof(m_rxMessage))
                {
                    MU_DEBUG_PRINTLN("\n[MU_Modem] Parse Error (Cmd->CR): Buffer overflow. Flushing.");
                    m_FlushGarbage();
                    return MU_Modem_CmdState::Overflow;
                }
            }
            break;

        case MU_Modem_ParserState::ReadCmdUntilLF:
            // Expecting LF after CR for command response
            if (currentByte == '\n')
            {
                // Finished command response successfully
                m_rxMessage[m_rxIdx] = '\0';                 // Null-terminate the received command string (excluding CR LF)
                m_parserState = MU_Modem_ParserState::Start; // Reset state for next message
                return MU_Modem_CmdState::FinishedCmdResponse;
            }
            else
            {
                // Did not get LF after CR
                MU_DEBUG_PRINTF("\n[MU_Modem] Parse Error (Cmd->LF): Expected LF, got 0x%02X. Flushing.\n", currentByte);
                m_FlushGarbage();
                return MU_Modem_CmdState::Garbage;
            }
            break; // Not strictly needed

        case MU_Modem_ParserState::ReadOptionUntilCR: // Reading DR optional data after '/'
            if (currentByte == '\r')
            {
                m_parserState = MU_Modem_ParserState::ReadOptionUntilLF; // Expect LF next
            }
            else if (currentByte == '\n')
            {
                // Unexpected LF without preceding CR
                MU_DEBUG_PRINTLN("\n[MU_Modem] Parse Error (Opt->CR): Unexpected LF. Flushing.");
                m_FlushGarbage();
                return MU_Modem_CmdState::Garbage;
            }
            else
            {
                m_drMessage[m_rxIdx++] = currentByte; // Append char to buffer
                // Check for buffer overflow
                if (m_rxIdx >= sizeof(m_drMessage))
                {
                    MU_DEBUG_PRINTLN("\n[MU_Modem] Parse Error (Opt->CR): Buffer overflow. Flushing.");
                    m_FlushGarbage();
                    return MU_Modem_CmdState::Overflow;
                }
            }
            break;

        case MU_Modem_ParserState::ReadOptionUntilLF: // Expecting LF after CR for DR message
            if (currentByte == '\n')
            {
                // Finished DR response successfully
                // Check if we have route info to parse (e.g., payload + "/R 01,02,03")
                // The sequence must start exactly after the payload data. Check for "/R"
                if (m_rxIdx > m_drMessageLen + 2 && m_drMessage[m_drMessageLen] == '/' && m_drMessage[m_drMessageLen + 1] == 'R')
                {
                    // Found "/R" after payload. Check for optional space.
                    bool space_present = (m_drMessage[m_drMessageLen + 2] == ' ');
                    const char *routeStr = (const char *)m_drMessage + m_drMessageLen + (space_present ? 3 : 2);
                    size_t routeStrLen = m_rxIdx - (routeStr - (const char *)m_drMessage);

                    // Use a temporary buffer to parse the route string
                    char tempRouteStr[MU_MAX_ROUTE_STR_LEN + 1];
                    memcpy(tempRouteStr, routeStr, routeStrLen);
                    tempRouteStr[routeStrLen] = '\0';

                    // Re-use m_HandleMessage_RT logic by simulating a "*RT=" response
                    // This is a bit of a hack but avoids code duplication.
                    char simulatedResponse[MU_MAX_ROUTE_STR_LEN + 10];
                    sprintf(simulatedResponse, "*RT=%s", tempRouteStr);
                    m_rxIdx = strlen(simulatedResponse);
                    memcpy(m_rxMessage, simulatedResponse, m_rxIdx);

                    m_HandleMessage_RT(m_drRouteInfo, sizeof(m_drRouteInfo), &m_drNumRouteNodes);
                }

                m_drMessagePresent = true;                   // Mark packet as ready
                m_parserState = MU_Modem_ParserState::Start; // Reset state for next message
                return MU_Modem_CmdState::FinishedDrResponse;
            }
            else
            {
                // Did not get LF after CR
                MU_DEBUG_PRINTF("\n[MU_Modem] Parse Error (Opt->LF): Expected LF, got 0x%02X. Flushing.\n", currentByte);
                m_FlushGarbage();
                return MU_Modem_CmdState::Garbage;
            }
            break; // Not strictly needed

        default:
            // Should never happen, indicates logic error
            MU_DEBUG_PRINTF("\n[MU_Modem] Parse Error: Reached unexpected state %d. Resetting.\n", (int)m_parserState);
            m_ResetParser();
            break;
        } // End switch

        // Log state change if debugging
        // if (m_pDebugStream && m_parserState != prevState) {
        //     m_pDebugStream->printf(" -> State %d\n", (int)m_parserState);
        // }

    } // End while(available)

    // If UART buffer is empty but we haven't finished parsing, return Parsing state
    return MU_Modem_CmdState::Parsing;
}

// Internal helper function: Parse *RT=... response
MU_Modem_Error MU_Modem::m_HandleMessage_RT(uint8_t *pDestBuffer, size_t bufferSize, uint8_t *pNumNodes)
{
    if (!pDestBuffer || bufferSize == 0 || !pNumNodes)
        return MU_Modem_Error::InvalidArg;

    *pNumNodes = 0; // Initialize

    size_t prefixLen = strlen(MU_SET_ROUTE_RESPONSE_PREFIX); // "*RT="

    // Check prefix
    if (m_rxIdx < prefixLen || strncmp(MU_SET_ROUTE_RESPONSE_PREFIX, (const char *)m_rxMessage, prefixLen) != 0)
    {
        return MU_Modem_Error::Fail;
    }

    // Check for "NA"
    if (m_rxIdx == MU_SET_ROUTE_RESPONSE_LEN && strncmp((const char *)m_rxMessage, "*RT=NA", MU_SET_ROUTE_RESPONSE_LEN) == 0)
    {
        *pNumNodes = 0;
        return MU_Modem_Error::Ok;
    }

    // Parse comma-separated hex values
    const char *routeStr = (const char *)m_rxMessage + prefixLen;
    size_t routeStrLen = m_rxIdx - prefixLen;
    uint8_t nodeCount = 0;
    const char *pStart = routeStr;
    const char *pEnd = routeStr + routeStrLen;

    while (pStart < pEnd && nodeCount < bufferSize && nodeCount < MU_MAX_ROUTE_NODES)
    {
        const char *pComma = (const char *)memchr(pStart, ',', pEnd - pStart);
        size_t hexLen = (pComma != nullptr) ? (pComma - pStart) : (pEnd - pStart);

        if (hexLen != 2)
        { // Expect 2 hex digits per node
            return MU_Modem_Error::Fail;
        }

        uint32_t hexValue;
        if (s_ParseHex((const uint8_t *)pStart, 2, &hexValue))
        {
            pDestBuffer[nodeCount++] = (uint8_t)hexValue;
        }
        else
        {
            return MU_Modem_Error::Fail;
        }

        if (pComma != nullptr)
        {
            pStart = pComma + 1; // Move past the comma
        }
        else
        {
            pStart = pEnd; // Reached the end
        }
    }

    if (pStart < pEnd)
    {                                          // Check if loop terminated due to buffer full or max nodes reached
        return MU_Modem_Error::BufferTooSmall; // Or a different error?
    }

    *pNumNodes = nodeCount;
    return MU_Modem_Error::Ok;
}

/**
 * @brief Waits synchronously for a complete command response (*...) from the modem.
 * Handles intervening *DR messages by calling the callback if set.
 * @param ms Timeout duration in milliseconds.
 * @return MU_Modem_Error::Ok if a command response is received,
 * MU_Modem_Error::Fail if timeout or parsing error occurs.
 */
MU_Modem_Error MU_Modem::m_WaitCmdResponse(uint32_t ms)
{
    MU_DEBUG_PRINTF("[MU_Modem] m_WaitCmdResponse: Waiting up to %lu ms...\n", ms);
    m_StartTimeout(ms); // Start the timeout timer

    while (!m_IsTimeout())
    {
        switch (m_Parse())
        {
        case MU_Modem_CmdState::Parsing:
            // Still waiting for data or haven't received a complete line yet
            // A small delay prevents busy-waiting. Arduino's delay() is simple.
            // Be careful: delay() can interfere with other timing-sensitive operations.
            // If using FreeRTOS or similar, vTaskDelay is preferred.
            delay(1); // Yield for 1ms
            break;

        case MU_Modem_CmdState::FinishedCmdResponse:
            // Successfully received a command response line (e.g., "*CH=07")
            MU_DEBUG_PRINTF("[MU_Modem] m_WaitCmdResponse: Finished CMD response received: '%.*s'\n", m_rxIdx, m_rxMessage);
            return MU_Modem_Error::Ok; // Success

        case MU_Modem_CmdState::FinishedDrResponse:
            // Received a data packet (*DR=...) while waiting for a command response.
            MU_DEBUG_PRINTF("[MU_Modem] m_WaitCmdResponse: Intervening DR received (Len=%u, RSSI=%d, RouteNodes=%u). Calling callback...\n", m_drMessageLen, m_lastRxRSSI, m_drNumRouteNodes);
            if (m_pCallback)
            {
                // Call the user's callback function to handle the data packet
                m_pCallback(MU_Modem_Error::Ok, MU_Modem_Response::DataReceived, m_lastRxRSSI, m_drMessage, m_drMessageLen, m_drRouteInfo, m_drNumRouteNodes);
            }
            else
            {
                // No callback, but DR was received. Keep m_drMessagePresent true.
                MU_DEBUG_PRINTLN("[MU_Modem] m_WaitCmdResponse: No callback for DR.");
                // The DR message remains available via HasPacket()/GetPacket().
            }
            // Parser state is implicitly reset by m_Parse returning FinishedDrResponse.
            // Continue waiting for the original command response.
            MU_DEBUG_PRINTLN("[MU_Modem] m_WaitCmdResponse: Continuing to wait for original CMD response...");
            break; // Continue the while loop

        case MU_Modem_CmdState::Garbage:
            MU_DEBUG_PRINTLN("[MU_Modem] m_WaitCmdResponse: Parser encountered garbage.");
            // Parser already flushed and reset by m_Parse() returning Garbage
            return MU_Modem_Error::Fail; // Indicate failure

        case MU_Modem_CmdState::Overflow:
            MU_DEBUG_PRINTLN("[MU_Modem] m_WaitCmdResponse: Parser encountered overflow.");
            // Parser already flushed and reset by m_Parse() returning Overflow
            return MU_Modem_Error::Fail; // Indicate failure

        default:
            // Should not happen
            MU_DEBUG_PRINTLN("[MU_Modem] m_WaitCmdResponse: Unexpected state from m_Parse.");
            m_ResetParser();             // Reset just in case
            return MU_Modem_Error::Fail; // Indicate failure
        }
    }

    // If the loop finishes, it means timeout occurred
    MU_DEBUG_PRINTLN("[MU_Modem] m_WaitCmdResponse: Timeout.");
    m_ResetParser();             // Reset parser state on timeout
    return MU_Modem_Error::Fail; // Indicate failure due to timeout
}

// Function seems unused currently
void MU_Modem::m_SetExpectedResponses(MU_Modem_Response ep0, MU_Modem_Response ep1, MU_Modem_Response ep2)
{
    m_asyncExpectedResponses[0] = ep0;
    m_asyncExpectedResponses[1] = ep1;
    m_asyncExpectedResponses[2] = ep2;
}

/**
 * @brief Dispatches the received command response (`m_rxMessage`) for an asynchronous operation.
 * Calls the appropriate handler, extracts the result, and notifies via callback.
 * Resets the async expected state.
 * @return MU_Modem_Error::Ok if dispatched successfully, MU_Modem_Error::Fail otherwise.
 */
MU_Modem_Error MU_Modem::m_DispatchCmdResponseAsync()
{
    MU_Modem_Error err = MU_Modem_Error::Fail; // Default to fail
    int32_t value = 0;                         // To store result like RSSI or SN (using int32_t for flexibility)
    const uint8_t *payloadPtr = nullptr;       // For potential payload (e.g., raw response)
    uint16_t payloadLen = 0;

    // Store expected response locally and clear global state immediately
    // This prevents race conditions if a new async command is issued from the callback
    MU_Modem_Response expected = m_asyncExpectedResponse;
    m_asyncExpectedResponse = MU_Modem_Response::Idle;
    m_ClearTimeout(); // Clear the async timeout timer

    switch (expected)
    {
    case MU_Modem_Response::Idle:
        // Should not happen if called correctly from Work()
        MU_DEBUG_PRINTLN("[MU_Modem] m_DispatchCmdResponseAsync: Called with Idle state.");
        break; // err remains Fail

    case MU_Modem_Response::SerialNumber:
        uint32_t sn_val;
        err = m_HandleMessage_SN(&sn_val);    // Parse *SN=... response
        value = static_cast<int32_t>(sn_val); // Store SN in value
        break;

    case MU_Modem_Response::RssiCurrentChannel:
        int16_t rssi_val;
        err = m_HandleMessage_RA(&rssi_val);    // Parse *RA=... response
        value = static_cast<int32_t>(rssi_val); // Store RSSI in value
        break;

    case MU_Modem_Response::RssiAllChannels:
        // For this response, we pass the raw message buffer as the payload
        // The user is responsible for parsing it.
        // We just validate the prefix and length here to determine the error code.
        {
            size_t expectedLen = (m_frequencyModel == MU_Modem_FrequencyModel::MHz_429)
                                     ? MU_GET_RSSI_ALL_CHANNELS_RESPONSE_LEN_429
                                     : MU_GET_RSSI_ALL_CHANNELS_RESPONSE_LEN_1216;

            if (m_rxIdx == expectedLen && strncmp(MU_GET_RSSI_ALL_CHANNELS_RESPONSE_PREFIX, (char *)m_rxMessage, strlen(MU_GET_RSSI_ALL_CHANNELS_RESPONSE_PREFIX)) == 0)
            {
                err = MU_Modem_Error::Ok;
                payloadPtr = m_rxMessage;
                payloadLen = m_rxIdx;
            }
            else
            {
                err = MU_Modem_Error::Fail;
            }
        }
        break;

    case MU_Modem_Response::GenericResponse: // Used by SendRawCommand
        // No specific parsing needed, just pass the raw buffer content
        payloadPtr = m_rxMessage;
        payloadLen = m_rxIdx;     // Length excluding CR/LF
        err = MU_Modem_Error::Ok; // Assume OK if we got a response line
        break;

        // Add cases for other potential async commands here
        // case MU_Modem_Response::Channel:
        // case MU_Modem_Response::GroupID:
        // ... etc.

    default:
        MU_DEBUG_PRINTF("[MU_Modem] m_DispatchCmdResponseAsync: Unhandled expected response type %d\n", (int)expected);
        err = MU_Modem_Error::Fail; // Unhandled type
        break;
    }

    // Call the callback function with the result
    if (m_pCallback)
    {
        m_pCallback(err, expected, value, payloadPtr, payloadLen, nullptr, 0);
    }
    else
    {
        MU_DEBUG_PRINTLN("[MU_Modem] m_DispatchCmdResponseAsync: No callback set to deliver result.");
    }

    return err; // Return the status of handling the response
}

/**
 * @brief Checks if the received message in m_rxMessage is a valid "*WR=PS" response.
 * @return MU_Modem_Error::Ok if it matches, MU_Modem_Error::Fail otherwise.
 */
MU_Modem_Error MU_Modem::m_HandleMessage_WR()
{
    if ((m_rxIdx == MU_WRITE_VALUE_RESPONSE_LEN) &&
        (strncmp(MU_WRITE_VALUE_RESPONSE_PREFIX, (char *)&m_rxMessage[0], MU_WRITE_VALUE_RESPONSE_LEN) == 0))
    {
        return MU_Modem_Error::Ok;
    }
    return MU_Modem_Error::Fail;
}

/**
 * @brief Handles common command responses with a 2-hex-digit value (e.g., *CH=XX, *GI=XX).
 * Checks prefix, length, and parses the hex value.
 * @param pValue Pointer to store the parsed byte value.
 * @param responseLen Expected total length of the response string (excluding CR LF).
 * @param responsePrefix Expected prefix string (e.g., "*CH=").
 * @return MU_Modem_Error::Ok on success, MU_Modem_Error::Fail on error (wrong length, prefix, or invalid hex).
 */
MU_Modem_Error MU_Modem::m_HandleMessageHexByte(uint8_t *pValue, uint32_t responseLen, const char *responsePrefix)
{
    if (!pValue || !responsePrefix)
        return MU_Modem_Error::InvalidArg;

    size_t prefixLen = strlen(responsePrefix);
    // Check total length and prefix length validity
    if (responseLen != prefixLen + 2)
    {
        return MU_Modem_Error::Fail; // Internal logic error
    }

    // Check actual received length
    if (m_rxIdx != responseLen)
    {
        return MU_Modem_Error::Fail; // Message wrong size
    }

    // Check prefix
    if (strncmp(responsePrefix, (char *)&m_rxMessage[0], prefixLen) != 0)
    {
        return MU_Modem_Error::Fail; // Wrong prefix
    }

    // Parse the 2 hex digits
    uint32_t value{};
    if (s_ParseHex(&m_rxMessage[prefixLen], 2, &value))
    {
        *pValue = static_cast<uint8_t>(value);
        return MU_Modem_Error::Ok; // Success
    }
    else
    {
        return MU_Modem_Error::Fail; // Invalid hex characters
    }
}

/**
 * @brief Checks if the received message is a valid *RA=XX response and parses the RSSI value.
 * @param pRssi Pointer to store the parsed RSSI value (negative dBm).
 * @return MU_Modem_Error::Ok on success, MU_Modem_Error::Fail otherwise.
 */
MU_Modem_Error MU_Modem::m_HandleMessage_RA(int16_t *pRssi)
{
    if (!pRssi)
        return MU_Modem_Error::InvalidArg;
    uint8_t rssi_hex_val;
    MU_Modem_Error rv = m_HandleMessageHexByte(&rssi_hex_val, MU_GET_RSSI_CURRENT_CHANNEL_RESPONSE_LEN, MU_GET_RSSI_CURRENT_CHANNEL_RESPONSE_PREFIX);
    if (rv == MU_Modem_Error::Ok)
    {
        *pRssi = -static_cast<int16_t>(rssi_hex_val); // Convert hex value to negative dBm
    }
    else
    {
        *pRssi = 0; // Set to 0 on parsing failure
    }
    return rv;
}

/**
 * @brief Checks if the received message is a valid *SN=... response and parses the serial number.
 * Handles potential leading alpha character (e.g., *SN=A12345678).
 * @param pSerialNumber Pointer to store the parsed serial number (numeric part only).
 * @return MU_Modem_Error::Ok on success, MU_Modem_Error::Fail otherwise.
 */
MU_Modem_Error MU_Modem::m_HandleMessage_SN(uint32_t *pSerialNumber)
{
    if (!pSerialNumber)
        return MU_Modem_Error::InvalidArg;

    size_t prefixLen = strlen(MU_GET_SERIAL_NUMBER_RESPONSE_PREFIX);

    // Check minimum length and prefix
    if (m_rxIdx < MU_GET_SERIAL_NUMBER_RESPONSE_MIN_LEN ||
        strncmp(MU_GET_SERIAL_NUMBER_RESPONSE_PREFIX, (char *)&m_rxMessage[0], prefixLen) != 0)
    {
        return MU_Modem_Error::Fail;
    }

    const uint8_t *startPtr = &m_rxMessage[prefixLen]; // Point after "*SN="
    uint8_t parseLen = m_rxIdx - prefixLen;            // Length of the remaining part

    // Check for and skip optional leading alpha character (like 'A' in MU-4 SN)
    if (parseLen > 0 && isalpha(*startPtr))
    {
        startPtr++;
        parseLen--;
    }

    // Parse the remaining digits as a decimal number.
    if (parseLen > 0)
    {
        // Create a null-terminated string from the number part to use with atoi
        char numStr[16]; // Buffer for the number string, large enough for a 32-bit integer
        if (parseLen < sizeof(numStr))
        {
            memcpy(numStr, startPtr, parseLen);
            numStr[parseLen] = '\0';
            *pSerialNumber = (uint32_t)atol(numStr); // Use atol for long
            return MU_Modem_Error::Ok;
        }
    }

    // If parsing fails or length is zero
    return MU_Modem_Error::Fail; // Parsing failed
}

// --- Function: SendRawCommand ---
/**
 * @brief Sends a raw command string and waits for a single-line response.
 * @param command The null-terminated command string to send (must include \r\n).
 * @param responseBuffer Buffer to store the received response (excluding \r\n).
 * @param bufferSize Size of the responseBuffer.
 * @param timeoutMs Timeout duration in milliseconds to wait for the response.
 * @return MU_Modem_Error::Ok on success (response in buffer),
 * MU_Modem_Error::Fail on timeout or parse error,
 * MU_Modem_Error::BufferTooSmall if response exceeds bufferSize,
 * MU_Modem_Error::Busy if another async command is pending.
 */
MU_Modem_Error MU_Modem::SendRawCommand(const char *command, char *responseBuffer, size_t bufferSize, uint32_t timeoutMs)
{
    if (!command || !responseBuffer || bufferSize == 0)
    {
        MU_DEBUG_PRINTLN("[MU_Modem] SendRawCommand: Invalid args.");
        return MU_Modem_Error::InvalidArg;
    }
    // Basic check for command format (optional but helpful)
    size_t cmdLen = strlen(command);
    if (cmdLen < 3 || command[0] != '@' || command[cmdLen - 2] != '\r' || command[cmdLen - 1] != '\n')
    {
        MU_DEBUG_PRINTLN("[MU_Modem] SendRawCommand: Warning! Command format might be incorrect (should be @...\\r\\n).");
        // Proceed anyway, but log a warning.
    }

    if (m_asyncExpectedResponse != MU_Modem_Response::Idle)
    {
        MU_DEBUG_PRINTLN("[MU_Modem] SendRawCommand: Busy with async command.");
        return MU_Modem_Error::Busy;
    }

    m_WriteString(command);

    MU_Modem_Error rv = m_WaitCmdResponse(timeoutMs);

    if (rv == MU_Modem_Error::Ok)
    {
        // Check if response fits in the provided buffer
        if (m_rxIdx < bufferSize)
        {
            memcpy(responseBuffer, m_rxMessage, m_rxIdx);
            responseBuffer[m_rxIdx] = '\0'; // Null-terminate
            return MU_Modem_Error::Ok;
        }
        else
        {
            responseBuffer[0] = '\0'; // Clear buffer
            return MU_Modem_Error::BufferTooSmall;
        }
    }
    else
    {
        // m_WaitCmdResponse failed (Timeout or Parse Error)
        responseBuffer[0] = '\0'; // Clear buffer
        return rv;                // Return the error from m_WaitCmdResponse
    }
}
