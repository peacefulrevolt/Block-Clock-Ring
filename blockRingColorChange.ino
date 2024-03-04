#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <FastLED.h>

#define LED_PIN     5
#define NUM_LEDS    60
#define BRIGHTNESS  240
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
CRGB leds[NUM_LEDS];

const int numberOfKnownNetworks = 1;
const char* knownSSIDs[numberOfKnownNetworks] = {"network"}; // Ensure network names are correctly filled
const char* knownPasswords[numberOfKnownNetworks] = {"password"}; // Ensure passwords are correctly filled

const char* blockHeightEndpoint = "https://mempool.space/api/blocks/tip/height";
long lastBlockHeight = -1;
bool newBlockFound = false;

unsigned long lastBlockCheckTime = 0;
const unsigned long blockCheckInterval = 10000; // 10 seconds in milliseconds

unsigned long cycleStartTime; // Tracks the start of the current cycle
const unsigned long cycleDuration = 10 * 60 * 1000; // 10 minutes in milliseconds
const int startSweepPosition = 29; // Desired start position for the sweep
int sweepPosition = 0;

int revCounter = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial); // Wait for serial port to connect

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(); // Ensure disconnection from previous connections
  delay(100);

  // Attempt to connect to known networks
  bool connected = false;
  for (int i = 0; i < numberOfKnownNetworks && !connected; i++) {
    WiFi.begin(knownSSIDs[i], knownPasswords[i]);
    Serial.print("Attempting to connect to ");
    Serial.println(knownSSIDs[i]);

    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 20000) {
      delay(500);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConnected!");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
      connected = true;
    } else {
      Serial.println("\nFailed to connect.");
    }
  }

  if (!connected) {
    Serial.println("Unable to connect to known networks. Check your credentials.");
    return;
  }

  cycleStartTime = millis(); // Initialize the cycle start time
}

void fetchBlockHeight() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(blockHeightEndpoint);
    int httpCode = http.GET();

    if (httpCode == 200) {
      String payload = http.getString();
      long currentBlockHeight = payload.toInt();
      Serial.print("Current Block Height: ");
      Serial.println(currentBlockHeight);

      if (lastBlockHeight != -1 && currentBlockHeight > lastBlockHeight) {
        Serial.println("New block found!");
        newBlockFound = true;
      }
      lastBlockHeight = currentBlockHeight;
    } else {
      Serial.print("Error fetching block height: ");
      Serial.println(httpCode);
    }
    http.end();
  } else {
    Serial.println("Wi-Fi not connected");
  }
}

void updateSweep() {
  unsigned long currentTime = millis();
  unsigned long elapsedTime = currentTime - cycleStartTime;

  // Calculate the position in the cycle as a fraction
  float cycleProgress = (float)elapsedTime / cycleDuration;

  int sweepCapture = sweepPosition; // Capture the current position

  // Calculate sweep position, starting from 29 and wrapping around
  sweepPosition = (startSweepPosition + (int)(NUM_LEDS * cycleProgress)) % NUM_LEDS;

  // Detect transition from position 28 to 29 and increment revolution counter
  if (sweepCapture == 28 && sweepPosition == 29) {
    revCounter++;
    sweepCapture = 0;
    if (revCounter >= 5) { // Reset after completing the sequence of colors
      revCounter = 0;
    }
  }

  // Select color based on revolutionCounter
  CRGB color;
  switch (revCounter) {
    case 0: color = CRGB::White; break;
    case 1: color = CRGB::Green; break;
    case 2: color = CRGB::Yellow; break;
    case 3: color = CRGB::Blue; break;
    case 4: color = CRGB::Red; break;
    default: color = CRGB::White; // Fallback
  }

  // Clear the strip and set brightness based on distance from sweepPosition
  fill_solid(leds, NUM_LEDS, CRGB::Black);

  // Define brightness levels
  int brightnessLevels[8] = {0, 0, 0, 32, 64, 128, 240, 240};

  for (int i = 0; i < NUM_LEDS; i++) {
    int distance = abs(i - sweepPosition); // Ensure you are using the correct variable for sweep position
    if (distance > NUM_LEDS / 2) {
      distance = NUM_LEDS - distance;
    }
    int brightnessIndex = 7 - distance;
    if (brightnessIndex >= 0 && brightnessIndex < 8) {
      leds[i] = color;
      leds[i].fadeToBlackBy(255 - brightnessLevels[brightnessIndex]);
    }
  }

  FastLED.show();
}


void chaseOrangeLights() {
    fill_solid(leds, NUM_LEDS, CRGB::Black); // Clear the strip before starting the chase
    FastLED.show();

    const int chaseDuration = 3000; // Duration of the chase effect in milliseconds
    const int chaseSpeed = 50; // Speed of the chase effect in milliseconds (delay between steps)
    const CRGB chaseColor = CRGB(255, 165, 0); // Orange color

    unsigned long chaseStartTime = millis();
    while (millis() - chaseStartTime < chaseDuration) {
        for (int i = 0; i < NUM_LEDS; ++i) {
            leds[i] = chaseColor;
            FastLED.show();
            delay(chaseSpeed);
            leds[i] = CRGB::Black;
        }
    }

    // Optionally clear the strip after the chase effect
    fill_solid(leds, NUM_LEDS, CRGB::Black); 
    FastLED.show();
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastBlockCheckTime >= blockCheckInterval) {
    fetchBlockHeight();
    lastBlockCheckTime = currentMillis; // Update the last check time
  }

  if (newBlockFound) {
    chaseOrangeLights(); // Execute orange lights chase effect
    newBlockFound = false;
    revCounter = 0;
    cycleStartTime = currentMillis; // Restart the ten-minute timer
  }

  // The call to updateSweep() is now correctly placed to ensure it's updated regularly.
  updateSweep(); // Regular sweep update
}
