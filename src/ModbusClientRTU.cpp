// =================================================================================================
// eModbus: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to eModbus
//               MIT license - see license.md for details
// =================================================================================================
#include "ModbusClientRTU.h"

#if HAS_FREERTOS

#include "Logging.h"

// Constructor takes an optional DE/RE pin and queue size
ModbusClientRTU::ModbusClientRTU(int8_t rtsPin, uint16_t queueLimit) :
  ModbusClient(),
  MR_serial(nullptr),
  MR_lastMicros(micros()),
  MR_interval(2000),
  MR_rtsPin(rtsPin),
  MR_qLimit(queueLimit),
  MR_timeoutValue(DEFAULTTIMEOUT),
  MR_useASCII(false),
  MR_skipLeadingZeroByte(false) {
    if (MR_rtsPin >= 0) {
      pinMode(MR_rtsPin, OUTPUT);
      MTRSrts = [this](bool level) {
        digitalWrite(MR_rtsPin, level);
      };
      MTRSrts(LOW);
    } else {
      MTRSrts = RTUutils::RTSauto;
    }
}

// Alternative constructor takes an RTS callback function
ModbusClientRTU::ModbusClientRTU(RTScallback rts, uint16_t queueLimit) :
  ModbusClient(),
  MR_serial(nullptr),
  MR_lastMicros(micros()),
  MR_interval(2000),
  MTRSrts(rts),
  MR_qLimit(queueLimit),
  MR_timeoutValue(DEFAULTTIMEOUT),
  MR_useASCII(false),
  MR_skipLeadingZeroByte(false) {
    MR_rtsPin = -1;
    MTRSrts(LOW);
}

// Destructor: clean up queue, task etc.
ModbusClientRTU::~ModbusClientRTU() {
  // Kill worker task and clean up request queue
  end();
}

// begin: start worker task - general version
void ModbusClientRTU::begin(Stream& serial, uint32_t baudRate, int coreID) {
  MR_serial = &serial;
  doBegin(baudRate, coreID);
}

// begin: start worker task - HardwareSerial version
void ModbusClientRTU::begin(HardwareSerial& serial, int coreID) {
  MR_serial = &serial;
  uint32_t baudRate = serial.baudRate();
  serial.setRxFIFOFull(1);
  doBegin(baudRate, coreID);
}

void ModbusClientRTU::doBegin(uint32_t baudRate, int coreID) {
  // Task already running? End it in case
  end();

  // Pull down RTS toggle, if necessary
  MTRSrts(LOW);

  // Set minimum interval time
  MR_interval = RTUutils::calculateInterval(baudRate);

  // Create unique task name
  char taskName[9];
  snprintf(taskName, 9, "MB%02XRTU", instanceCounter);
  // Start task to handle the queue
  xTaskCreatePinnedToCore((TaskFunction_t)&handleConnection, taskName, CLIENT_TASK_STACK, this, 6, &worker, coreID >= 0 ? coreID : NULL);

  mb_log_d("Client task %u started. Interval=%u", (uint32_t)worker, MR_interval);
}

// end: stop worker task
void ModbusClientRTU::end() {
  if (worker) {
    // Kill task
#if IS_LINUX
    pthread_cancel(worker);
#else
    xTaskNotify(worker, STOP_NOTIFICATION_VALUE, eSetValueWithOverwrite);
    while (eTaskGetState(worker) != eTaskState::eDeleted)
    {
      vTaskDelay(5 / portTICK_PERIOD_MS);
    }
#endif
    mb_log_d("Client task %u killed.", (uint32_t)worker);
    worker = nullptr;
  }
}

// setTimeOut: set/change the default interface timeout
void ModbusClientRTU::setTimeout(uint32_t TOV) {
  MR_timeoutValue = TOV;
  mb_log_d("Timeout set to %u", TOV);
}

// Toggle protocol to ModbusASCII
void ModbusClientRTU::useModbusASCII(unsigned long timeout) {
  MR_useASCII = true;
  MR_timeoutValue = timeout; // Switch timeout to ASCII's value
  mb_log_d("Protocol mode: ASCII");
}

// Toggle protocol to ModbusRTU
void ModbusClientRTU::useModbusRTU() {
  MR_useASCII = false;
  mb_log_d("Protocol mode: RTU");
}

// Inquire protocol mode
bool ModbusClientRTU::isModbusASCII() {
  return MR_useASCII;
}

// Toggle skipping of leading 0x00 byte
void ModbusClientRTU::skipLeading0x00(bool onOff) {
  MR_skipLeadingZeroByte = onOff;
  mb_log_d("Skip leading 0x00 mode = %s", onOff ? "ON" : "OFF");
}

// Return number of unprocessed requests in queue
uint32_t ModbusClientRTU::pendingRequests() {
  return requests.size();
}

// Base addRequest taking a preformatted data buffer and length as parameters
Error ModbusClientRTU::addRequestM(ModbusMessage msg, uint32_t token, MBOnResponse handler) {
  Error rc = SUCCESS;        // Return value

  mb_log_d("request for %02X/%02X", msg.getServerID(), msg.getFunctionCode());

  // Add it to the queue, if valid
  if (msg) {
    // Queue add successful?
    if (!addToQueue(token, msg, handler)) {
      // No. Return error after deleting the allocated request.
      rc = REQUEST_QUEUE_FULL;
    }
  }

  mb_log_d("RC=%02X", rc);
  return rc;
}

// Base syncRequest follows the same pattern
ModbusMessage ModbusClientRTU::syncRequestM(ModbusMessage msg, uint32_t token) {
  ModbusMessage response;

  if (msg) {
    // Queue add successful?
    if (!addToQueue(token, msg, nullptr, true)) {
      // No. Return error after deleting the allocated request.
      response.setError(msg.getServerID(), msg.getFunctionCode(), REQUEST_QUEUE_FULL);
    } else {
      // Request is queued - wait for the result.
      response = waitSync(msg.getServerID(), msg.getFunctionCode(), token);
    }
  } else {
    response.setError(msg.getServerID(), msg.getFunctionCode(), EMPTY_MESSAGE);
  }
  return response;
}

// addBroadcastMessage: create a fire-and-forget message to all servers on the RTU bus
Error ModbusClientRTU::addBroadcastMessage(const uint8_t *data, uint8_t len) {
  Error rc = SUCCESS;        // Return value

  mb_log_d("Broadcast request of length %u", len);

  // We do only accept requests with data, 0 byte, data and CRC must fit into 256 bytes.
  if (len && len < 254) {
    // Create a "broadcast token"
    uint32_t token = (millis() & 0xFFFFFF) | 0xBC000000;
    ModbusMessage msg;
    
    // Server ID is 0x00 for broadcast
    msg.add((uint8_t)0x00);
    // Append data
    msg.add(data, len);

    // Queue add successful?
    if (!addToQueue(token, msg)) {
      // No. Return error after deleting the allocated request.
      rc = REQUEST_QUEUE_FULL;
    }
  } else {
    rc =  BROADCAST_ERROR;
  }

  mb_log_d("RC=%02X", rc);
  return rc;
}


// addToQueue: send freshly created request to queue
bool ModbusClientRTU::addToQueue(uint32_t token, ModbusMessage request, MBOnResponse handler, bool syncReq) {
  bool rc = false;
  // Did we get one?
  if (request) {
    RequestEntry re(token, request, handler, syncReq);
    if (requests.size()<MR_qLimit) {
      // Yes. Safely lock queue and push request to queue
      rc = true;
      LOCK_GUARD(lockGuard, qLock);
      requests.push(re);
    }
    {
      LOCK_GUARD(cntLock, countAccessM);
      messageCount++;
    }
  }

  mb_log_d("RC=%02X", rc);
  return rc;
}

// handleConnection: worker task
// This was created in begin() to handle the queue entries
void ModbusClientRTU::handleConnection(ModbusClientRTU *instance) {
  // initially clean the serial buffer
  while (instance->MR_serial->available()) instance->MR_serial->read();
  delay(100);

  // Loop forever - or until task is killed
  while (1) {
    if (ulTaskNotifyTake(pdTRUE, 1) == STOP_NOTIFICATION_VALUE)
    {
      instance->_clearRequests(); // Ensure event handlers are called
      break;
    }
    // Clear requests if requested
    if (instance->clearRequests)
    {
      instance->_clearRequests();
      instance->clearRequests = false;
    }
    // Do we have a reuest in queue?
    if (!instance->requests.empty()) {
      // Yes. pull it.
      RequestEntry request = instance->requests.front();

      mb_log_d("Pulled request from queue");

      // Send it via Serial
      RTUutils::send(*(instance->MR_serial), instance->MR_lastMicros, instance->MR_interval, instance->MTRSrts, request.msg, instance->MR_useASCII);

      mb_log_d("Request sent.");
      mb_log_buf_v(request.msg.data(), request.msg.size());

      // For a broadcast, we will not wait for a response
      if (request.msg.getServerID() != 0 || ((request.token & 0xFF000000) != 0xBC000000)) {
        // This is a regular request, Get the response - if any
        ModbusMessage response = RTUutils::receive(
          'C',
          *(instance->MR_serial), 
          instance->MR_timeoutValue, 
          instance->MR_lastMicros, 
          instance->MR_interval, 
          instance->MR_useASCII,
          instance->MR_skipLeadingZeroByte);
  
        mb_log_d("%s response (%u bytes) received.", response.size()>1 ? "Data" : "Error", response.size());
        mb_log_buf_v(response.data(), response.size());
  
        // No error in receive()?
        if (response.size() > 1) {
          // No. Check message contents
          // Does the serverID match the requested?
          if (request.msg.getServerID() != response.getServerID()) {
            // No. Return error response
            response.setError(request.msg.getServerID(), request.msg.getFunctionCode(), SERVER_ID_MISMATCH);
          // ServerID ok, but does the FC match as well?
          } else if (request.msg.getFunctionCode() != (response.getFunctionCode() & 0x7F)) {
            // No. Return error response
            response.setError(request.msg.getServerID(), request.msg.getFunctionCode(), FC_MISMATCH);
          } 
        } else {
          // No, we got an error code from receive()
          // Return it as error response
          response.setError(request.msg.getServerID(), request.msg.getFunctionCode(), static_cast<Error>(response[0]));
        }
  
        mb_log_d("Response generated.");
        mb_log_buf_v(response.data(), response.size());

        // If we got an error, count it
        if (response.getError() != SUCCESS) {
          instance->errorCount++;
        }
  
        // Was it a synchronous request?
        if (request.isSyncRequest) {
          // Yes. Put it into the response map
          {
            LOCK_GUARD(sL, instance->syncRespM);
            instance->syncResponse[request.token] = response;
          }
        // No, an async request. Do we have an onResponse handler?
        } else if (request.responseHandler) {
          // Yes. Call it
          request.responseHandler(response, request.token);
        } else {
          mb_log_w("No response handler.");
        }
      }
      // Clean-up time. 
      {
        // Safely lock the queue
        LOCK_GUARD(lockGuard, instance->qLock);
        // Remove the front queue entry
        instance->requests.pop();
      }
    } else {
      delay(1);
    }
  }
  vTaskDelete(NULL);
}

void ModbusClientRTU::_clearRequests()
{
  LOCK_GUARD(lockGuard, qLock);
  while (!requests.empty())
  {
    ModbusMessage response;
    RequestEntry request = requests.front();
    response.setError(request.msg.getServerID(), request.msg.getFunctionCode(), QUEUE_CLEARED);
    request.responseHandler(response, request.token);
    messageCount--;
    requests.pop();
  }
}

#endif  // HAS_FREERTOS
