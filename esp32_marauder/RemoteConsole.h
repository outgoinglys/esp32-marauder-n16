#pragma once

#ifndef RemoteConsole_h
#define RemoteConsole_h

#include <Arduino.h>
#include <NimBLEDevice.h>

// A Stream that mirrors the Marauder shell to USB and to a phone over BLE.
// It uses the Nordic UART Service, supported by Serial Bluetooth Terminal on
// Android. Commands arrive through the characteristic write callback and are
// read by CommandLine exactly like USB input.
class RemoteConsole : public Stream {
  public:
    void begin();
    void begin(unsigned long baud);
    void maintain(bool bleBusy = false);

    size_t write(uint8_t c) override;
    size_t write(const uint8_t* buffer, size_t size) override;
    int available() override;
    int read() override;
    int peek() override;
    void flush() override;

    void onConnect(uint16_t handle);
    void onDisconnect();
    void onReceive(const uint8_t* data, size_t length);

  private:
    static const size_t RX_CAPACITY = 2048;
    // A help listing is about 2 KB and settings about 1 KB. The notification
    // FIFO only accepts a few packets at a time, so keep enough room for a
    // whole reply while the loop drains it.
    static const size_t TX_CAPACITY = 32768;

    char rx[RX_CAPACITY] = {};
    volatile size_t rx_head = 0;
    volatile size_t rx_tail = 0;
    volatile size_t rx_used = 0;
    portMUX_TYPE rx_mux = portMUX_INITIALIZER_UNLOCKED;

    char tx[TX_CAPACITY] = {};
    volatile size_t tx_head = 0;
    volatile size_t tx_tail = 0;
    volatile size_t tx_used = 0;
    portMUX_TYPE tx_mux = portMUX_INITIALIZER_UNLOCKED;

    volatile bool client_connected = false;
    volatile uint16_t conn_handle = BLE_HS_CONN_HANDLE_NONE;
    NimBLEServer* server = nullptr;
    NimBLECharacteristic* tx_char = nullptr;
    bool started = false;
    volatile bool radio_paused = false;
    uint32_t last_start_attempt = 0;
    uint32_t last_tx_time = 0;

    void startServer();
    void pushRx(uint8_t value);
    void pushTx(const uint8_t* data, size_t length);
    void drainTx();
};

extern RemoteConsole console_out;

#endif  // RemoteConsole_h
