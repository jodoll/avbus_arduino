#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include "AvBusReader.hpp"
#include "AvBusWriter.hpp"
#include "project.hpp"

#if defined(ESP32)
#include <SPIFFS.h>
#include "esp/AvWebserver.hpp"
#include "esp/CertificateStore.hpp"
#include "esp/EspAvBusClock.hpp"
#elif defined(UNO)
#include "uno/UnoAvBusClock.hpp"
#endif

#if defined(ESP32)
#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif
#endif

#if defined(UNO)
constexpr uint8_t BUS_INTERRUPT_PIN = 3;
constexpr uint8_t BUS_SEND_PIN = 4;
constexpr uint8_t CLOCK_INTERRUPT_PIN = 2;
#elif defined(ESP32)
constexpr uint8_t BUS_INTERRUPT_PIN = 39;
constexpr uint8_t BUS_SEND_PIN = 16;
constexpr uint8_t CLOCK_INTERRUPT_PIN = 17;
#endif

constexpr uint32_t CLOCK_FREQUENCY_HZ = 8000;

void clockInterruptHandler();
void busInterruptHandler();
void onClockTick();

#if defined(ESP32)
void setupFileSystem();
void serverTask(void* params);
#endif

#if defined(ESP32)
EspAvBusClock* avBusClock = new EspAvBusClock(CLOCK_FREQUENCY_HZ);
#elif defined(UNO)
UnoAvBusClock* avBusClock = new UnoAvBusClock(CLOCK_FREQUENCY_HZ, CLOCK_INTERRUPT_PIN);
#endif

AvBusReader* reader = new AvBusReader(avBusClock, BUS_INTERRUPT_PIN);
AvBusWriter* writer = new AvBusWriter(avBusClock, BUS_SEND_PIN);

void setup() {
  Serial.begin(115200);
  Serial.println("Starting up...");

#if defined(ESP32)
  setupFileSystem();
  CertificateStore::getInstance()->getCertificate();

  Serial.print("Connecting to: ");
  Serial.print(WIFI_SSID);
  WiFi.setHostname("avbus"); //TODO: Move this to a build flag
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  AvWebserver::setup(writer);
  int mainCore = xPortGetCoreID();
  Serial.printf("Main loop is running on core %d\n", mainCore);
  int webServerCore = (mainCore +1) % 2;
  Serial.printf("Starting WebServer on core %d\n", webServerCore);
  xTaskCreatePinnedToCore(serverTask, "https443", 6144, NULL, 1, NULL, webServerCore);
#endif

  pinMode(BUS_INTERRUPT_PIN, INPUT_PULLUP);
  pinMode(CLOCK_INTERRUPT_PIN, INPUT_PULLUP);
  pinMode(BUS_SEND_PIN, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(BUS_INTERRUPT_PIN), &busInterruptHandler, CHANGE);

  avBusClock->init(&clockInterruptHandler);
  avBusClock->registerTickCallback(&onClockTick);
}

void loop() {
#if defined(UNO)
  writer.setCommand(0b010110101010100);
  delay(1000);
#endif
}

void clockInterruptHandler() { avBusClock->tick(); }

void busInterruptHandler() { reader->onBusValueChanged(); }

void onClockTick() { writer->onClockTick(); }

#if defined(ESP32)
/**
 * This function is mostly taken from https://github.com/fhessel/esp32_https_server
 * released under the MIT License by Frank Hessel.
 */
void setupFileSystem() {
  // Try to mount SPIFFS without formatting on failure
  if (!SPIFFS.begin(false)) {
    // If SPIFFS does not work, we wait for serial connection...
    while(!Serial);
    delay(1000);

    // Ask to format SPIFFS using serial interface
    Serial.print("Mounting SPIFFS failed. Try formatting? (y/n): ");
    while(!Serial.available());
    Serial.println();

    // If the user did not accept to try formatting SPIFFS or formatting failed:
    if (Serial.read() != 'y' || !SPIFFS.begin(true)) {
      Serial.println("SPIFFS not available. Stop.");
      while(true);
    }
    Serial.println("SPIFFS has been formated.");
  }
  Serial.println("SPIFFS has been mounted.");
}

void serverTask(void* params) {
  AvWebserver::run();
  AvWebserver::tearDown();
}
#endif
