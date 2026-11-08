#include "RemoteConsole.h"

// Nordic UART Service. Serial Bluetooth Terminal, nRF Connect and BLE Serial
// Monitor all understand this profile without extra setup.
static const char* CONSOLE_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
static const char* CONSOLE_RX_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
static const char* CONSOLE_TX_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";

RemoteConsole console_out;

namespace {
  class ConsoleServerCallbacks : public NimBLEServerCallbacks {
    public:
      void onConnect(NimBLEServer* server, NimBLEConnInfo& connInfo) override {
        console_out.onConnect(connInfo.getConnHandle());
      }

      void onDisconnect(NimBLEServer* server, NimBLEConnInfo& connInfo, int reason) override {
        console_out.onDisconnect();
      }
  };

  class ConsoleRxCallbacks : public NimBLECharacteristicCallbacks {
    public:
      void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo) override {
        NimBLEAttValue value = characteristic->getValue();
        console_out.onReceive(value.data(), value.size());
      }
  };

  ConsoleServerCallbacks server_callbacks;
  ConsoleRxCallbacks rx_callbacks;
}

void RemoteConsole::begin(unsigned long baud) {
  Serial.begin(baud);
}

void RemoteConsole::startServer() {
  server = NimBLEDevice::createServer();
  if (server == nullptr) {
    Serial.println("[BLE] createServer failed");
    return;
  }
  server->setCallbacks(&server_callbacks, false);
  server->advertiseOnDisconnect(true);

  NimBLEService* service = server->createService(CONSOLE_SERVICE_UUID);
  if (service == nullptr) {
    Serial.println("[BLE] createService failed");
    return;
  }

  tx_char = service->createCharacteristic(CONSOLE_TX_UUID,
                                          NIMBLE_PROPERTY::NOTIFY,
                                          512);
  NimBLECharacteristic* rx_char = service->createCharacteristic(
      CONSOLE_RX_UUID,
      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR,
      512);
  if ((tx_char == nullptr) || (rx_char == nullptr)) {
    Serial.println("[BLE] createCharacteristic failed");
    return;
  }
  rx_char->setCallbacks(&rx_callbacks);
  service->start();

  // A 128-bit service UUID plus the device name exceeds the 31-byte
  // advertisement. Keep the UUID in the advertisement and the human-readable
  // name in the scan response, where scanners will show it.
  NimBLEAdvertisementData advertisement;
  advertisement.addServiceUUID(CONSOLE_SERVICE_UUID);

  NimBLEAdvertisementData scan_response;
  scan_response.setName("Marauder-C5");

  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
  advertising->setAdvertisementData(advertisement);
  advertising->setScanResponseData(scan_response);
  advertising->enableScanResponse(true);
  const bool advertising_started = advertising->start();

  started = true;
  Serial.printf("[BLE] console Marauder-C5 advertising=%d\n", advertising_started ? 1 : 0);
}

void RemoteConsole::begin() {
  if (started) {
    return;
  }

  const uint32_t now = millis();
  if ((now - last_start_attempt) < 1000) {
    return;
  }
  last_start_attempt = now;

  const bool ble_initialized = NimBLEDevice::isInitialized();
  if (!ble_initialized) {
    // Marauder's own BLE scans leave a stale host behind; clear it first.
    if (NimBLEDevice::getServer() != nullptr) {
      Serial.println("[BLE] discarding stale host");
      NimBLEDevice::deinit(true);
    }
    if (!NimBLEDevice::init("Marauder-C5")) {
      Serial.println("[BLE] NimBLE init failed");
      return;
    }
    NimBLEDevice::setMTU(517);
    Serial.println("[BLE] NimBLE host initialized");
  } else if (NimBLEDevice::getServer() != nullptr) {
    // Another part of the firmware already owns the GATT server.
    started = true;
    return;
  }

  startServer();
}

void RemoteConsole::maintain(bool bleBusy) {
  radio_paused = bleBusy;

  // Marauder's Bluetooth functions call NimBLEDevice::deinit(), which frees
  // the GATT server and every characteristic. Drop our pointers before they
  // can be used, then rebuild the console when the scan has finished.
  if (bleBusy) {
    if (started) {
      started = false;
      client_connected = false;
      conn_handle = BLE_HS_CONN_HANDLE_NONE;
      tx_char = nullptr;
      server = nullptr;
    }
    return;
  }

  if (!started) {
    begin();
  }

  if (started && (!NimBLEDevice::isInitialized() ||
                  (NimBLEDevice::getServer() != server))) {
    // A Bluetooth scan tore the host down. Rebuild once the radio is idle so
    // the phone can reconnect without power cycling the board.
    started = false;
    client_connected = false;
    conn_handle = BLE_HS_CONN_HANDLE_NONE;
    tx_char = nullptr;
    server = nullptr;
    last_start_attempt = 0;
  }

  drainTx();

  if (started && (server != nullptr)) {
    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    if ((advertising != nullptr) && !advertising->isAdvertising()) {
      advertising->start();
    }
  }
}

size_t RemoteConsole::write(uint8_t c) {
  return write(&c, 1);
}

size_t RemoteConsole::write(const uint8_t* buffer, size_t size) {
  Serial.write(buffer, size);
  pushTx(buffer, size);
  drainTx();
  return size;
}

int RemoteConsole::available() {
  size_t queued = 0;
  portENTER_CRITICAL(&rx_mux);
  queued = rx_used;
  portEXIT_CRITICAL(&rx_mux);
  if (queued > 0) {
    return static_cast<int>(queued);
  }
  return Serial.available();
}

int RemoteConsole::read() {
  int value = -1;
  portENTER_CRITICAL(&rx_mux);
  if (rx_used > 0) {
    value = static_cast<uint8_t>(rx[rx_tail]);
    rx_tail = (rx_tail + 1) % RX_CAPACITY;
    rx_used--;
  }
  portEXIT_CRITICAL(&rx_mux);

  if (value >= 0) {
    return value;
  }
  return Serial.read();
}

int RemoteConsole::peek() {
  int value = -1;
  portENTER_CRITICAL(&rx_mux);
  if (rx_used > 0) {
    value = static_cast<uint8_t>(rx[rx_tail]);
  }
  portEXIT_CRITICAL(&rx_mux);

  if (value >= 0) {
    return value;
  }
  return Serial.peek();
}

void RemoteConsole::flush() {
  Serial.flush();
  drainTx();
}

void RemoteConsole::onConnect(uint16_t handle) {
  client_connected = true;
  conn_handle = handle;
  last_tx_time = 0;
  if (server != nullptr) {
    server->updateConnParams(handle, 12, 24, 0, 200);
  }

  // Drop output that was buffered while no phone was listening.
  portENTER_CRITICAL(&tx_mux);
  tx_head = 0;
  tx_tail = 0;
  tx_used = 0;
  portEXIT_CRITICAL(&tx_mux);

  static const char welcome[] =
      "\r\nMarauder-C5 BLE console ready.\r\n"
      "End every command with CR/LF.\r\n"
      "Type: info, help, scanall, list -a, stopscan\r\n> ";
  pushTx(reinterpret_cast<const uint8_t*>(welcome), sizeof(welcome) - 1);
}

void RemoteConsole::onDisconnect() {
  client_connected = false;
  conn_handle = BLE_HS_CONN_HANDLE_NONE;
}

void RemoteConsole::onReceive(const uint8_t* data, size_t length) {
  bool ended_with_newline = false;
  for (size_t i = 0; i < length; i++) {
    uint8_t c = data[i];
    if ((c == '\r') || (c == '\n')) {
      bool last_is_newline = false;
      portENTER_CRITICAL(&rx_mux);
      if (rx_used > 0) {
        size_t last = (rx_head + RX_CAPACITY - 1) % RX_CAPACITY;
        last_is_newline = (rx[last] == '\n');
      }
      portEXIT_CRITICAL(&rx_mux);
      if (!last_is_newline) {
        pushRx('\n');
      }
      ended_with_newline = true;
    } else if ((c == 0x08) || (c == 0x7f)) {
      portENTER_CRITICAL(&rx_mux);
      if (rx_used > 0) {
        size_t previous = (rx_head + RX_CAPACITY - 1) % RX_CAPACITY;
        if (rx[previous] != '\n') {
          rx_head = previous;
          rx_used--;
        }
      }
      portEXIT_CRITICAL(&rx_mux);
      ended_with_newline = false;
    } else {
      pushRx(c);
      ended_with_newline = false;
    }
  }

  // Some Android terminals deliver a finished command as one characteristic
  // write but do not add CR/LF. Treat a multi-character write as a complete
  // line. Per-character writes are left alone until the user presses Enter.
  if ((length > 1) && !ended_with_newline) {
    pushRx('\n');
  }
}

void RemoteConsole::pushRx(uint8_t value) {
  portENTER_CRITICAL(&rx_mux);
  if (rx_used >= RX_CAPACITY) {
    rx_tail = (rx_tail + 1) % RX_CAPACITY;
    rx_used--;
  }
  rx[rx_head] = static_cast<char>(value);
  rx_head = (rx_head + 1) % RX_CAPACITY;
  rx_used++;
  portEXIT_CRITICAL(&rx_mux);
}

void RemoteConsole::pushTx(const uint8_t* data, size_t length) {
  portENTER_CRITICAL(&tx_mux);
  for (size_t i = 0; i < length; i++) {
    if (tx_used >= TX_CAPACITY) {
      tx_tail = (tx_tail + 1) % TX_CAPACITY;
      tx_used--;
    }
    tx[tx_head] = static_cast<char>(data[i]);
    tx_head = (tx_head + 1) % TX_CAPACITY;
    tx_used++;
  }
  portEXIT_CRITICAL(&tx_mux);
}

void RemoteConsole::drainTx() {
  if (!client_connected || (server == nullptr) || (tx_char == nullptr)) {
    return;
  }

  // The controller keeps only a few notification buffers. One nominal BLE
  // chunk per loop keeps long replies intact; larger writes are broken up by
  // the stack into 20-byte packets anyway.
  size_t chunk = 20;

  // Let NimBLE send one notification per connection event instead of
  // flooding it, so a server callback has room to queue the packet.

  uint8_t buffer[180];
  for (size_t sent = 0; sent < 1; sent++) {
    size_t count = 0;
    portENTER_CRITICAL(&tx_mux);
    if (tx_used > 0) {
      count = (tx_used < chunk) ? tx_used : chunk;
      for (size_t i = 0; i < count; i++) {
        buffer[i] = static_cast<uint8_t>(tx[(tx_tail + i) % TX_CAPACITY]);
      }
    }
    portEXIT_CRITICAL(&tx_mux);

    if (count == 0) {
      break;
    }
    if (!tx_char->notify(buffer, count, conn_handle)) {
      // The BLE stack has no free notification buffer right now. Keep the
      // data queued and retry from maintain() on the next loop instead of
      // throwing away console output.
      break;
    }

    portENTER_CRITICAL(&tx_mux);
    if (tx_used >= count) {
      tx_tail = (tx_tail + count) % TX_CAPACITY;
      tx_used -= count;
    } else {
      tx_head = 0;
      tx_tail = 0;
      tx_used = 0;
    }
    portEXIT_CRITICAL(&tx_mux);
  }
}
