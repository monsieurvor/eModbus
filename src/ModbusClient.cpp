// =================================================================================================
// eModbus: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to eModbus
//               MIT license - see license.md for details
// =================================================================================================
#include "ModbusClient.h"
#include "Logging.h"

uint16_t ModbusClient::instanceCounter = 0;

// Default constructor: set the default timeout to 2000ms, zero out all other 
ModbusClient::ModbusClient() :
  messageCount(0),
  errorCount(0),
  #if HAS_FREERTOS
  worker(NULL),
  #elif IS_LINUX
  worker(0),
  #endif
  onData(nullptr),
  onError(nullptr),
  onResponse(nullptr) { instanceCounter++; }

// onDataHandler: register callback for data responses
bool ModbusClient::onDataHandler(MBOnData handler) {
  if (onData) {
    mb_log_w("onData handler was already claimed");
  } else if (onResponse) {
    mb_log_e("onData handler is unavailable with an onResponse handler");
    return false;
  }
  onData = handler;
  return true;
}

// onErrorHandler: register callback for error responses
bool ModbusClient::onErrorHandler(MBOnError handler) {
  if (onError) {
    mb_log_w("onError handler was already claimed");
  } else if (onResponse) {
    mb_log_e("onError handler is unavailable with an onResponse handler");
    return false;
  } 
  onError = handler;
  return true;
}

// onResponseHandler: register callback for error responses
bool ModbusClient::onResponseHandler(MBOnResponse handler) {
  if (onError || onData) {
    mb_log_e("onResponse handler is unavailable with an onData or onError handler");
    return false;
  } 
  onResponse = handler;
  return true;
}

// getMessageCount: return message counter value
uint32_t ModbusClient::getMessageCount() {
  return messageCount;
}

// getErrorCount: return error counter value
uint32_t ModbusClient::getErrorCount() {
  return errorCount;
}

// resetCounts: Set both message and error counts to zero
void ModbusClient::resetCounts() {
  {
    LOCK_GUARD(cntLock, countAccessM);
    messageCount = 0;
    errorCount = 0;
  }
}

// waitSync: wait for response on syncRequest to arrive
ModbusMessage ModbusClient::waitSync(uint8_t serverID, uint8_t functionCode, uint32_t token) {
  ModbusMessage response;
  unsigned long lostPatience = millis();
 
  // Default response is TIMEOUT
  response.setError(serverID, functionCode, TIMEOUT);

  // Loop 60 seconds, if unlucky
  while (millis() - lostPatience < 60000) {
    {
      LOCK_GUARD(lg, syncRespM);
      // Look for the token
      auto sR = syncResponse.find(token);
      // Is it there?
      if (sR != syncResponse.end()) {
        // Yes. get the response, delete it from the map and return
        response = sR->second;
        syncResponse.erase(sR);
        break;
      }
    }
    // Give the watchdog time to act
    delay(10);
  }
  return response;
}
