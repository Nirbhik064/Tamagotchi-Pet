// Pin definitions (traced from schematic)
#define BTN_LEFT   D0   // SW1 → D0
#define BTN_MIDDLE D1   // SW2 → D1
#define BTN_RIGHT  D2   // SW3 → D2
#define BTN_INFO   D3   // SW4 → D3
#define SDA_PIN    D4   // OLED + MPU6050 SDA
#define SCL_PIN    D5   // OLED + MPU6050 SCL
#define DHT_PIN    D8   // DHT11 data
#define BUZZER_PIN D10  // Buzzer signal

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define MPU_ADDR 0x68

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHT dht(DHT_PIN, DHT11);

struct Pet {
  int hunger;       // 0 to 100 (0 = starving, 100 = full)
  int happiness;    // 0 to 100 (0 = miserable, 100 = ecstatic)
  int energy;       // 0 to 100 (0 = exhausted, 100 = fully rested)
  unsigned long age; // total seconds the pet has been alive
};
Pet pet;

enum Screen {
  SCREEN_MAIN,
  SCREEN_FEED,
  SCREEN_PLAY,
  SCREEN_SLEEP,
  SCREEN_ENV
};

Screen currentScreen = SCREEN_MAIN;

float temperature = 0;   // from DHT11 (°C)
float humidity = 0;      // from DHT11 (%)
float accelX = 0, accelY = 0, accelZ = 0;  // from MPU6050 (g)
bool mpuFound = false;

void setup() {
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_MIDDLE, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_INFO, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  Wire.begin(SDA_PIN, SCL_PIN);
  dht.begin();

  // Wake up the MPU6050
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  mpuFound = (Wire.endTransmission() == 0);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Tamagotchi Init...");
  display.display();
  delay(1000);

  pet.hunger = 80;
  pet.happiness = 80;
  pet.energy = 80;
  pet.age = 0;
}

unsigned long lastUpdate = 0;

void updatePet() {
  if (millis() - lastUpdate > 5000) {  // every 5 seconds
    pet.hunger--;
    pet.happiness--;
    pet.energy--;

    // too hot or too cold makes the pet extra unhappy
    if (temperature < 15 || temperature > 32) pet.happiness--;

    if (pet.hunger < 0) pet.hunger = 0;
    if (pet.happiness < 0) pet.happiness = 0;
    if (pet.energy < 0) pet.energy = 0;

    pet.age += 5;
    lastUpdate = millis();
  }
}

unsigned long lastSensorRead = 0;

void readSensors() {
  if (millis() - lastSensorRead < 2000) return;  // DHT11 is slow, every 2 seconds
  lastSensorRead = millis();

  // DHT11
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) temperature = t;
  if (!isnan(h)) humidity = h;
}

float lastMagnitude = 1.0;
unsigned long lastShake = 0;

void checkShake() {
  if (!mpuFound) return;

  // MPU6050
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 6);
  accelX = (int16_t)((Wire.read() << 8) | Wire.read()) / 16384.0;
  accelY = (int16_t)((Wire.read() << 8) | Wire.read()) / 16384.0;
  accelZ = (int16_t)((Wire.read() << 8) | Wire.read()) / 16384.0;

  float magnitude = sqrt(accelX * accelX + accelY * accelY + accelZ * accelZ);

  // shaking the pet makes it happier
  if (fabs(magnitude - lastMagnitude) > 0.8 && millis() - lastShake > 1000) {
    pet.happiness += 5;
    if (pet.happiness > 100) pet.happiness = 100;
    tone(BUZZER_PIN, 1500, 50);
    lastShake = millis();
  }
  lastMagnitude = magnitude;
}

unsigned long lastButtonPress = 0;

void checkButtons() {
  if (millis() - lastButtonPress < 200) return;  // debounce: ignore presses within 200ms

  if (digitalRead(BTN_LEFT) == LOW) {
    currentScreen = SCREEN_FEED;
    tone(BUZZER_PIN, 1000, 50);
    lastButtonPress = millis();
  }
  else if (digitalRead(BTN_MIDDLE) == LOW) {
    currentScreen = SCREEN_PLAY;
    tone(BUZZER_PIN, 1200, 50);
    lastButtonPress = millis();
  }
  else if (digitalRead(BTN_RIGHT) == LOW) {
    currentScreen = SCREEN_SLEEP;
    tone(BUZZER_PIN, 800, 50);
    lastButtonPress = millis();
  }
  else if (digitalRead(BTN_INFO) == LOW) {
    if (currentScreen == SCREEN_ENV) currentScreen = SCREEN_MAIN;
    else currentScreen = SCREEN_ENV;
    tone(BUZZER_PIN, 1400, 50);
    lastButtonPress = millis();
  }
}

void handleScreenLogic() {
  switch(currentScreen) {

    case SCREEN_FEED:
      pet.hunger += 10;
      if (pet.hunger > 100) pet.hunger = 100;
      currentScreen = SCREEN_MAIN;
      break;

    case SCREEN_PLAY:
      pet.happiness += 10;
      pet.energy -= 5;
      if (pet.happiness > 100) pet.happiness = 100;
      if (pet.energy < 0) pet.energy = 0;
      currentScreen = SCREEN_MAIN;
      break;

    case SCREEN_SLEEP:
      pet.energy += 15;
      if (pet.energy > 100) pet.energy = 100;
      currentScreen = SCREEN_MAIN;
      break;

    case SCREEN_MAIN:
      break;  // do nothing, just display stats

    case SCREEN_ENV:
      break;  // do nothing, just display sensor readings
  }
}

// Example: Happy face (all stats above 50)
const unsigned char PROGMEM petHappy[] = {
  0b00000000, 0b00000000,
  0b00011111, 0b11111000,
  0b00100000, 0b00000100,
  0b01000000, 0b00000010,
  0b01001100, 0b00110010,
  0b01001100, 0b00110010,
  0b01000000, 0b00000010,
  0b01000000, 0b00000010,
  0b01000100, 0b00100010,
  0b01000011, 0b11000010,
  0b01000000, 0b00000010,
  0b00100000, 0b00000100,
  0b00011111, 0b11111000,
  0b00000000, 0b00000000,
  0b00000000, 0b00000000,
  0b00000000, 0b00000000
};

// Example: Sad face (any stat below 30)
const unsigned char PROGMEM petSad[] = {
  0b00000000, 0b00000000,
  0b00011111, 0b11111000,
  0b00100000, 0b00000100,
  0b01000000, 0b00000010,
  0b01001100, 0b00110010,
  0b01001100, 0b00110010,
  0b01000000, 0b00000010,
  0b01000000, 0b00000010,
  0b01000000, 0b00000010,
  0b01000011, 0b11000010,
  0b01000100, 0b00100010,
  0b00100000, 0b00000100,
  0b00011111, 0b11111000,
  0b00000000, 0b00000000,
  0b00000000, 0b00000000,
  0b00000000, 0b00000000
};

// Example: Neutral face (everything else)
const unsigned char PROGMEM petNeutral[] = {
  0b00000000, 0b00000000,
  0b00011111, 0b11111000,
  0b00100000, 0b00000100,
  0b01000000, 0b00000010,
  0b01001100, 0b00110010,
  0b01001100, 0b00110010,
  0b01000000, 0b00000010,
  0b01000000, 0b00000010,
  0b01000111, 0b11100010,
  0b01000000, 0b00000010,
  0b01000000, 0b00000010,
  0b00100000, 0b00000100,
  0b00011111, 0b11111000,
  0b00000000, 0b00000000,
  0b00000000, 0b00000000,
  0b00000000, 0b00000000
};

// Example: Sleeping face (closed eyes)
const unsigned char PROGMEM petSleep[] = {
  0b00000000, 0b00000000,
  0b00011111, 0b11111000,
  0b00100000, 0b00000100,
  0b01000000, 0b00000010,
  0b01001111, 0b01110010,
  0b01000000, 0b00000010,
  0b01000000, 0b00000010,
  0b01000000, 0b00000010,
  0b01000011, 0b11000010,
  0b01000000, 0b00000010,
  0b01000000, 0b00000010,
  0b00100000, 0b00000100,
  0b00011111, 0b11111000,
  0b00000000, 0b00000000,
  0b00000000, 0b00000000,
  0b00000000, 0b00000000
};

void render() {
  display.clearDisplay();

  // Environment screen: DHT11 + MPU6050 readings
  if (currentScreen == SCREEN_ENV) {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("-- ENVIRONMENT --");

    display.setCursor(0, 14);
    display.print("Temp:  ");
    display.print(temperature, 1);
    display.println(" C");

    display.setCursor(0, 24);
    display.print("Humid: ");
    display.print(humidity, 0);
    display.println(" %");

    display.setCursor(0, 36);
    display.print("X:");
    display.print(accelX, 1);
    display.print(" Y:");
    display.print(accelY, 1);
    display.print(" Z:");
    display.print(accelZ, 1);

    display.setCursor(0, 56);
    display.println("[Back]");

    display.display();
    return;
  }

  // Choose the right sprite based on pet stats
  const unsigned char* sprite;
  if (pet.hunger < 30 || pet.happiness < 30 || pet.energy < 30) {
    sprite = petSad;
  } else if (pet.hunger > 50 && pet.happiness > 50 && pet.energy > 50) {
    sprite = petHappy;
  } else {
    sprite = petNeutral;
  }

  // Draw the pet sprite (centered horizontally, near the top)
  display.drawBitmap(56, 2, sprite, 16, 16, SSD1306_WHITE);

  // Draw stat bars below the pet
  display.setTextSize(1);

  display.setCursor(0, 24);
  display.print("HUN ");
  drawBar(24, 24, pet.hunger);

  display.setCursor(0, 34);
  display.print("HAP ");
  drawBar(24, 34, pet.happiness);

  display.setCursor(0, 44);
  display.print("ENG ");
  drawBar(24, 44, pet.energy);

  // Button labels at the bottom
  display.setCursor(0, 56);
  display.println("Feed Play Sleep Info");

  display.display();
}

// Draws a stat bar: empty rectangle with a filled portion based on value (0 to 100)
void drawBar(int x, int y, int value) {
  int barWidth = 100;
  int barHeight = 6;
  int fillWidth = map(value, 0, 100, 0, barWidth);

  display.drawRect(x, y, barWidth, barHeight, SSD1306_WHITE);       // outline
  display.fillRect(x, y, fillWidth, barHeight, SSD1306_WHITE);      // filled portion
}

void loop() {
  checkButtons();       // 1. Read input
  readSensors();        // 2. Read DHT11
  checkShake();         // 3. Read MPU6050
  updatePet();          // 4. Update state over time
  handleScreenLogic();  // 5. Process actions
  render();             // 6. Display results
  delay(100);           // 7. Short pause
}