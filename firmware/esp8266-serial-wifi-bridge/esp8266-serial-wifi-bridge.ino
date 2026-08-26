#include <Arduino.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>

// Minimal serial-to-Wi-Fi bridge for an ESP8266 and MAX3232/DE9 carrier.
// This hardware uses normal ESP8266 UART0 GPIO1/GPIO3.
// No PPP, SLIP, HTTP, filesystem, OTA, update check, or flow control is
// implemented.

namespace {
constexpr uint32_t kDefaultBaud = 300;
constexpr uint16_t kTcpPort = 23;
constexpr uint8_t kWifiLedPin = 16;
constexpr uint8_t kFlashButtonPin = 0;
// V4-style MAX3232 carriers route GPIO13 from DB9 RTS and GPIO15 to DB9 CTS.
// Assert CTS exactly as the carrier's original firmware does even though the
// bridge does not otherwise implement hardware flow control.
constexpr uint8_t kRtsInputPin = 13;
constexpr uint8_t kCtsOutputPin = 15;
constexpr size_t kLineCapacity = 96;
constexpr uint8_t kConfigVersion = 3;
constexpr uint32_t kWifiConnectTimeoutMs = 20000;
constexpr uint32_t kReplyTurnaroundMs = 100;
constexpr uint32_t kBaudRecoveryHoldMs = 5000;
constexpr char kMagic[4] = {'S', 'W', 'B', '3'};
constexpr char kLegacyMagic[4] = {'S', 'W', 'B', '2'};

struct __attribute__((packed)) LegacyConfigV2 {
  char magic[4];
  uint8_t version;
  char ssid[33];
  char password[65];
  uint32_t checksum;
};

struct __attribute__((packed)) Config {
  char magic[4];
  uint8_t version;
  char ssid[33];
  char password[65];
  uint32_t baud;
  uint32_t checksum;
};

enum class InputState : uint8_t {
  Command,
  WifiSsid,
  WifiSsidConfirm,
  WifiPassword,
  WifiPasswordConfirm,
  WifiConfirm,
};

Config config{};
WiFiServer server(kTcpPort);
WiFiClient client;
InputState inputState = InputState::Command;
char line[kLineCapacity]{};
size_t lineLength = 0;
char pendingSsid[33]{};
char pendingPassword[65]{};
bool wifiWasConnected = false;
uint32_t activeBaud = kDefaultBaud;
uint32_t flashPressedAt = 0;
bool flashPressActive = false;
bool baudRecoveryTriggered = false;

uint32_t checksum(const uint8_t* data, size_t length) {
  uint32_t value = 2166136261UL;
  for (size_t i = 0; i < length; ++i) {
    value ^= data[i];
    value *= 16777619UL;
  }
  return value;
}

bool baudSupported(uint32_t baud) {
  switch (baud) {
    case 300:
    case 1200:
    case 2400:
    case 4800:
    case 9600:
    case 19200:
    case 38400:
    case 57600:
    case 115200:
      return true;
    default:
      return false;
  }
}

bool parseBaud(const String& text, uint32_t* baud) {
  if (text.length() == 0) return false;
  uint32_t value = 0;
  for (size_t i = 0; i < text.length(); ++i) {
    if (text[i] < '0' || text[i] > '9') return false;
    value = value * 10 + static_cast<uint32_t>(text[i] - '0');
  }
  if (!baudSupported(value)) return false;
  *baud = value;
  return true;
}

bool configValid() {
  if (memcmp(config.magic, kMagic, sizeof(kMagic)) != 0 ||
      config.version != kConfigVersion) {
    return false;
  }
  const uint32_t expected = checksum(
      reinterpret_cast<const uint8_t*>(&config), offsetof(Config, checksum));
  return expected == config.checksum && config.ssid[32] == '\0' &&
         config.password[64] == '\0' && baudSupported(config.baud);
}

bool legacyConfigValid(const LegacyConfigV2& legacy) {
  if (memcmp(legacy.magic, kLegacyMagic, sizeof(kLegacyMagic)) != 0 ||
      legacy.version != 2) {
    return false;
  }
  const uint32_t expected = checksum(
      reinterpret_cast<const uint8_t*>(&legacy),
      offsetof(LegacyConfigV2, checksum));
  return expected == legacy.checksum && legacy.ssid[32] == '\0' &&
         legacy.password[64] == '\0';
}

bool wifiConfigured() {
  return configValid() && config.ssid[0] != '\0';
}

bool writeConfig(Config& next) {
  next.checksum = checksum(
      reinterpret_cast<const uint8_t*>(&next), offsetof(Config, checksum));
  EEPROM.put(0, next);
  if (!EEPROM.commit()) return false;
  config = next;
  return true;
}

void loadConfig() {
  EEPROM.begin(sizeof(Config));
  EEPROM.get(0, config);
  if (configValid()) return;

  LegacyConfigV2 legacy{};
  EEPROM.get(0, legacy);
  if (legacyConfigValid(legacy)) {
    Config migrated{};
    memcpy(migrated.magic, kMagic, sizeof(kMagic));
    migrated.version = kConfigVersion;
    strncpy(migrated.ssid, legacy.ssid, sizeof(migrated.ssid) - 1);
    strncpy(migrated.password, legacy.password, sizeof(migrated.password) - 1);
    migrated.baud = kDefaultBaud;
    if (writeConfig(migrated)) return;
  }
  memset(&config, 0, sizeof(config));
}

bool saveConfig(const char* ssid, const char* password) {
  Config next{};
  memcpy(next.magic, kMagic, sizeof(kMagic));
  next.version = kConfigVersion;
  strncpy(next.ssid, ssid, sizeof(next.ssid) - 1);
  strncpy(next.password, password, sizeof(next.password) - 1);
  next.baud = configValid() ? config.baud : activeBaud;
  return writeConfig(next);
}

bool saveBaud(uint32_t baud) {
  Config next{};
  if (configValid()) {
    next = config;
  } else {
    memcpy(next.magic, kMagic, sizeof(kMagic));
    next.version = kConfigVersion;
  }
  next.baud = baud;
  return writeConfig(next);
}

void setWifiLed(bool on) {
  digitalWrite(kWifiLedPin, on ? LOW : HIGH);
}

void prompt() {
  Serial.print("SERIALWIFI> ");
}

void printHelp() {
  Serial.println();
  Serial.println("VINTAGE SERIAL WIFI BRIDGE COMMANDS");
  Serial.println("  AT       presence check");
  Serial.println("  WIFI     guided Wi-Fi setup");
  Serial.println("  STATUS   serial, Wi-Fi, IP, and TCP status");
  Serial.println("  BAUD     show active and saved serial speed");
  Serial.println("  BAUD N   save 300..115200; reset to apply");
  Serial.println("  CONNECT  connect using saved Wi-Fi settings");
  Serial.println("  HANGUP   close active TCP client");
  Serial.println("  HELP     show this list");
}

void printStatus() {
  Serial.println();
  Serial.println("BUILD: SERIAL-WIFI-BRIDGE-13-VARIABLE-BAUD-RC");
  Serial.print("SERIAL: V4 UART0 GPIO1/GPIO3 ");
  Serial.print(activeBaud);
  Serial.println(" 8-N-1 NO FLOW");
  Serial.print("SAVED BAUD: ");
  Serial.println(configValid() ? config.baud : kDefaultBaud);
  Serial.print("CONFIGURED: ");
  Serial.println(wifiConfigured() ? "YES" : "NO");
  if (wifiConfigured()) {
    Serial.print("SSID: ");
    Serial.println(config.ssid);
  }
  Serial.print("WIFI: ");
  Serial.println(WiFi.status() == WL_CONNECTED ? "CONNECTED" : "DISCONNECTED");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("TCP PORT: ");
    Serial.println(kTcpPort);
  }
  Serial.print("REMOTE: ");
  Serial.println(client && client.connected() ? "CONNECTED" : "IDLE");
}

bool connectWifiAndReport() {
  if (!wifiConfigured()) {
    Serial.println("NO SAVED WIFI CONFIGURATION. TYPE WIFI.");
    return false;
  }
  if (client) client.stop();
  WiFi.disconnect();
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.hostname("serial-wifi-bridge");
  WiFi.begin(config.ssid, config.password);
  wifiWasConnected = false;
  setWifiLed(false);
  Serial.print("CONNECTING TO ");
  Serial.println(config.ssid);

  const uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED &&
         static_cast<uint32_t>(millis() - started) < kWifiConnectTimeoutMs) {
    delay(50);
    yield();
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiWasConnected = true;
    setWifiLed(true);
    server.begin();
    server.setNoDelay(true);
    Serial.print("WIFI CONNECTED - IP ");
    Serial.println(WiFi.localIP());
    Serial.println("TCP 23 READY");
    return true;
  }

  WiFi.disconnect();
  setWifiLed(false);
  Serial.println("WIFI CONNECTION FAILED");
  return false;
}

void printBaud() {
  Serial.print("ACTIVE BAUD: ");
  Serial.println(activeBaud);
  Serial.print("SAVED BAUD: ");
  Serial.println(configValid() ? config.baud : kDefaultBaud);
  Serial.println("SUPPORTED: 300 1200 2400 4800 9600 19200 38400 57600 115200");
}

void setBaudAndReport(uint32_t requested) {
  if (!saveBaud(requested)) {
    Serial.println("BAUD SAVE FAILED");
    return;
  }
  Serial.print("BAUD SAVED: ");
  Serial.println(requested);
  if (requested == activeBaud) {
    Serial.println("BAUD ALREADY ACTIVE");
  } else {
    Serial.print("RESET ADAPTER, THEN SET TERMINAL TO ");
    Serial.println(requested);
  }
}

void finishLine() {
  line[lineLength] = '\0';
  delay(kReplyTurnaroundMs);
  Serial.println();

  if (inputState == InputState::WifiSsid) {
    if (lineLength == 0 || lineLength > 32) {
      Serial.println("SSID MUST BE 1-32 CHARACTERS. SETUP CANCELLED.");
      inputState = InputState::Command;
      prompt();
    } else {
      strncpy(pendingSsid, line, sizeof(pendingSsid) - 1);
      Serial.print("SSID RECEIVED: ");
      Serial.println(pendingSsid);
      Serial.print("USE THIS SSID (Y/N)? ");
      inputState = InputState::WifiSsidConfirm;
    }
  } else if (inputState == InputState::WifiSsidConfirm) {
    if (lineLength == 1 && (line[0] == 'Y' || line[0] == 'y')) {
      inputState = InputState::WifiPassword;
      Serial.print("PASSWORD (HIDDEN): ");
    } else {
      memset(pendingSsid, 0, sizeof(pendingSsid));
      inputState = InputState::WifiSsid;
      Serial.print("RE-ENTER SSID: ");
    }
  } else if (inputState == InputState::WifiPassword) {
    if (lineLength < 8 || lineLength > 63) {
      Serial.println("PASSWORD MUST BE 8-63 CHARACTERS. SETUP CANCELLED.");
      memset(pendingPassword, 0, sizeof(pendingPassword));
      inputState = InputState::Command;
      prompt();
    } else {
      strncpy(pendingPassword, line, sizeof(pendingPassword) - 1);
      Serial.print("PASSWORD RECEIVED: ");
      Serial.print(lineLength);
      Serial.println(" CHARACTERS");
      Serial.print("RE-ENTER PASSWORD (HIDDEN): ");
      inputState = InputState::WifiPasswordConfirm;
    }
  } else if (inputState == InputState::WifiPasswordConfirm) {
    if (strcmp(line, pendingPassword) != 0) {
      Serial.println("PASSWORDS DO NOT MATCH");
      Serial.print("RE-ENTER PASSWORD (HIDDEN): ");
    } else {
      Serial.println("PASSWORDS MATCH");
      Serial.print("SAVE AND CONNECT (Y/N)? ");
      inputState = InputState::WifiConfirm;
    }
  } else if (inputState == InputState::WifiConfirm) {
    if (lineLength == 1 && (line[0] == 'Y' || line[0] == 'y')) {
      if (saveConfig(pendingSsid, pendingPassword)) {
        Serial.println("SAVED");
        inputState = InputState::Command;
        memset(pendingPassword, 0, sizeof(pendingPassword));
        connectWifiAndReport();
        prompt();
      } else {
        Serial.println("SAVE FAILED");
        inputState = InputState::Command;
        prompt();
      }
    } else {
      Serial.println("SETUP CANCELLED; NOTHING SAVED");
      memset(pendingPassword, 0, sizeof(pendingPassword));
      inputState = InputState::Command;
      prompt();
    }
  } else {
    String command(line);
    command.trim();
    command.toUpperCase();
    if (command == "AT") {
      Serial.println("OK");
    } else if (command == "WIFI") {
      memset(pendingSsid, 0, sizeof(pendingSsid));
      memset(pendingPassword, 0, sizeof(pendingPassword));
      inputState = InputState::WifiSsid;
      Serial.print("SSID: ");
    } else if (command == "STATUS" || command == "ATI") {
      printStatus();
    } else if (command == "BAUD" || command == "AT$SB?") {
      printBaud();
    } else if (command == "BAUD RESET") {
      setBaudAndReport(kDefaultBaud);
    } else if (command.startsWith("BAUD ") ||
               command.startsWith("AT$SB=")) {
      const size_t offset = command.startsWith("BAUD ") ? 5 : 6;
      uint32_t requested = 0;
      if (parseBaud(command.substring(offset), &requested)) {
        setBaudAndReport(requested);
      } else {
        Serial.println("UNSUPPORTED BAUD - TYPE BAUD FOR OPTIONS");
      }
    } else if (command == "CONNECT" || command == "ATC1") {
      connectWifiAndReport();
    } else if (command == "HANGUP" || command == "ATH") {
      if (client) client.stop();
      Serial.println("REMOTE DISCONNECTED");
    } else if (command == "HELP" || command == "AT?" || command == "ATHELP") {
      printHelp();
    } else if (command.length() != 0) {
      Serial.println("ERROR - TYPE HELP");
    }
    if (inputState == InputState::Command) prompt();
  }

  memset(line, 0, sizeof(line));
  lineLength = 0;
}

void consumeSerialByte(uint8_t value) {
  if (value == '\n') return;
  if (value == '\r') {
    finishLine();
    return;
  }
  if (value == 0x08 || value == 0x7f) {
    if (lineLength > 0) {
      --lineLength;
      line[lineLength] = '\0';
    }
    return;
  }
  if (value < 0x20 || value > 0x7e || lineLength >= kLineCapacity - 1) return;
  line[lineLength++] = static_cast<char>(value);
}

void serviceWifi() {
  const bool connected = WiFi.status() == WL_CONNECTED;
  setWifiLed(connected);
  if (connected && !wifiWasConnected) {
    wifiWasConnected = true;
    server.begin();
    server.setNoDelay(true);
  } else if (!connected && wifiWasConnected) {
    wifiWasConnected = false;
    if (client) client.stop();
  }
}

void serviceBridge() {
  if (WiFi.status() != WL_CONNECTED) return;
  if ((!client || !client.connected()) && server.hasClient()) {
    if (client) client.stop();
    client = server.accept();
    client.setNoDelay(true);
    Serial.println("\r\nREMOTE CONNECTED");
  }
  if (!client || !client.connected()) return;

  while (client.available() > 0) {
    Serial.write(static_cast<uint8_t>(client.read()));
  }
  while (Serial.available() > 0) {
    client.write(static_cast<uint8_t>(Serial.read()));
  }
}

void serviceBaudRecoveryButton() {
  const bool pressed = digitalRead(kFlashButtonPin) == LOW;
  if (!pressed) {
    flashPressActive = false;
    flashPressedAt = 0;
    baudRecoveryTriggered = false;
    return;
  }
  if (!flashPressActive) {
    flashPressActive = true;
    flashPressedAt = millis();
    return;
  }
  if (baudRecoveryTriggered ||
      static_cast<uint32_t>(millis() - flashPressedAt) <
          kBaudRecoveryHoldMs) {
    return;
  }

  baudRecoveryTriggered = true;
  if (!saveBaud(kDefaultBaud)) {
    Serial.println("\r\nBAUD RECOVERY SAVE FAILED");
    return;
  }
  if (client) client.stop();
  Serial.println("\r\nBAUD RECOVERY SAVED: 300");
  Serial.println("RELEASE FLASH BUTTON TO RESTART");
  Serial.flush();
  for (uint8_t i = 0; i < 6; ++i) {
    setWifiLed(true);
    delay(75);
    setWifiLed(false);
    delay(75);
  }
  while (digitalRead(kFlashButtonPin) == LOW) {
    delay(10);
    yield();
  }
  delay(100);
  ESP.restart();
}

}  // namespace

void setup() {
  pinMode(kWifiLedPin, OUTPUT);
  setWifiLed(false);
  pinMode(kFlashButtonPin, INPUT_PULLUP);
  pinMode(kRtsInputPin, INPUT);
  pinMode(kCtsOutputPin, OUTPUT);
  digitalWrite(kCtsOutputPin, HIGH);

  loadConfig();
  activeBaud = configValid() ? config.baud : kDefaultBaud;
  Serial.setRxBufferSize(256);
  Serial.begin(activeBaud, SERIAL_8N1);
  Serial.setDebugOutput(false);
  delay(150);

  Serial.println();
  Serial.println("VINTAGE SERIAL WIFI BRIDGE 13 VARIABLE BAUD RC");
  Serial.print("UART0 GPIO1/GPIO3 ");
  Serial.print(activeBaud);
  Serial.println(" 8-N-1 NO FLOW");
  Serial.println("COMMAND ECHO: OFF");
  Serial.println("HOLD FLASH 5 SECONDS WHILE RUNNING TO RESTORE 300 BAUD");
  Serial.println("TERMINAL: DTR OFF, RTS OFF, 16550 FIFO ON");

  if (wifiConfigured()) {
    connectWifiAndReport();
  } else {
    Serial.println("TYPE WIFI FOR GUIDED SETUP OR HELP FOR COMMANDS");
  }
  prompt();
}

void loop() {
  serviceBaudRecoveryButton();
  serviceWifi();
  serviceBridge();

  if (!client || !client.connected()) {
    while (Serial.available() > 0) {
      consumeSerialByte(static_cast<uint8_t>(Serial.read()));
    }
  }
  yield();
}
