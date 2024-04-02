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
  worker(0)
  #endif
  { instanceCounter++; }

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

  // Loop 10 seconds
  while (millis() - lostPatience < 10000) {
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
