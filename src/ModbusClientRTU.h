// =================================================================================================
// eModbus: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to eModbus
//               MIT license - see license.md for details
// =================================================================================================
#ifndef _MODBUS_CLIENT_RTU_H
#define _MODBUS_CLIENT_RTU_H

#include "options.h"

#if HAS_FREERTOS

#include "ModbusClient.h"
#include "Stream.h"
#include "RTUutils.h"
#include <queue>
#include <vector>

using std::queue;

#define DEFAULTTIMEOUT 2000

class ModbusClientRTU : public ModbusClient {
public:
  // Constructor takes an optional DE/RE pin and queue limit
  explicit ModbusClientRTU(int8_t rtsPin = -1, uint16_t queueLimit = 100);

  // Alternative Constructor takes an RTS line toggle callback
  explicit ModbusClientRTU(RTScallback rts, uint16_t queueLimit = 100);

  // Destructor: clean up queue, task etc.
  ~ModbusClientRTU();

  // begin: start worker task
  void begin(Stream& serial, uint32_t baudrate, int coreID = -1);
  // Special variant for HardwareSerial
  void begin(HardwareSerial& serial, int coreID = -1);

  // end: stop the worker
  void end();

  // Set default timeout value for interface
  void setTimeout(uint32_t TOV);

  // Toggle protocol to ModbusASCII
  void useModbusASCII(unsigned long timeout = 1000);

  // Toggle protocol to ModbusRTU
  void useModbusRTU();

  // Inquire protocol mode
  bool isModbusASCII();

  // Toggle skipping of leading 0x00 byte
  void skipLeading0x00(bool onOff = true);

  // Return number of unprocessed requests in queue
  uint32_t pendingRequests();

  // Remove all pending request from queue
  inline void clearQueue()
  {
    clearRequests = true;
  }
  
  // addBroadcastMessage: create a fire-and-forget message to all servers on the RTU bus
  Error addBroadcastMessage(const uint8_t *data, uint8_t len);

protected:
  struct RequestEntry {
    uint32_t token;
    ModbusMessage msg;
    MBOnResponse responseHandler;
    bool isSyncRequest;
    RequestEntry(uint32_t t, ModbusMessage m, MBOnResponse r, bool syncReq = false) :
      token(t),
      msg(m),
      responseHandler(r),
      isSyncRequest(syncReq) {}
  };

  // Base addRequest and syncRequest must be present
  Error addRequestM(ModbusMessage msg, uint32_t token, MBOnResponse handler = nullptr);
  ModbusMessage syncRequestM(ModbusMessage msg, uint32_t token);

  // addToQueue: send freshly created request to queue
  bool addToQueue(uint32_t token, ModbusMessage msg, MBOnResponse handler = nullptr, bool syncReq = false);

  // handleConnection: worker task method
  static void handleConnection(ModbusClientRTU *instance);

  // receive: get response via Serial
  ModbusMessage receive(const ModbusMessage request);

  // start background task
  void doBegin(uint32_t baudRate, int coreID);

  void isInstance() { return; }   // make class instantiable
  bool clearRequests;             // Bool to indicate requests must be cleared
  void _clearRequests();          // Helper function to clear requests from queue, calling response handler
  queue<RequestEntry> requests;   // Queue to hold requests to be processed
  #if USE_MUTEX
  mutex qLock;                    // Mutex to protect queue
  #endif
  Stream *MR_serial;              // Ptr to the serial interface used
  unsigned long MR_lastMicros;    // Microseconds since last bus activity
  uint32_t MR_interval;           // Modbus RTU bus quiet time
  int8_t MR_rtsPin;               // GPIO pin to toggle RS485 DE/RE line. -1 if none.
  RTScallback MTRSrts;            // RTS line callback function
  uint16_t MR_qLimit;             // Maximum number of requests to hold in the queue
  uint32_t MR_timeoutValue;       // Interface default timeout
  bool MR_useASCII;               // true=ModbusASCII, false=ModbusRTU
  bool MR_skipLeadingZeroByte;    // true=skip the first byte if it is 0x00, false=accept all bytes

};

#endif  // HAS_FREERTOS

#endif  // INCLUDE GUARD
