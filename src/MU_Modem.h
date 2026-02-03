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
// Interface driver for MU-3 (FSK modem manufactured by Circuit Design)

#pragma once
#include <Arduino.h>

/**
 * @brief Default baud rate for the MU modem.
 */
static constexpr uint32_t MU_DEFAULT_BAUDRATE = 19200;

/**
 * @brief Channel range constants for MU modem models.
 * Declared here, defined in MU_Modem.cpp
 */
/// 429MHz Model (Japan)
static constexpr uint8_t MU_CHANNEL_MIN_429 = 0x07; ///!< Minimum channel number for 429MHz model
static constexpr uint8_t MU_CHANNEL_MAX_429 = 0x2E; ///!< Maximum channel number for 429MHz model
/// 1.2GHz Model (Japan) - Assuming MU-4 1.2GHz or similar
static constexpr uint8_t MU_CHANNEL_MIN_1216 = 0x02; ///!< Minimum channel number for 1.2GHz model (Tentative)
static constexpr uint8_t MU_CHANNEL_MAX_1216 = 0x14; ///!< Maximum channel number for 1.2GHz model (Tentative)

/**
 * @brief Maximum payload and route node constants.
 */
static constexpr uint8_t MU_MAX_PAYLOAD_LEN = 255;
static constexpr uint8_t MU_MAX_ROUTE_NODES_IN_DR = 12; //!< Max route nodes in a *DR response (src + 10 relays + dest)

// --- Debug Configuration ---
// To enable debug prints for this library, add the following to your platformio.ini:
// build_flags = -D ENABLE_MU_MODEM_DEBUG
#ifdef ENABLE_MU_MODEM_DEBUG
#define MU_DEBUG_PRINT(...) \
    if (m_pDebugStream)     \
    m_pDebugStream->print(__VA_ARGS__)
#define MU_DEBUG_PRINTLN(...) \
    if (m_pDebugStream)       \
    m_pDebugStream->println(__VA_ARGS__)
#define MU_DEBUG_PRINTF(...) \
    if (m_pDebugStream)      \
    m_pDebugStream->printf(__VA_ARGS__)
#define MU_DEBUG_WRITE(...) \
    if (m_pDebugStream)     \
    m_pDebugStream->write(__VA_ARGS__)
#else
#define MU_DEBUG_PRINT(...)
#define MU_DEBUG_PRINTLN(...)
#define MU_DEBUG_PRINTF(...)
#define MU_DEBUG_WRITE(...)
#endif

/**
 * @enum MU_Modem_Response
 * @brief Defines the types of responses from the modem or internal states.
 */
enum class MU_Modem_Response
{
    Idle,       //!< No message received or expected.
    ParseError, //!< Garbage characters received.
    Timeout,    //!< No response received within the timeout period.

    ShowMode,           //!< Response indicating the modem's mode (e.g., "FSK CMD MODE").
    SaveValue,          //!< Response confirming a value has been written to non-volatile memory ("*WR=PS").
    Channel,            //!< Response related to the frequency channel ("*CH...").
    SerialNumber,       //!< Response containing the device's serial number ("*SN=...").
    MU_Modem_DtAck,     //!< Acknowledgment for the @DT (data transmission) command.
    DataReceived,       //!< Indicates that a data packet has been received ("*DR=...").
    RssiCurrentChannel, //!< Response containing the current RSSI value of the channel ("*RA=...").
    RssiAllChannels,    //!< Response containing RSSI values for all channels ("*RC=...").
    GenericResponse,    //!< Generic response received from SendRawCommand.
};

/**
 * @enum MU_Modem_Error
 * @brief Defines API level error codes.
 */
enum class MU_Modem_Error
{
    Ok,            //!< No error.
    Busy,          //!< The modem is busy processing a previous command.
    InvalidArg,    //!< An invalid argument was provided to a command.
    FailLbt,       //!< Transmission failed due to Listen Before Talk (LBT) detecting a busy channel.
    Fail,          //!< A general failure occurred.
    BufferTooSmall //!< Provided response buffer is too small.
};

/**
 * @enum MU_Modem_Mode
 * @brief Defines the operating modes of the modem.
 */
enum class MU_Modem_Mode : uint8_t
{
    FskBin = 0, //!< FSK Binary mode.
    FskCmd = 1, //!< FSK Command mode.
};

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
 * @enum MU_Modem_CmdState (Internal)
 * @brief Represents the high-level state of the command parser.
 */
enum class MU_Modem_CmdState
{
    Parsing = 0,         //!< Still parsing, waiting for more data.
    Garbage,             //!< Garbage data received.
    Overflow,            //!< Receive buffer overflowed.
    FinishedCmdResponse, //!< A complete command response has been received.
    FinishedDrResponse,  //!< A complete data reception (*DR) message has been received.
};

/**
 * @enum MU_Modem_ParserState (Internal)
 * @brief Represents the low-level state of the command parser state machine.
 */
enum class MU_Modem_ParserState
{
    Start = 0,

    ReadCmdFirstLetter,  //!< First char was '*', now reading the first letter of the command.
    ReadCmdSecondLetter, //!< Reading the second letter of the command.
    ReadCmdParam,        //!< Reading command parameters, typically after '*XX'.

    ReadRawString,

    RadioDrSize,    //!< Parsing the size of a *DR (data received) message.
    RadioDrPayload, //!< Reading the payload of a *DR message.

    ReadCmdUntilCR,    //!< Reading until a carriage return ('\r') is found.
    ReadCmdUntilLF,    //!< Reading until a line feed ('\n') is found.
    ReadOptionUntilCR, //!< Reading optional data until a carriage return ('\r') is found.
    ReadOptionUntilLF,

    ReadDsRSSI, //!< Reading RSSI value from a *DS message.
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
 *
 * This class encapsulates the logic for communicating with the MU modem
 * via a Stream interface (e.g., HardwareSerial). It supports sending commands,
 * handling synchronous and asynchronous responses, and processing received data packets.
 */
class MU_Modem
{

public: // methods
    /**
     * @brief Initializes the modem driver.
     * @param frequencyModel The frequency model of the modem (e.g., MHz_429 or MHz_1216).
     * @param pUart A reference to the Stream object (e.g., Serial1) used for communication.
     * @param pCallback A pointer to the callback function for asynchronous events. Defaults to nullptr.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error begin(Stream &pUart, MU_Modem_FrequencyModel frequencyModel, MU_Modem_AsyncCallback pCallback = nullptr);

    /**
     * @brief Sets or updates the asynchronous callback function.
     * @param pCallback A pointer to the callback function. Set to nullptr to disable callbacks.
     */
    void SetAsyncCallback(MU_Modem_AsyncCallback pCallback) { m_pCallback = pCallback; }

    /**
     * @brief Sets the stream for debug output.
     * @param debugStream Pointer to the Stream object (e.g., &Serial). Set to nullptr to disable debug output.
     */
    void setDebugStream(Stream *debugStream);

    /**
     * @brief Transmits a data packet.
     * @details This function sends a data packet and waits for the LBT (Listen Before Talk) result.
     * If the channel is busy, it returns `FailLbt`. If the channel is clear, it proceeds with the transmission.
     * This is a blocking function. For continuous high-throughput transmission, consider using `CheckCarrierSense` followed by `TransmitDataFireAndForget`.
     * @param useRouteRegister If true, appends the /R option to use the route information stored in the modem's route register.
     * @param pMsg Pointer to the data buffer to transmit.
     * @param len Length of the data in bytes.
     *
     * @return MU_Modem_Error::Ok on success, MU_Modem_Error::FailLbt if the channel is busy, or other error codes.
     */
    MU_Modem_Error TransmitData(const uint8_t *pMsg, uint8_t len, bool useRouteRegister = false);

    /**
     * @brief Transmits a data packet without waiting for LBT or ACK confirmation ("Fire and Forget").
     * This function sends the data and waits only for the command acknowledgment (*DT=...).
     * It returns immediately after, allowing for high-throughput continuous transmission
     * by utilizing the modem's double buffer, as described in the datasheet.
     * @param pMsg Pointer to the data buffer to transmit.
     * @param len Length of the data in bytes.
     * @param useRouteRegister If true, appends the /R option to use the route register.
     * @return MU_Modem_Error::Ok if the command was accepted by the modem, or an error code on failure.
     */
    MU_Modem_Error TransmitDataFireAndForget(const uint8_t *pMsg, uint8_t len, bool useRouteRegister = false);

    /**
     * @brief Checks the current channel status using carrier sense (@CS command).
     * This function queries the modem to see if the channel is clear for transmission.
     * The modem's threshold for this check is -105dBm.
     * @return
     * - MU_Modem_Error::Ok if the channel is clear (*CS=EN).
     * - MU_Modem_Error::FailLbt if the channel is busy (*CS=DI).
     * - MU_Modem_Error::Fail on timeout or communication error.
     */
    MU_Modem_Error CheckCarrierSense();

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
     * @brief Gets the current frequency channel.
     * @param pChannel Pointer to a variable to store the channel number.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error GetChannel(uint8_t *pChannel);
    /**
     * @brief Sets the frequency channel.
     * @param channel The channel number to set. Valid range depends on the frequency model.
     * @param saveValue If true, saves the setting to non-volatile memory.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error SetChannel(uint8_t channel, bool saveValue);

    /**
     * @brief Gets the Group ID.
     * @param pGI Pointer to a variable to store the Group ID.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error GetGroupID(uint8_t *pGI);
    /**
     * @brief Sets the Group ID.
     * @param gi The Group ID to set (0x00 - 0xFF).
     * @param saveValue If true, saves the setting to non-volatile memory.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error SetGroupID(uint8_t gi, bool saveValue);

    /**
     * @brief Gets the Destination ID.
     * @param pDI Pointer to a variable to store the Destination ID.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error GetDestinationID(uint8_t *pDI);
    /**
     * @brief Sets the Destination ID.
     * @param di The Destination ID to set (0x00 - 0xFF).
     * @param saveValue If true, saves the setting to non-volatile memory.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error SetDestinationID(uint8_t di, bool saveValue);

    /**
     * @brief Gets the Equipment ID.
     * @param pEI Pointer to a variable to store the Equipment ID.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error GetEquipmentID(uint8_t *pEI);
    /**
     * @brief Sets the Equipment ID.
     * @param ei The Equipment ID to set (0x00 - 0xFF).
     * @param saveValue If true, saves the setting to non-volatile memory.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error SetEquipmentID(uint8_t ei, bool saveValue);

    /**
     * @brief Gets the read-only User ID.
     * @param pUI Pointer to a variable to store the User ID.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error GetUserID(uint16_t *pUI);

    /**
     * @brief Gets the current RSSI value of the configured channel (carrier sense).
     * @param pRssi Pointer to a variable to store the RSSI value in dBm.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error GetRssiCurrentChannel(int16_t *pRssi);

    /**
     * @brief Gets the RSSI values for all available channels at once.
     * This function sends the @RC command to the modem.
     * The number of channels depends on the modem's frequency model.
     * (429MHz: 40 channels, 1216MHz: 19 channels)
     * @param pRssiBuffer Pointer to an array to store the RSSI values in dBm.
     * @param bufferSize The size of the pRssiBuffer in number of elements (int16_t).
     *                   It must be large enough to hold all channel data.
     * @param pNumRssiValues Pointer to a variable to store the number of RSSI values actually written to the buffer.
     * @return MU_Modem_Error::Ok on success, MU_Modem_Error::BufferTooSmall if the buffer is not large enough, or other error codes.
     */
    MU_Modem_Error GetAllChannelsRssi(int16_t *pRssiBuffer, size_t bufferSize, uint8_t *pNumRssiValues);

    /**
     * @brief Asynchronously requests the current RSSI value of the channel.
     * The result will be delivered via the callback function.
     * @return MU_Modem_Error::Ok if the request was sent successfully, MU_Modem_Error::Busy if busy.
     */
    MU_Modem_Error GetRssiCurrentChannelAsync();

    /**
     * @brief Asynchronously requests the RSSI values for all available channels.
     * The result (raw response string) will be delivered via the callback function's payload.
     * @return MU_Modem_Error::Ok if the request was sent successfully, MU_Modem_Error::Busy if busy.
     */
    MU_Modem_Error GetAllChannelsRssiAsync();

    /**
     * @brief Sets the relay route information in the route register.
     * @param pRouteInfo Pointer to an array containing the route information (relay station IDs and destination ID).
     * @param numNodes The number of IDs in the route information (1 <= numNodes <= 11).
     * @param saveValue If true, saves the setting to non-volatile memory.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error SetRouteInfo(const uint8_t *pRouteInfo, uint8_t numNodes, bool saveValue);

    /**
     * @brief Gets the relay route information from the route register.
     * @param pRouteInfoBuffer Pointer to a buffer to store the route information.
     * @param bufferSize The size of the buffer in bytes.
     * @param pNumNodes Pointer to a variable to store the number of retrieved IDs. Will be set to 0 if the route is not available ("NA").
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error GetRouteInfo(uint8_t *pRouteInfoBuffer, size_t bufferSize, uint8_t *pNumNodes);

    /**
     * @brief Clears the route information in the route register (sets it to "NA").
     * @param saveValue If true, saves the setting to non-volatile memory.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error ClearRouteInfo(bool saveValue);

    /**
     * @brief Transmits a data packet with explicitly specified route information.
     * The route information is appended directly to the command string.
     * @param pRouteInfo Pointer to an array containing the route information (relay station IDs and destination ID).
     * @param numNodes The number of IDs in the route information (1 <= numNodes <= 11).
     * @param pMsg Pointer to the data buffer to transmit.
     * @param len Length of the data in bytes.
     * @param requestAck If true, requests an ACK response from the destination station (/A option).
     * @param outputToRelays If true, outputs the data to relay stations as well (/B or /S option).
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error TransmitDataWithRoute(const uint8_t *pRouteInfo, uint8_t numNodes, const uint8_t *pMsg, uint8_t len, bool requestAck = false, bool outputToRelays = false);

    /**
     * @brief Gets the transmission power setting.
     * @param pPower Pointer to a variable to store the power setting (0x01 for 1mW, 0x10 for 10mW).
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error GetPower(uint8_t *pPower);
    /**
     * @brief Sets the transmission power.
     * @param power The power setting to set (0x01 for 1mW, 0x10 for 10mW).
     * @param saveValue If true, saves the setting to non-volatile memory.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error SetPower(uint8_t power, bool saveValue);

    /**
     * @brief Gets the serial number of the modem.
     * @param pSn Pointer to a variable to store the serial number.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error GetSerialNumber(uint32_t *pSerialNumber);

    /**
     * @brief Asynchronously requests the serial number of the modem.
     * The result will be delivered via the callback function.
     * @return MU_Modem_Error::Ok if the request was sent successfully, MU_Modem_Error::Busy if busy.
     */
    MU_Modem_Error GetSerialNumberAsync();

    /**
     * @brief Enables the modem to include the RSSI value in received data messages (*DS).
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error SetAddRssiValue();

    /**
     * @brief Performs a software reset of the modem.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error SoftReset();

    /**
     * @brief Gets the setting for the automatic reply route feature (@RR).
     * When enabled, the modem automatically updates its route register with a reply route
     * upon receiving a data packet with route information.
     * @param pEnabled Pointer to a boolean to store the result (true if ON, false if OFF).
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error GetAutoReplyRoute(bool *pEnabled);

    /**
     * @brief Sets the automatic reply route feature (@RR ON or @RR OF).
     * @param enabled If true, enables the feature (@RR ON). When a data packet with route information is received,
     * the modem automatically generates a return route and writes it to the route register. If false, disables the feature (@RR OF).
     * @param saveValue If true, saves the setting to non-volatile memory.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error SetAutoReplyRoute(bool enabled, bool saveValue);

    /**
     * @brief Gets the status of the route information addition mode (@RI).
     * When ON, route information is included in received data messages (*DR).
     * @param pEnabled Pointer to a boolean to store the result (true if ON, false if OFF).
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error GetRouteInfoAddMode(bool *pEnabled);

    /**
     * @brief Sets whether to include route information in received data messages (@RI ON or @RI OF).
     * @param enabled True to enable (@RI ON), false to disable (@RI OF).
     * @param saveValue If true, saves the setting to non-volatile memory.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error SetRouteInfoAddMode(bool enabled, bool saveValue);

    /**
     * @brief Sends a raw command string to the modem and waits for a response.
     * This function is intended for sending commands not directly supported by the library.
     * The user is responsible for formatting the command correctly (e.g., "@CMD VAL\r\n")
     * and parsing the raw response received in the buffer.
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
     * @brief Checks if a complete radio packet has been received and is ready for processing.
     * @return True if a packet is available, false otherwise.
     */
    bool HasPacket() { return m_drMessagePresent; }

    /**
     * @brief Retrieves the last received packet.
     * @param ppData Pointer to a pointer that will be set to the packet's data buffer.
     * @param len Pointer to a variable to store the packet's length.
     * @return MU_Modem_Error::Ok if a packet was retrieved, MU_Modem_Error::Fail otherwise.
     */
    MU_Modem_Error GetPacket(const uint8_t **ppData, uint8_t *len);

    /**
     * @brief Marks the current packet as processed, allowing the buffer to be used for the next one.
     */
    void DeletePacket() { m_drMessagePresent = false; }

    /**
     * @brief Main work/polling function to be called repeatedly in the main loop.
     * This function processes incoming serial data, parses responses, and triggers callbacks.
     */
    void Work();

private: // methods
    /**
     * @brief Checks if the timeout for a synchronous command has expired.
     * @return True if timeout has occurred, false otherwise.
     */
    bool m_IsTimeout();

    /**
     * @brief Starts or restarts the timeout counter.
     * @param ms The timeout duration in milliseconds.
     */
    void m_StartTimeout(uint32_t ms = 500);

    void m_ClearTimeout();

    void m_WriteString(const char *pString);
    void m_WriteString(const __FlashStringHelper *pString);

    // methods for reading from UART, using a one-byte buffer
    uint8_t m_ReadByte();
    void m_UnreadByte(uint8_t unreadByte);
    void m_ClearUnreadByte();
    uint32_t m_Read(uint8_t *pDst, uint32_t count);

    void m_ResetParser();

    void m_FlushGarbage();

    /**
     * @brief Parses incoming data from the serial stream.
     * This is the core state machine for interpreting modem responses.
     * @return A MU_Modem_CmdState indicating the result of the parsing attempt.
     */
    MU_Modem_CmdState m_Parse();

    /**
     * @brief Internal helper function to parse route information from a *RT response.
     * @param pDestBuffer Buffer to store the parsed route nodes.
     * @param bufferSize Size of the destination buffer.
     * @param pNumNodes Pointer to store the number of parsed nodes.
     * @return MU_Modem_Error::Ok on success, or an error code on failure.
     */
    MU_Modem_Error m_HandleMessage_RT(uint8_t *pDestBuffer, size_t bufferSize, uint8_t *pNumNodes);

    /**
     * @brief Waits for a command response synchronously.
     * @param ms The maximum time to wait in milliseconds.
     * @return MU_Modem_Error::Ok on success, MU_Modem_Error::Fail on timeout or error.
     */
    MU_Modem_Error m_WaitCmdResponse(uint32_t ms = 500);

    void m_SetExpectedResponses(MU_Modem_Response ep0, MU_Modem_Response ep1, MU_Modem_Response ep2);

    MU_Modem_Error m_DispatchCmdResponseAsync();

    MU_Modem_Error m_HandleMessage_WR();

    MU_Modem_Error m_HandleMessageHexByte(uint8_t *pValue, uint32_t responseLen, const char *responsePrefix);

    MU_Modem_Error m_HandleMessage_RA(int16_t *pRssi);

    MU_Modem_Error m_HandleMessage_SN(uint32_t *pSerialNumber);

    void m_ClearOneLine();

private:                                      // data
    Stream *m_pUart;                          //!< Pointer to the serial stream object.
    Stream *m_pDebugStream;                   //!< Pointer to the stream for debug output.
    MU_Modem_AsyncCallback m_pCallback;       //!< Pointer to the user-defined callback function.
    MU_Modem_FrequencyModel m_frequencyModel; //!< The configured frequency model of the modem.
    MU_Modem_Mode m_mode;                     //!< Current operating mode of the modem.

    // --- Parser State ---
    MU_Modem_ParserState m_parserState; //!< Current state of the low-level parser.
    int16_t m_oneByteBuf;               //!< A 1-byte buffer for un-reading a character. -1 if empty.
    uint16_t m_rxIdx;                   //!< Current index in the m_rxMessage or m_drMessage buffer.
    uint8_t m_rxMessage[128];           //!< Buffer for standard command responses.

    // --- Received Data Packet (*DR) State ---
    bool m_drMessagePresent;                         //!< Flag indicating if a complete *DR message is in the buffer.
    uint8_t m_drMessageLen;                          //!< Length of the payload in the *DR message.
    uint8_t m_drMessage[300];                        //!< Buffer for received data packets (*DR messages).
    int16_t m_lastRxRSSI;                            //!< RSSI value of the last received packet.
    uint8_t m_drRouteInfo[MU_MAX_ROUTE_NODES_IN_DR]; //!< Buffer for route info from a *DR message.
    uint8_t m_drNumRouteNodes;                       //!< Number of route nodes from a *DR message.

    // --- Asynchronous Command State ---
    MU_Modem_Response m_asyncExpectedResponse;     //!< The expected response type for an asynchronous command.
    MU_Modem_Response m_asyncExpectedResponses[3]; //!< Array for holding multiple expected response types. (Currently unused)
    bool bTimeout = true;                          //!< Flag indicating if the timeout timer is active (false) or expired/inactive (true).
    uint32_t startTime;                            //!< Start time for timeout measurement (from millis()).
    uint32_t timeOut;                              //!< Duration of the timeout in milliseconds.
};
