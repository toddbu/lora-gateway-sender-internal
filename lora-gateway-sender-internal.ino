#include <Crypto.h>
#include <ChaCha.h>
#include <SPI.h>
#include <LoRa.h>
#include <LoRaCrypto.h>
#include <LoRaCryptoCreds.h>

LoRaCrypto* loRaCrypto;

enum MESSAGE_TYPE {
  messageTypeHealth = 1
};

void sendPacket(enum MESSAGE_TYPE messageType, byte* data, uint dataLength) {
  LoRa.idle();
  LoRa.beginPacket();
  
  byte encryptedMessage[255];
  uint encryptedMessageLength;
  uint32_t counter = loRaCrypto->encrypt(encryptedMessage, &encryptedMessageLength, 1, messageType, data, dataLength);
  LoRa.write(encryptedMessage, encryptedMessageLength);

  Serial.print("Sending packet: ");
  Serial.print(counter);
  Serial.print(", type = ");
  Serial.print(messageType);
  Serial.print(", length = ");
  Serial.println(encryptedMessageLength);

  LoRa.endPacket();
  delay(2000);
  LoRa.sleep();
}

#define RGB_BUILTIN 10

void setup() {
  pinMode(RGB_BUILTIN, OUTPUT);
  neopixelWrite(RGB_BUILTIN,0,0,RGB_BRIGHTNESS); // Blue

  Serial.begin(9600);
  delay(3000); // Give serial time to stabilize if connected
  Serial.println("Gateway Sender - Internal");

  LoRa.setPins(7, 8, 3);  // ESP32 C3 dev board #2

  if (!LoRa.begin(912900000)) {
    Serial.println("Starting LoRa failed! Waiting 60 seconds for restart...");
    delay(60000);
    ESP.restart();
    // while (1);
  }

  LoRa.setSpreadingFactor(10);
  LoRa.setSignalBandwidth(125000);
  LoRa.setCodingRate4(5);
  LoRa.setPreambleLength(8);
  LoRa.setSyncWord(0x12);
  LoRa.enableCrc();

  esp_sleep_enable_timer_wakeup(10000000);

  loRaCrypto = new LoRaCrypto(&encryptionCredentials);
 }

void loop() {
  // Determine why the MCU was woken up
  esp_sleep_wakeup_cause_t wakeupCause = esp_sleep_get_wakeup_cause();

  switch (wakeupCause) {
    case ESP_SLEEP_WAKEUP_TIMER:
      Serial.println("Wakeup from timer");

      // Send packet
      sendPacket(messageTypeHealth, (byte*) &hasMail, sizeof(hasMail));
      Serial.print("hasMail = ");
      Serial.println(hasMail);
      delay(1000);

      break;

    default:
      Serial.print("Ignore wakeup cause = ");
      Serial.println(wakeupCause);

      // One thing that causes unknown wakeup events is if you set gpio_wakeup_enable to
      // the same wakeup state as the pin is already in. So if we don't know what the
      // wakeup cause is then reset the gpio_wakeup_enable waekup value to the opposite
      // of what the pin state currently is. This should help greatly with debounce
      // gpio_wakeup_enable((gpio_num_t) UPPER_DOOR, upperDoorOpen ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL);
      // gpio_wakeup_enable((gpio_num_t) LOWER_DOOR, lowerDoorOpen ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL);
      // esp_sleep_enable_gpio_wakeup();
      // lastUpperDoorOpen = upperDoorOpen;
      // lastLowerDoorOpen = lowerDoorOpen;
      // getUpperDoorTriggered(lastUpperDoorOpen);
      // getLowerDoorTriggered(lastLowerDoorOpen);
}

  Serial.flush();
  esp_light_sleep_start();
}
