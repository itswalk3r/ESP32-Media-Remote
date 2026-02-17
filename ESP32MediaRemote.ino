#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BluetoothA2DPSink.h>
#include <WiFi.h>

// OLED Display Settings (0.96" SSD1306)
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Bluetooth A2DP Sink
BluetoothA2DPSink a2dp_sink;

// Button Pins (connect to GND when pressed)
#define PLAY_PAUSE_BTN 13
#define PREV_BTN 12
#define NEXT_BTN 14

// WiFi Credentials (optional)
const char* ssid = "";      // Leave empty if no WiFi
const char* password = "";  // Leave empty if no WiFi

// Music Metadata Structure
struct MusicInfo {
  String title = "No Song Playing";
  String artist = "";
  String album = "";
  bool isPlaying = false;
  int volume = 50;
} currentMusic;

// Button Debouncing Structure
struct Button {
  int pin;
  bool lastState;
  bool pressed;
  unsigned long lastDebounceTime;
} buttons[3];

// Variables for scrolling
unsigned long lastScrollTime = 0;
int titleScrollPos = 0;
int artistScrollPos = 0;
int albumScrollPos = 0;

// UI State
bool showConnectionStatus = true;
unsigned long connectionStatusTimeout = 0;

void setup() {
  Serial.begin(115200);
  
  // Initialize OLED Display
  initializeOLED();
  
  // Initialize Buttons
  initializeButtons();
  
  // Connect to WiFi (if credentials provided)
  if(strlen(ssid) > 0) {
    connectToWiFi();
  } else {
    displayWiFiSkipped();
  }
  
  // Setup Bluetooth A2DP
  setupBluetooth();
  
  // Display initial splash screen
  displaySplashScreen();
}

void loop() {
  // Check for button presses
  checkButtons();
  
  // Update display periodically
  static unsigned long lastDisplayUpdate = 0;
  if(millis() - lastDisplayUpdate > 300) {
    updateDisplay();
    lastDisplayUpdate = millis();
  }
  
  // Handle text scrolling for long titles
  handleTextScrolling();
  
  // Hide connection status after timeout
  if(showConnectionStatus && millis() > connectionStatusTimeout) {
    showConnectionStatus = false;
  }
}

void initializeOLED() {
  Wire.begin(21, 22); // SDA=GPIO21, SCL=GPIO22 for ESP32
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    while(true);
  }
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.display();
  delay(100);
}

void initializeButtons() {
  int buttonPins[] = {PLAY_PAUSE_BTN, PREV_BTN, NEXT_BTN};
  
  for(int i = 0; i < 3; i++) {
    buttons[i].pin = buttonPins[i];
    buttons[i].lastState = HIGH;
    buttons[i].pressed = false;
    buttons[i].lastDebounceTime = 0;
    pinMode(buttonPins[i], INPUT_PULLUP);
  }
}

void connectToWiFi() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 24);
  display.println("Connecting WiFi...");
  display.display();
  
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while(WiFi.status() != WL_CONNECTED && attempts < 15) {
    delay(500);
    display.print(".");
    display.display();
    attempts++;
  }
  
  display.clearDisplay();
  display.setCursor(0, 24);
  
  if(WiFi.status() == WL_CONNECTED) {
    display.println("WiFi Connected");
  } else {
    display.println("WiFi Failed");
  }
  
  display.display();
  delay(1500);
}

void displayWiFiSkipped() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 24);
  display.println("WiFi: Disabled");
  display.display();
  delay(1500);
}

void setupBluetooth() {
  // Set callback for Bluetooth metadata (older library version)
  a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
  
  // Set volume - using older function name
  a2dp_sink.set_volume(100); // Set initial volume
  
  // Start Bluetooth A2DP sink
  a2dp_sink.start("ESP32 Music Remote");
  
  // Display Bluetooth status
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.println("Bluetooth Ready");
  display.setCursor(0, 35);
  display.println("Name: Music Remote");
  display.display();
  delay(1500);
}

// Bluetooth AVRCP Metadata Callback - older version compatible
void avrc_metadata_callback(uint8_t id, const uint8_t* text) {
  if (text == nullptr) return;
  
  String metadata = String((char*)text);
  
  switch(id) {
    case 1:  // ESP_AVRC_MD_ATTR_TITLE in older versions
      if(metadata.length() > 0) {
        currentMusic.title = metadata;
        titleScrollPos = 0;
      }
      break;
      
    case 2:  // ESP_AVRC_MD_ATTR_ARTIST
      if(metadata.length() > 0) {
        currentMusic.artist = metadata;
        artistScrollPos = 0;
      }
      break;
      
    case 4:  // ESP_AVRC_MD_ATTR_ALBUM
      if(metadata.length() > 0) {
        currentMusic.album = metadata;
        albumScrollPos = 0;
      }
      break;
  }
  
  Serial.printf("Metadata received - ID: %d, Text: %s\n", id, metadata.c_str());
}

// Connection state callback for older library
void bt_connection_state_changed(esp_a2d_connection_state_t state, void *ptr){
  showConnectionStatus = true;
  connectionStatusTimeout = millis() + 2000;
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 24);
  
  if(state == ESP_A2D_CONNECTION_STATE_CONNECTED){
    display.println("Device Connected");
    display.setCursor(0, 40);
    display.println("Play music to begin");
  } else if(state == ESP_A2D_CONNECTION_STATE_DISCONNECTED){
    display.println("Disconnected");
    currentMusic.title = "No Song Playing";
    currentMusic.artist = "";
    currentMusic.album = "";
  }
  
  display.display();
}

void checkButtons() {
  for(int i = 0; i < 3; i++) {
    bool currentState = digitalRead(buttons[i].pin);
    
    if(currentState == LOW && buttons[i].lastState == HIGH) {
      if(millis() - buttons[i].lastDebounceTime > 50) {
        buttons[i].pressed = true;
        buttons[i].lastDebounceTime = millis();
      }
    }
    
    if(buttons[i].pressed && currentState == HIGH) {
      buttons[i].pressed = false;
      handleButtonAction(i);
    }
    
    buttons[i].lastState = currentState;
  }
}

void handleButtonAction(int buttonIndex) {
  Serial.printf("Button %d pressed\n", buttonIndex);
  
  switch(buttonIndex) {
    case 0: // Play/Pause
      // For older library versions, use these commands:
      if(currentMusic.isPlaying) {
        a2dp_sink.pause();
      } else {
        a2dp_sink.play();
      }
      currentMusic.isPlaying = !currentMusic.isPlaying;
      Serial.println("Play/Pause toggled");
      break;
      
    case 1: // Previous
      a2dp_sink.previous();
      Serial.println("Previous track");
      break;
      
    case 2: // Next
      a2dp_sink.next();
      Serial.println("Next track");
      break;
  }
  
  // Show brief button feedback
  showButtonFeedback(buttonIndex);
}

void showButtonFeedback(int buttonIndex) {
  const char* icons[] = {"⏯", "⏮", "⏭"};
  
  display.clearDisplay();
  display.setTextSize(3);
  display.setCursor(50, 20);
  display.print(icons[buttonIndex]);
  display.display();
  delay(200);
}

void updateDisplay() {
  display.clearDisplay();
  
  // Draw top status bar
  drawStatusBar();
  
  // Draw separator line
  display.drawFastHLine(0, 10, SCREEN_WIDTH, SSD1306_WHITE);
  
  // Draw music info area
  drawMusicInfo();
  
  // Draw bottom control bar
  drawControlBar();
  
  display.display();
}

void drawStatusBar() {
  display.setTextSize(1);
  
  // Left: Now Playing
  display.setCursor(2, 2);
  display.print("▶ NOW PLAYING");
  
  // Right: Bluetooth status
  display.setCursor(SCREEN_WIDTH - 30, 2);
  display.print("[BT]");
}

void drawMusicInfo() {
  // Title (with scrolling)
  display.setTextSize(1);
  display.setCursor(0, 15);
  display.print("T: ");
  
  String displayTitle = currentMusic.title;
  if(displayTitle.length() > 18) {
    displayTitle = displayTitle.substring(titleScrollPos);
    if(displayTitle.length() < 18) {
      displayTitle = displayTitle + "  " + currentMusic.title;
    }
  }
  display.println(displayTitle.substring(0, 18));
  
  // Artist (with scrolling)
  display.setCursor(0, 27);
  display.print("A: ");
  
  String displayArtist = currentMusic.artist;
  if(displayArtist.length() > 18) {
    displayArtist = displayArtist.substring(artistScrollPos);
    if(displayArtist.length() < 18) {
      displayArtist = displayArtist + "  " + currentMusic.artist;
    }
  }
  display.println(displayArtist.substring(0, 18));
  
  // Album (with scrolling)
  display.setCursor(0, 39);
  display.print("B: ");
  
  String displayAlbum = currentMusic.album;
  if(displayAlbum.length() > 18) {
    displayAlbum = displayAlbum.substring(albumScrollPos);
    if(displayAlbum.length() < 18) {
      displayAlbum = displayAlbum + "  " + currentMusic.album;
    }
  }
  display.println(displayAlbum.substring(0, 18));
}

void drawControlBar() {
  // Draw separator line
  display.drawFastHLine(0, 52, SCREEN_WIDTH, SSD1306_WHITE);
  
  // Play/Pause status
  display.setTextSize(1);
  display.setCursor(2, 56);
  display.print(currentMusic.isPlaying ? "⏸ PAUSED" : "▶ PLAYING");
  
  // Button indicators
  display.setCursor(SCREEN_WIDTH - 36, 56);
  display.print("⏮ ⏯ ⏭");
}

void handleTextScrolling() {
  if(millis() - lastScrollTime > 350) {
    lastScrollTime = millis();
    
    if(currentMusic.title.length() > 18) {
      titleScrollPos = (titleScrollPos + 1) % currentMusic.title.length();
    }
    
    if(currentMusic.artist.length() > 18) {
      artistScrollPos = (artistScrollPos + 1) % currentMusic.artist.length();
    }
    
    if(currentMusic.album.length() > 18) {
      albumScrollPos = (albumScrollPos + 1) % currentMusic.album.length();
    }
  }
}

void displaySplashScreen() {
  display.clearDisplay();
  
  // Draw centered logo
  display.setTextSize(2);
  display.setCursor(20, 10);
  display.println("MUSIC");
  display.setCursor(25, 30);
  display.println("CTRL");
  
  // Version/subtitle
  display.setTextSize(1);
  display.setCursor(35, 50);
  display.println("v1.0");
  
  display.display();
  delay(2000);
  
  // Brief instructions
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(10, 10);
  display.println("Connect Bluetooth");
  display.setCursor(10, 25);
  display.println("from your device");
  display.setCursor(10, 40);
  display.println("and play music");
  display.display();
  delay(2000);
}
