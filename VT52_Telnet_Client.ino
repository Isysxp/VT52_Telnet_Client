/**
 *  NimBLE_Async_client Demo:
 *
 *  Demonstrates asynchronous client operations.
 *
 *  Created: on November 4, 2024
 *      Author: H2zero
 */

#include <Arduino.h>
#include <NimBLEDevice.h>
#include "src/key_codes.h"
#include <esp_task_wdt.h>

static constexpr uint32_t scanTimeMs = 5 * 1000;
static bool online = false;
static char nulls[8];
static NimBLERemoteService* pSvc = nullptr;
static NimBLERemoteCharacteristic* pChr = nullptr;
static NimBLERemoteDescriptor* pDsc = nullptr;
static NimBLEClient* pClient = nullptr;
uint8_t cplock = 1;
static uint8_t lstkey = 2;
bool connected = false, run8 = false;

class ClientCallbacks : public NimBLEClientCallbacks {
  void onConnect(NimBLEClient* pClient) override {
    Serial.printf("Connected to: %s\n", pClient->getPeerAddress().toString().c_str());
    connected = true;
  }

  void onDisconnect(NimBLEClient* pClient, int reason) override {
    Serial.printf("%s Disconnected, reason = %d - Starting scan\n", pClient->getPeerAddress().toString().c_str(), reason);
    NimBLEDevice::deleteClient(pClient);
    NimBLEDevice::getScan()->start(scanTimeMs);
    connected = online = false;
  }
} clientCallbacks;

class ScanCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
    Serial.printf("Advertised Device found: %s\n", advertisedDevice->toString().c_str());
    if (advertisedDevice->haveName() && advertisedDevice->getName() == "BT+2.4G KB") {
      Serial.printf("Found Our Device\n");

      /** Async connections can be made directly in the scan callbacks */
      pClient = NimBLEDevice::getDisconnectedClient();
      if (!pClient) {
        pClient = NimBLEDevice::createClient(advertisedDevice->getAddress());
        if (!pClient) {
          Serial.printf("Failed to create client\n");
          return;
        }
      }
      pClient->setConnectionParams(12, 12, 0, 150);
      pClient->setConnectTimeout(5 * 1000);
      pClient->setClientCallbacks(&clientCallbacks, false);
      if (!pClient->connect(false, true, true)) {  // delete attributes, async connect, no MTU exchange
        NimBLEDevice::deleteClient(pClient);
        Serial.printf("Failed to connect\n");
        return;
      }
    }
  }

  void onScanEnd(const NimBLEScanResults& results, int reason) override {
    Serial.printf("Scan Ended\n");
    NimBLEDevice::getScan()->start(scanTimeMs);
  }
} scanCallbacks;

void setup() {

  Serial.begin(115200);
  while (!Serial)
    ;
  setvbuf(stdout, (char*)NULL, _IONBF, 0); /* Disable stdout buffering */
  Serial.printf("Starting NimBLE Async Client\n");
  NimBLEDevice::init("Async-Client");
  NimBLEDevice::setPower(3); /** +3db */
  NimBLEDevice::setSecurityAuth(BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_MITM | BLE_SM_PAIR_AUTHREQ_SC);
  NimBLEScan* pScan = NimBLEDevice::getScan();
  pScan->setScanCallbacks(&scanCallbacks);
  pScan->setInterval(45);
  pScan->setWindow(45);
  pScan->setActiveScan(true);
  pScan->start(scanTimeMs);
}

void Decode(uint8_t* data, size_t length) {
  int ndx = 2;
  char bfr[16];
  int caps = 0, skey = 1;
  std::vector<std::string> ref;

  if (!data[ndx])
    return;
  while (data[ndx++])
    ;
  ndx -= 2;
  if (lstkey > ndx) {
    lstkey = ndx;
    return;
  }
  // printf("%x:",data[ndx]);
  // Serial.printf("Code:%x ",data[ndx]);
  if (usb_codes.find(data[ndx]) != usb_codes.end()) {
    ref = usb_codes[data[ndx]];
  } else
    return;
  if (data[ndx] == 0x39) {
    cplock = cplock == 1 ? 3 : 1;
    return;
  }
  if (cplock == 3 && data[ndx] >= 0x4 && data[ndx] <= 0x1d)
    caps = 1;
  if (data[ndx] > 0x39)
    caps = skey = 0;
  strcpy(bfr, (data[0] & 0x22) ? ref[skey].c_str() : ref[caps].c_str());
  if (data[0] & 0x11)
    bfr[0] &= 0x1f;
  //Serial.print(bfr);
  SetChar(bfr);
  fflush(stdout);
  lstkey = ndx;
  return;
}

void notifyCB(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  Decode(pData, length);
}

void loop() {
  char buf;
  int ix = 0;

  delay(400);
  std::vector<NimBLEClient*> pClients = NimBLEDevice::getConnectedClients();
  if (!pClients.size()) {
    return;
  }
  pClient = pClients[0];
  if (connected && !online) {
    Serial.printf("Before Total heap: %d\r\n", ESP.getHeapSize());
    Serial.printf("Free heap: %d\r\n", ESP.getFreeHeap());
    Serial.printf("Alloc heap: %d\r\n", ESP.getMaxAllocHeap());
    Serial.printf("Finding services\r\n");
    pSvc = pClient->getService("1812");
    nulls[0] = 0;
    if (pSvc) {
      for (int i = 0; i < 4; i++) {
        pChr = pSvc->getCharacteristic("2a4d");
        pChr->unsubscribe(true);
        pChr->writeValue(nulls, 1, true);
        pChr->readValue();
        delay(10);
        pChr->subscribe(true, notifyCB);
        delay(10);
        online = true;
        Serial.printf("Online....\r\n");
        Serial.flush();
      }
      return;
    }
  }


  if (online ) {
    start_display(&online);
  }
}
