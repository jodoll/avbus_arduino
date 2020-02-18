#include <Arduino.h>
#include <Wire.h>
#include "AvBusClock.hpp"
#include "AvBusReader.hpp"
#include "AvBusWriter.hpp"
#include "project.hpp"

#if defined(ESP32)
#include "esp/AvWebserver.hpp"
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

AvBusClock avBusClock(CLOCK_FREQUENCY_HZ, CLOCK_INTERRUPT_PIN);
AvBusReader reader(&avBusClock, BUS_INTERRUPT_PIN);
AvBusWriter writer(&avBusClock, BUS_SEND_PIN);
#if defined(ESP32)
AvWebserver webserver(&writer);
hw_timer_t* timer = NULL;
#endif

void setup() {
  Serial.begin(115200);
  Serial.println("Starting up...");

#if defined(ESP32)
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  webserver.start();

  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onClockTick, true);
  timerAlarmWrite(timer, 125, true);
  timerAlarmEnable(timer);
#else
  Wire.begin();
  avBusClock.init();
#endif
  pinMode(BUS_INTERRUPT_PIN, INPUT_PULLUP);
  pinMode(CLOCK_INTERRUPT_PIN, INPUT_PULLUP);
  pinMode(BUS_SEND_PIN, OUTPUT);
  // Tune up
  avBusClock.registerTickCallback(&onClockTick);

  attachInterrupt(digitalPinToInterrupt(BUS_INTERRUPT_PIN), &busInterruptHandler, CHANGE);
  //attachInterrupt(digitalPinToInterrupt(CLOCK_INTERRUPT_PIN), &clockInterruptHandler, RISING);
}

void loop() {
#if defined(UNO)
  writer.setCommand(0b010110101010100);
  delay(1000);
#endif
}

void clockInterruptHandler() { avBusClock.tick(); }

void busInterruptHandler() { reader.onBusValueChanged(); }

void onClockTick() { writer.onClockTick(); }
