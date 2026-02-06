/**
 * @file MU_Modem.h
 * @brief Interface driver for MU FSK modem.
 *
 * This file defines the MU_Modem class, which provides an interface
 * for controlling the MU-3 FSK modem manufactured by Circuit Design, Inc.
 * It handles sending commands, receiving responses, and parsing data packets.
 */
//
// The original program
// (c) 2019 Reimesch Kommunikationssysteme
// Authors: aj, cl
// Created on: 13.03.2019
// Released under the MIT license
//
// (c) 2026 CircuitDesign,Inc.
// Interface driver for MU-3/MU-4 (FSK modem manufactured by Circuit Design)

#pragma once
#include <Arduino.h>
#include "common/SerialModemBase.h"

/**
 * @brief Default baud rate for the MU modem.
 */
static constexpr uint32_t MU_DEFAULT_BAUDRATE = 19200;

/**
 * @brief Channel range constants for MU modem models.
 */
static constexpr uint8_t MU_CHANNEL_MIN_429 = 0x07;  ///!< Minimum channel number for 429MHz model
static constexpr uint8_t MU_CHANNEL_MAX_429 = 0x2E;  ///!< Maximum channel number for 429MHz model
static constexpr uint8_t MU_CHANNEL_MIN_1216 = 0x02; ///!< Minimum channel number for 1.2GHz model
static constexpr uint8_t MU_CHANNEL_MAX_1216 = 0x14; ///!< Maximum channel number for 1.2GHz model

/**
 * @brief Maximum payload and route node constants.
 */
static constexpr uint8_t MU_MAX_PAYLOAD_LEN = 255;
static constexpr uint8_t MU_MAX_ROUTE_NODES_IN_DR = 12; //!< Max route nodes in a *DR response (src + 10 relays + dest)

// --- Debug Configuration ---
// Uncomment the following line to enable debug output
// #define ENABLE_MU_MODEM_DEBUG

/**
 * @enum MU_Modem_Response
 * @brief Defines the types of responses from the modem or internal states.
 */
enum class MU_Modem_Response
{
    Idle,       //!< No message received or expected.
    ParseError, //!< Garbage characters received.
    Timeout,    //!< No response received within the timeout period.

    // Serial commands responses
    ShowMode,           //!< Response indicating the modem's mode.
    SaveValue,          //!< Response confirming a value has been written to NVM ("*WR=PS").
    Channel,            //!< Response related to the frequency channel ("*CH...").
    SerialNumber,       //!< Response containing the device's serial number ("*SN=...").
    MU_Modem_DtAck,     //!< Acknowledgment for the @DT command.
    DataReceived,       //!< Indicates that a data packet has been received ("*DR=..." or "*DS=...").
    RssiCurrentChannel, //!< Response containing the current RSSI value ("*RA=...").
    RssiAllChannels,    //!< Response containing RSSI values for all channels ("*RC=...").
    RouteInfo,          //!< Response containing route information ("*RT=...").
    GenericResponse,    //!< Generic response received from SendRawCommand.
};

/**
 * @enum MU_Modem_Error
 * @brief Defines API level error codes.
 */
using MU_Modem_Error = ModemError;

/**
 * @enum MU_Modem_FrequencyModel
 * @brief Defines the frequency model of the MU modem.
 */
enum class MU_Modem_FrequencyModel
{
    MHz_429, //!< 429 MHz model
    MHz_1216 //!< 1216 MHz model
};

/**
 * @enum MU_Modem_ParserState (Internal)
 * @brief Represents the low-level state of the command parser state machine.
 */
enum class MU_Modem_ParserState
{
    Start = 0,

    ReadCmdFirstLetter,  //!< First char was '*', now reading the first letter.
    ReadCmdSecondLetter, //!< Reading the second letter.
    ReadCmdParam,        //!< Reading command parameters (e.g., after '*XX').

    RadioDrSize,    //!< Parsing the size of a *DR (data received) message.
    RadioDsRssi,    //!< Parsing the RSSI of a *DS (data received with RSSI) message.
    RadioDrPayload, //!< Reading the payload of a *DR/*DS message.

    ReadCmdUntilCR,    //!< Reading until a carriage return ('\r') is found.
    ReadCmdUntilLF,    //!< Reading until a line feed ('\n') is found.
    ReadOptionUntilCR, //!< Reading optional data (like route info) until '\r'.
    ReadOptionUntilLF, //!< Reading optional data until '\n'.
};

/**
 * @brief Callback function type for asynchronous operations and received data events.
 * @param error Status of the received response. If not MU_Modem_Error::Ok, other parameters may be invalid.
 * @param responseType The type of the response or event (MU_Modem_Response).
 * @param value A numerical value associated with the response (e.g., RSSI).
 * @param pPayload Pointer to the payload of a received data packet.
 * @param len Length of the received payload in bytes.
 * @param pRouteInfo Pointer to an array containing the route information of the received packet.
 * @param numRouteNodes Number of nodes in the pRouteInfo array.
 */
typedef void (*MU_Modem_AsyncCallback)(MU_Modem_Error error, MU_Modem_Response responseType, int32_t value, const uint8_t *pPayload, uint16_t len, const uint8_t *pRouteInfo, uint8_t numRouteNodes);

/**
 * @class MU_Modem
 * @brief Provides an interface to control the MU FSK modem.
 */
class MU_Modem : public SerialModemBase
{
public: // methods
    // --- Initialization & Lifecycle ---

    /**
     * @brief Initializes the modem driver.
     * @param pUart A reference to the Stream object (e.g., Serial1).
     * @param frequencyModel The frequency model of the modem.
     * @param pCallback A pointer to the callback function.
     * @return MU_Modem_Error::Ok on success.
     */
    MU_Modem_Error begin(Stream &pUart, MU_Modem_FrequencyModel frequencyModel, MU_Modem_AsyncCallback pCallback = nullptr);

    /**
     * @brief Main processing loop.
     */
    void Work();

    /**
     * @brief Performs a software reset.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error SoftReset();

    // --- Configuration & Debug ---

    /**
     * @brief Sets the asynchronous callback function.
     * @param pCallback A pointer to the callback function. Set to nullptr to disable callbacks.
     */
    void SetAsyncCallback(MU_Modem_AsyncCallback pCallback) { m_pCallback = pCallback; }

    /**
     * @brief Sets the stream for debug output.
     * @param debugStream Pointer to the Stream object (e.g., &Serial). Set to nullptr to disable debug output.
     */
    // void setDebugStream(Stream *debugStream); // Inherited from SerialModemBase

    // --- Data Transmission ---

    /**
     * @brief Transmits a data packet.
     * @param pMsg Pointer to the data buffer to transmit.
     * @param len Length of the data in bytes.
     * @param useRouteRegister If true, appends the /R option to use the route register.
     * @return MU_Modem_Error::Ok on success, MU_Modem_Error::FailLbt if busy.
     */
    MU_Modem_Error TransmitData(const uint8_t *pMsg, uint8_t len, bool useRouteRegister = false);

    /**
     * @brief Transmits a data packet without waiting for transmission completion.
     * @param pMsg Pointer to the data buffer to transmit.
     * @param len Length of the data in bytes.
     * @param useRouteRegister If true, appends the /R option to use the route register.
     * @return MU_Modem_Error::Ok if command accepted.
     */
    MU_Modem_Error TransmitDataFireAndForget(const uint8_t *pMsg, uint8_t len, bool useRouteRegister = false);

    /**
     * @brief Transmits data with explicit route information.
     * @param pRouteInfo Pointer to an array containing the route information (relay station IDs and destination ID).
     * @param numNodes The number of IDs in the route information (1 <= numNodes <= 11).
     * @param pMsg Pointer to the data buffer to transmit.
     * @param len Length of the data in bytes.
     * @param requestAck If true, requests an ACK response from the destination station (/A option).
     * @param outputToRelays If true, outputs the data to relay stations as well (/B or /S option).
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error TransmitDataWithRoute(const uint8_t *pRouteInfo, uint8_t numNodes, const uint8_t *pMsg, uint8_t len, bool requestAck = false, bool outputToRelays = false);

    // --- Data Reception ---

    /**
     * @brief Checks if a packet is available.
     */
    bool HasPacket() { return m_drMessagePresent; }

    /**
     * @brief Retrieves the packet data.
     * @param ppData Pointer to a pointer that will be set to the packet's data buffer.
     * @param len Pointer to a variable to store the packet's length.
     * @return MU_Modem_Error::Ok if a packet was retrieved, MU_Modem_Error::Fail otherwise.
     */
    MU_Modem_Error GetPacket(const uint8_t **ppData, uint8_t *len);

    /**
     * @brief Deletes the current packet.
     */
    void DeletePacket() { m_drMessagePresent = false; }

    // --- Modem Status & RSSI ---

    /**
     * @brief Checks the current channel status (@CS).
     * @return
     * - MU_Modem_Error::Ok if the channel is clear (*CS=EN).
     * - MU_Modem_Error::FailLbt if the channel is busy (*CS=DI).
     * - MU_Modem_Error::Fail on timeout or communication error.
     */
    MU_Modem_Error CheckCarrierSense();

    /**
     * @brief Gets the current RSSI value of the channel.
     * @param pRssi Pointer to a variable to store the RSSI value in dBm.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error GetRssiCurrentChannel(int16_t *pRssi);

    /**
     * @brief Asynchronously requests the current RSSI value.
     * @return MU_Modem_Error::Ok if the request was sent successfully, MU_Modem_Error::Busy if busy.
     */
    MU_Modem_Error GetRssiCurrentChannelAsync();

    /**
     * @brief Gets RSSI values for all channels.
     * @param pRssiBuffer Pointer to an array to store the RSSI values in dBm.
     * @param bufferSize The size of the pRssiBuffer in number of elements (int16_t).
     *                   It must be large enough to hold all channel data.
     * @param pNumRssiValues Pointer to a variable to store the number of RSSI values actually written to the buffer.
     * @return MU_Modem_Error::Ok on success, MU_Modem_Error::BufferTooSmall if the buffer is not large enough, or other error codes.
     */
    MU_Modem_Error GetAllChannelsRssi(int16_t *pRssiBuffer, size_t bufferSize, uint8_t *pNumRssiValues);

    /**
     * @brief Asynchronously requests RSSI values for all channels.
     * @return MU_Modem_Error::Ok if the request was sent successfully, MU_Modem_Error::Busy if busy.
     */
    MU_Modem_Error GetAllChannelsRssiAsync();

    /**
     * @brief Enables RSSI appending to received data (*DS).
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error SetAddRssiValue();

    // --- Device Information ---

    /**
     * @brief Gets the serial number.
     * @param pSn Pointer to a variable to store the serial number.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error GetSerialNumber(uint32_t *pSerialNumber);

    /**
     * @brief Asynchronously requests the serial number.
     * @return MU_Modem_Error::Ok if the request was sent successfully, MU_Modem_Error::Busy if busy.
     */
    MU_Modem_Error GetSerialNumberAsync();

    /**
     * @brief Gets the User ID.
     * @param pUI Pointer to a variable to store the User ID.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error GetUserID(uint16_t *pUI);

    // --- Basic Settings ---

    /**
     * @brief Sets the UART baud rate of the modem.
     * The change is applied immediately after the modem sends its response.
     * @warning This function does NOT reconfigure the host's UART.
     * The caller MUST reconfigure the host's UART baud rate (e.g., via `Serial.begin()` or `Serial.updateBaudRate()`)
     * immediately after this function returns `Ok`.
     * @param baudRate The target baud rate. Supported values: 1200, 2400, 4800, 9600, 19200, 38400, 57600.
     * @param saveValue If true, saves the setting to non-volatile memory.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error SetBaudRate(uint32_t baudRate, bool saveValue);

    /**
     * @brief Sets the frequency channel.
     * @param channel The channel number to set. Valid range depends on the frequency model.
     * @param saveValue If true, saves the setting to non-volatile memory.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error SetChannel(uint8_t channel, bool saveValue);

    /**
     * @brief Gets the current frequency channel.
     * @param pChannel Pointer to a variable to store the channel number.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error GetChannel(uint8_t *pChannel);

    /**
     * @brief Sets the transmission power.
     * @param power The power setting to set (0x01 for 1mW, 0x10 for 10mW).
     * @param saveValue If true, saves the setting to non-volatile memory.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error SetPower(uint8_t power, bool saveValue);

    /**
     * @brief Gets the transmission power setting.
     * @param pPower Pointer to a variable to store the power setting (0x01 for 1mW, 0x10 for 10mW).
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error GetPower(uint8_t *pPower);

    // --- ID Settings ---

    /**
     * @brief Sets the Destination ID.
     * @param di The Destination ID to set (0x00 - 0xFF).
     * @param saveValue If true, saves the setting to non-volatile memory.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error SetDestinationID(uint8_t di, bool saveValue);

    /**
     * @brief Gets the Destination ID.
     * @param pDI Pointer to a variable to store the Destination ID.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error GetDestinationID(uint8_t *pDI);

    /**
     * @brief Sets the Equipment ID.
     * @param ei The Equipment ID to set (0x00 - 0xFF).
     * @param saveValue If true, saves the setting to non-volatile memory.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error SetEquipmentID(uint8_t ei, bool saveValue);

    /**
     * @brief Gets the Equipment ID.
     * @param pEI Pointer to a variable to store the Equipment ID.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error GetEquipmentID(uint8_t *pEI);

    /**
     * @brief Sets the Group ID.
     * @param gi The Group ID to set (0x00 - 0xFF).
     * @param saveValue If true, saves the setting to non-volatile memory.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error SetGroupID(uint8_t gi, bool saveValue);

    /**
     * @brief Gets the Group ID.
     * @param pGI Pointer to a variable to store the Group ID.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error GetGroupID(uint8_t *pGI);

    /**
     * @brief Sets the route info add mode setting.
     * @brief Sets the route info add mode setting.
     * @param enabled True to enable (@RI ON), false to disable (@RI OF).
     * @param saveValue If true, saves the setting to non-volatile memory.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error SetRouteInfoAddMode(bool enabled, bool saveValue);

    // --- Routing Settings ---

    /**
     * @brief Gets the route info add mode setting.
     * @param pEnabled Pointer to a boolean to store the result (true if ON, false if OFF).
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error GetRouteInfoAddMode(bool *pEnabled);

    /**
     * @brief Sets the auto reply route setting.
     * @param enabled If true, enables the feature (@RR ON). When a data packet with route information is received,
     * the modem automatically generates a return route and writes it to the route register. If false, disables the feature (@RR OF).
     * @param saveValue If true, saves the setting to non-volatile memory.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error SetAutoReplyRoute(bool enabled, bool saveValue);

    /**
     * @brief Gets the auto reply route setting.
     * @param pEnabled Pointer to a boolean to store the result (true if ON, false if OFF).
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error GetAutoReplyRoute(bool *pEnabled);

    /**
     * @brief Sets the relay route information.
     * @param pRouteInfo Pointer to an array containing the route information (relay station IDs and destination ID).
     * @param numNodes The number of IDs in the route information (1 <= numNodes <= 11).
     * @param saveValue If true, saves the setting to non-volatile memory.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error SetRouteInfo(const uint8_t *pRouteInfo, uint8_t numNodes, bool saveValue);

    /**
     * @brief Gets the relay route information.
     * @param pRouteInfoBuffer Pointer to a buffer to store the route information.
     * @param bufferSize The size of the buffer in bytes.
     * @param pNumNodes Pointer to a variable to store the number of retrieved IDs. Will be set to 0 if the route is not available ("NA").
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error GetRouteInfo(uint8_t *pRouteInfoBuffer, size_t bufferSize, uint8_t *pNumNodes);

    /**
     * @brief Clears the route information.
     * @param saveValue If true, saves the setting to non-volatile memory.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error ClearRouteInfo(bool saveValue);

    /**
     * @brief Sends a raw command.
     * @param command The null-terminated command string to send.
     * @param responseBuffer Buffer to store the raw response line from the modem (excluding CRLF).
     * @param bufferSize Size of the responseBuffer.
     * @param timeoutMs Timeout in milliseconds to wait for the response.
     * @return MU_Modem_Error::Ok on success, MU_Modem_Error::Timeout if no response,
     * MU_Modem_Error::Busy if another operation is in progress,
     * MU_Modem_Error::BufferTooSmall if the buffer is too small,
     * MU_Modem_Error::Fail on other errors.
     */
    MU_Modem_Error SendRawCommand(const char *command, char *responseBuffer, size_t bufferSize, uint32_t timeoutMs = 500);

    /**
     * @brief Sends a raw command asynchronously.
     */
    MU_Modem_Error SendRawCommandAsync(const char *command, uint32_t timeoutMs = 500);

protected:
    // --- SerialModemBase Overrides ---
    virtual ModemParseResult parse() override;
    virtual void onRxDataReceived() override;
    virtual const char *getLogPrefix() const override { return "[MU Modem] "; }

private: // meth
    // Parser helpers
    void m_ResetParser();

    // Parsing specific responses
    MU_Modem_Error m_HandleMessage_RT(uint8_t *pDestBuffer, size_t bufferSize, uint8_t *pNumNodes);
    MU_Modem_Error m_ProcessSaveResponse(bool saveValue);
    MU_Modem_Error m_DispatchCmdResponseAsync();

    MU_Modem_Error m_HandleMessage_WR();
    MU_Modem_Error m_HandleMessage_RA(int16_t *pRssi);
    MU_Modem_Error m_HandleMessage_SN(uint32_t *pSerialNumber);

    // Generic Value Helpers
    MU_Modem_Error m_SendCmd(const char *cmd);
    MU_Modem_Error m_SetByteValue(const char *cmdPrefix, uint8_t value, bool saveValue, const char *respPrefix, size_t respLen);
    MU_Modem_Error m_GetByteValue(const char *cmdPrefix, uint8_t *pValue, const char *respPrefix, size_t respLen);

    MU_Modem_Error m_HandleMessageHexByte(uint8_t *pValue, uint32_t responseLen, const char *responsePrefix);
    MU_Modem_Error m_HandleMessageHexWord(uint16_t *pValue, uint32_t responseLen, const char *responsePrefix);

private:                                      // data
    MU_Modem_AsyncCallback m_pCallback;       //!< User callback.
    MU_Modem_FrequencyModel m_frequencyModel; //!< Frequency model.

    // Parser State
    MU_Modem_ParserState m_parserState; //!< Internal parser state.

    // Received Packet Data
    bool m_drMessagePresent;                         //!< Packet ready flag.
    uint8_t m_drMessageLen;                          //!< Packet length.
    uint8_t m_drMessage[300];                        //!< Packet buffer.
    int16_t m_lastRxRSSI;                            //!< Last packet RSSI.
    uint8_t m_drRouteInfo[MU_MAX_ROUTE_NODES_IN_DR]; //!< Packet route info.
    uint8_t m_drNumRouteNodes;                       //!< Packet route info count.

    // Async State
    MU_Modem_Response m_asyncExpectedResponse; //!< Expected async response.
};