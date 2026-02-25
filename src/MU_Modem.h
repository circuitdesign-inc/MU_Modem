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
 * @brief Channel range constants.
 */
static constexpr uint8_t MU_CHANNEL_MIN_429 = 0x07;  //!< Minimum channel number for 429MHz model
static constexpr uint8_t MU_CHANNEL_MAX_429 = 0x2E;  //!< Maximum channel number for 429MHz model
static constexpr uint8_t MU_CHANNEL_MIN_1216 = 0x02; //!< Minimum channel number for 1.2GHz model
static constexpr uint8_t MU_CHANNEL_MAX_1216 = 0x14; //!< Maximum channel number for 1.2GHz model

static constexpr uint8_t MU_MAX_PAYLOAD_LEN = 255;      //!< Maximum payload and route node constants.
static constexpr uint8_t MU_MAX_ROUTE_NODES_IN_DR = 12; //!< Max route nodes in a *DR response (src + 10 relays + dest)

/**
 * @enum MU_Modem_Response
 * @brief Defines the types of responses from the modem.
 */
enum class MU_Modem_Response
{
    Idle,         //!< No message received or expected.
    ParseError,   //!< Garbage characters received.
    Timeout,      //!< No response received within the timeout period.
    TxComplete,   //!< Transmission accepted (*DT response received)
    TxFailed,     //!< Transmission failed (LBT Error or NACK)
    DataReceived, //!< Data packet received
    // Serial command responses
    ShowMode,           //!< Response indicating the modem's mode.
    SaveValue,          //!< Response confirming a value has been written to NVM ("*WR=PS").
    Channel,            //!< Response related to the frequency channel ("*CH...").
    SerialNumber,       //!< Response containing the device's serial number ("*SN=...").
    RssiCurrentChannel, //!< Response containing the current RSSI value ("*RA=...").
    RssiAllChannels,    //!< Response containing RSSI values for all channels ("*RC=...").
    RouteInfo,          //!< Response containing route information ("*RT=...").
    GroupID,            //!< Response related to Group ID ("*GI...").
    EquipmentID,        //!< Response related to Equipment ID ("*EI...").
    DestinationID,      //!< Response related to Destination ID ("*DI...").
    GenericResponse,    //!< Generic response received from SendRawCommand.
};

/**
 * @struct MU_Modem_Event
 * @brief Structure containing information about an asynchronous event or response.
 */
struct MU_Modem_Event
{
    ModemError error;           //!< Status of the operation.
    MU_Modem_Response type;     //!< Type of response or event.
    int32_t value;              //!< Numerical value (RSSI, Serial Number, etc.)
    const uint8_t *pPayload;    //!< Pointer to payload (for DataReceived).
    uint16_t payloadLen;        //!< Length of payload.
    const uint8_t *pRouteNodes; //!< Pointer to route info (for DataReceived).
    uint8_t numRouteNodes;      //!< Number of route nodes.

    // --- Constructors ---
    // 1. Default: Initialize everything to zero/null for safety
    MU_Modem_Event() : error(ModemError::Ok), type(MU_Modem_Response::Idle), value(0), pPayload(nullptr), payloadLen(0), pRouteNodes(nullptr), numRouteNodes(0) {}

    // 2. Helper for simple status events
    MU_Modem_Event(ModemError err, MU_Modem_Response t)
        : error(err), type(t), value(0), pPayload(nullptr), payloadLen(0), pRouteNodes(nullptr), numRouteNodes(0) {}

    // 3. Helper for events with a value (RSSI, Channel, etc.)
    MU_Modem_Event(ModemError err, MU_Modem_Response t, int32_t val)
        : error(err), type(t), value(val), pPayload(nullptr), payloadLen(0), pRouteNodes(nullptr), numRouteNodes(0) {}
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
 * @enum MU_Modem_ParserState
 * @brief Internal parser state for MU specific responses.
 */
enum class MU_Modem_ParserState
{
    Start,
    ReadCmdPrefix,    //!< Reading "*DR=" or "*IR=" etc.
    RadioDrSize,      //!< Parsing size of *DR
    RadioDsRssi,      //!< Parsing RSSI of *DS
    RadioDrPayload,   //!< Reading payload
    ReadOptionUntilLF //!< Reading route info etc.
};

/**
 * @brief Callback function type for asynchronous operations and received data events.
 * @param event Structure containing event details.
 */
typedef void (*MU_Modem_AsyncCallback)(const MU_Modem_Event &event);

/**
 * @class MU_Modem
 * @brief Provides an interface to control the MU FSK modem.
 */
class MU_Modem : public SerialModemBase
{
public:
    /**
     * @brief Initializes the modem driver.
     * @param pUart A reference to the Stream object (e.g., Serial1).
     * @param frequencyModel The frequency model of the modem.
     * @param pCallback A pointer to the callback function.
     * @return MU_Modem_Error::Ok on success.
     */
    MU_Modem_Error begin(Stream &pUart, MU_Modem_FrequencyModel frequencyModel, MU_Modem_AsyncCallback pCallback = nullptr);

    /**
     * @brief Main processing loop (Delegates to SerialModemBase::update).
     */
    void Work();

    /**
     * @brief Sets an external buffer to store received packets for HasPacket()/GetPacket().
     * Not required if using Async Callback only.
     * @param buf Pointer to the user-allocated buffer.
     * @param size Size of the buffer (should be at least MU_MAX_PAYLOAD_LEN).
     */
    void setPacketBuffer(uint8_t *buf, uint8_t size)
    {
        m_pLegacyBuffer = buf;
        m_legacyBufferSize = size;
    }

    // --- Data Transmission ---
    /**
     * @brief Transmits a data packet (Synchronous/Blocking).
     * Queues the command and waits for completion.
     * Checks for LBT error (*IR=01) for a short period after command acceptance.
     * @param pMsg Pointer to the data buffer to transmit.
     * @param len Length of the data in bytes.
     * @param useRouteRegister If true, appends the /R option to use the route register.
     * @return MU_Modem_Error::Ok on success, MU_Modem_Error::FailLbt if busy.
     */
    MU_Modem_Error TransmitData(const uint8_t *pMsg, uint8_t len, bool useRouteRegister = false);

    /**
     * @brief Transmits a data packet (Asynchronous/Non-blocking).
     * Queues the command and returns immediately.
     * Completion result (TxComplete/TxFailed) is notified via callback.
     * @param pMsg Pointer to the data buffer to transmit.
     * @param len Length of the data in bytes.
     * @param useRouteRegister If true, appends the /R option to use the route register.
     * @return MU_Modem_Error::Ok if command accepted.
     */
    MU_Modem_Error TransmitDataAsync(const uint8_t *pMsg, uint8_t len, bool useRouteRegister = false);

    // --- Configuration (Synchronous Wrappers) ---
    // These methods block until the modem responds.

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
     * @param pChannel Pointer to store the retrieved channel number.
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
     * @brief Gets the current transmission power setting.
     * @param pPower Pointer to store the retrieved power setting (0x01 or 0x10).
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
     * @brief Gets the current Destination ID.
     * @param pDI Pointer to store the retrieved Destination ID.
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
     * @brief Gets the current Equipment ID (Own ID).
     * @param pEI Pointer to store the retrieved Equipment ID.
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
     * @brief Gets the current Group ID.
     * @param pGI Pointer to store the retrieved Group ID.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error GetGroupID(uint8_t *pGI);

    /**
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

    // --- Info & Status ---
    /**
     * @brief Gets the serial number of the modem.
     * @param pSerialNumber Pointer to store the retrieved serial number.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error GetSerialNumber(uint32_t *pSerialNumber);

    /**
     * @brief Gets the User ID of the modem.
     * @param pUI Pointer to store the retrieved User ID.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error GetUserID(uint16_t *pUI);

    /**
     * @brief Checks the carrier sense status (LBT).
     * Sends @CS command and waits for response.
     * @return MU_Modem_Error::Ok if channel is clear, MU_Modem_Error::FailLbt if busy.
     */
    MU_Modem_Error CheckCarrierSense();

    /**
     * @brief Gets the RSSI (Received Signal Strength Indicator) of the current channel.
     * @param pRssi Pointer to store the RSSI value in dBm (negative value).
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error GetRssiCurrentChannel(int16_t *pRssi);

    /**
     * @brief Gets RSSI values for all channels (Synchronous).
     * This operation takes several seconds to complete.
     * @param pRssiBuffer Buffer to store RSSI values.
     * @param bufferSize Size of the buffer (number of elements).
     * @param pNumRssiValues Pointer to store the number of values retrieved.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error GetAllChannelsRssi(int16_t *pRssiBuffer, size_t bufferSize, uint8_t *pNumRssiValues);

    /**
     * @brief Starts getting RSSI values for all channels (Asynchronous).
     * The result will be delivered via the callback with type MU_Modem_Response::RssiAllChannels.
     * @return MU_Modem_Error::Ok if the command was successfully queued.
     */
    MU_Modem_Error GetAllChannelsRssiAsync();

    /**
     * @brief Enables appending RSSI value to received data (*DR).
     * Sends @SI command. Used internally during initialization.
     * @return MU_Modem_Error::Ok on success.
     */
    MU_Modem_Error SetAddRssiValue();

    /**
     * @brief Performs a software reset of the modem.
     * Sends @SR command.
     * @return MU_Modem_Error::Ok on success.
     */
    MU_Modem_Error SoftReset();

    // --- Raw Command ---
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

    // --- Data Reception ---
    // HasPacket/GetPacket/DeletePacket pattern is kept for compatibility,
    // but Callback is preferred in this architecture.
    /**
     * @brief Checks if a data packet has been received and is buffered (Legacy mode).
     * @return True if a packet is available, false otherwise.
     */
    bool HasPacket() { return m_drMessagePresent; }

    /**
     * @brief Retrieves the buffered data packet (Legacy mode).
     * @param ppData Output pointer to the data buffer.
     * @param len Output pointer to the length of data.
     * @return MU_Modem_Error::Ok if data is available, MU_Modem_Error::Fail otherwise.
     */
    MU_Modem_Error GetPacket(const uint8_t **ppData, uint8_t *len);

    /**
     * @brief Clears the buffered data packet flag (Legacy mode).
     */
    void DeletePacket() { m_drMessagePresent = false; }

    /**
     * @brief Registers or updates the asynchronous callback function.
     * @param pCallback The callback function to register.
     */
    void SetAsyncCallback(MU_Modem_AsyncCallback pCallback) { m_pCallback = pCallback; }

protected:
    // SerialModemBase overrides
    virtual ModemParseResult parse() override;
    virtual void onRxDataReceived() override;
    virtual void onCommandComplete(ModemError result) override;
    virtual const char *getLogPrefix() const override { return "[MU Modem] "; }

private:
    void m_ResetParser();

    // Parser sub-handlers
    ModemParseResult m_HandleReadCmdPrefix(uint8_t c);
    ModemParseResult m_HandleRadioDrSize(uint8_t c);
    ModemParseResult m_HandleRadioDsRssi(uint8_t c);
    ModemParseResult m_HandleRadioDrPayload(uint8_t c);
    ModemParseResult m_HandleReadOptionUntilLF(uint8_t c);

    MU_Modem_AsyncCallback m_pCallback;
    MU_Modem_FrequencyModel m_frequencyModel;

    // Parser State
    MU_Modem_ParserState m_parserState;

    // Data Packet Buffer
    // Kept separate from SerialModemBase::_rxBuffer to allow interleaving
    int16_t m_lastRxRSSI;
    uint8_t m_drRouteInfo[MU_MAX_ROUTE_NODES_IN_DR];
    uint8_t m_drNumRouteNodes;

    bool m_drMessagePresent = false;
    uint8_t m_drMessageLen = 0;

    // Pointer to external buffer for legacy polling (GetPacket)
    uint8_t *m_pLegacyBuffer = nullptr;
    uint8_t m_legacyBufferSize = 0;

    // Async Request State
    MU_Modem_Response m_asyncExpectedResponse;

    // Internal LBT Error Flag (set by parse when *IR=01 is seen)
    volatile bool m_lbtErrorDetected;

    // Flag to suppress async callbacks during synchronous operations
    bool m_blockAsyncCallback;
};
