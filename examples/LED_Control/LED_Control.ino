// reTerminal E Series - LED Control Example
// Supports E1001/E1002, E1003, and E1004
//
// DEVICE_MODEL selects the correct pin mapping for your board.
// When compiling locally in Arduino IDE, change the default below.
// When built by CI / Firmware Hub, the value is injected via build flags.

#ifndef DEVICE_MODEL
#define DEVICE_MODEL 1001
#endif

#if DEVICE_MODEL == 1001 || DEVICE_MODEL == 1002
  #define LED_PIN 6   // GPIO6 - Onboard LED (inverted logic)
#elif DEVICE_MODEL == 1003
  #define LED_PIN 16  // GPIO16 - Onboard LED (inverted logic)
#elif DEVICE_MODEL == 1004
  #define LED_PIN 48  // GPIO48 - Onboard LED (inverted logic)
#else
  #error "Unknown DEVICE_MODEL — expected 1001, 1002, 1003, or 1004."
#endif

#define SERIAL_RX 44
#define SERIAL_TX 43

void setup() {
  Serial1.begin(115200, SERIAL_8N1, SERIAL_RX, SERIAL_TX);
  while (!Serial1) {
    delay(10);
  }

  Serial1.println("LED Control Example");

  // Configure LED pin
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  // Turn LED ON (LOW because it's inverted)
  digitalWrite(LED_PIN, LOW);
  Serial1.println("LED ON");
  delay(1000);

  // Turn LED OFF (HIGH because it's inverted)
  digitalWrite(LED_PIN, HIGH);
  Serial1.println("LED OFF");
  delay(1000);
}
