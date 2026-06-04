// reTerminal E Series - LED Control Example
// Supports E1001/E1002, E1003, and E1004

// ============================================================
// Device Selection
// Uncomment ONE line below for your device:
// ============================================================
#define DEVICE_E1001_E1002
// #define DEVICE_E1003
// #define DEVICE_E1004

// LED pin assignment per device
// LED pin differs across models
#ifdef DEVICE_E1001_E1002
  #define LED_PIN 6   // GPIO6 - Onboard LED (inverted logic)
#elif defined(DEVICE_E1003)
  #define LED_PIN 16  // GPIO16 - Onboard LED (inverted logic)
#elif defined(DEVICE_E1004)
  #define LED_PIN 48  // GPIO48 - Onboard LED (inverted logic)
#else
  #error "Please uncomment one DEVICE_ line above to select your model."
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
