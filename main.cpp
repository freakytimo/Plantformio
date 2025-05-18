#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <Preferences.h>

const char* ssid = "xxx";
const char* password = "xxx";

WiFiServer server(80);
String header;

const int statusLedPin = 23;
const bool RELAY_ACTIVE_HIGH = true;

const int NUM_RELAYS = 4;
const int relayPins[NUM_RELAYS] = {32, 33, 25, 26};
String relayState[NUM_RELAYS] = {"off", "off", "off", "off"};

String onTimes[NUM_RELAYS]  = {"--:--", "--:--", "--:--", "--:--"};
String offTimes[NUM_RELAYS] = {"--:--", "--:--", "--:--", "--:--"};
bool timerEnabled[NUM_RELAYS] = {true, true, true, true};

Preferences preferences;


const int fanPin = 27;
const int fanChannel = 0;
const int fanFrequency = 25000;
const int fanResolution = 8;
int fanSpeedPercent = 0;

void setFanSpeed(int percent) {
  percent = constrain(percent, 0, 100);
  fanSpeedPercent = percent;
  int pwmValue = map(percent, 0, 100, 0, 255);
  ledcWrite(fanChannel, pwmValue);
  Serial.println("Lüftergeschwindigkeit gesetzt: " + String(percent) + "%");

  preferences.begin("fan", false);
  if (preferences.getInt("speed", -1) != percent) {
  preferences.putInt("speed", percent);
  }
  preferences.end();
}

void loadStoredSettings() {
  preferences.begin("relay-times", true);
  for (int i = 0; i < NUM_RELAYS; i++) {
    onTimes[i] = preferences.getString(("on" + String(i)).c_str(), "--:--");
    offTimes[i] = preferences.getString(("off" + String(i)).c_str(), "--:--");
    timerEnabled[i] = preferences.getBool(("timer" + String(i)).c_str(), true);
  }
  preferences.end();

  preferences.begin("fan", true);
  fanSpeedPercent = preferences.getInt("speed", 0);
  preferences.end();

  Serial.println("Einstellungen geladen.");
}

void saveSettings() {
  preferences.begin("relay-times", false);
  for (int i = 0; i < NUM_RELAYS; i++) {
    preferences.putString(("on" + String(i)).c_str(), onTimes[i]);
    preferences.putString(("off" + String(i)).c_str(), offTimes[i]);
    preferences.putBool(("timer" + String(i)).c_str(), timerEnabled[i]);
  }
  preferences.end();

  preferences.begin("fan", false);
  preferences.putInt("speed", fanSpeedPercent);
  preferences.end();

  Serial.println("Einstellungen gespeichert.");
}

int timeStringToMinutes(const String& t) {
  if (t.length() != 5 || t.charAt(2) != ':') return -1;
  int h = t.substring(0, 2).toInt();
  int m = t.substring(3, 5).toInt();
  if (h < 0 || h > 23 || m < 0 || m > 59) return -1;
  return h * 60 + m;
}

void checkRelaySchedule() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Uhrzeit konnte nicht geladen werden.");
    return;
  }

  int currentMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  char formattedTime[20];
  strftime(formattedTime, sizeof(formattedTime), "%H:%M:%S", &timeinfo);
  Serial.println("Aktuelle Uhrzeit: " + String(formattedTime));

  for (int i = 0; i < NUM_RELAYS; i++) {
    if (!timerEnabled[i]) {
      Serial.println("Timer für Relais " + String(i + 1) + " ist deaktiviert.");
      continue;
    }

    int onMins = timeStringToMinutes(onTimes[i]);
    int offMins = timeStringToMinutes(offTimes[i]);

    if (onMins >= 0 && offMins >= 0) {
      bool inRange = (onMins < offMins)
        ? (currentMinutes >= onMins && currentMinutes < offMins)
        : (currentMinutes >= onMins || currentMinutes < offMins);

      if (inRange && relayState[i] != "on") {
        relayState[i] = "on";
        digitalWrite(relayPins[i], RELAY_ACTIVE_HIGH ? HIGH : LOW);
        Serial.println("Relais " + String(i + 1) + " EIN (Zeitplan)");
      } else if (!inRange && relayState[i] != "off") {
        relayState[i] = "off";
        digitalWrite(relayPins[i], RELAY_ACTIVE_HIGH ? LOW : HIGH);
        Serial.println("Relais " + String(i + 1) + " AUS (Zeitplan)");
      }
    } else {
      Serial.println("Ungültige Zeit für Relais " + String(i + 1));
    }
  }
}

void handleTimeInputs(const String& url) {
  bool timerFormSubmitted = false;

  for (int i = 0; i < NUM_RELAYS; i++) {
    if (url.indexOf("on" + String(i + 1) + "=") != -1 ||
        url.indexOf("off" + String(i + 1) + "=") != -1 ||
        url.indexOf("timer" + String(i + 1) + "=") != -1) {
      timerFormSubmitted = true;
      break;
    }
  }

  if (timerFormSubmitted) {
    for (int i = 0; i < NUM_RELAYS; i++) {
      String keyOn = "on" + String(i + 1) + "=";
      String keyOff = "off" + String(i + 1) + "=";
      String keyTimer = "timer" + String(i + 1) + "=";

      int onIdx = url.indexOf(keyOn);
      if (onIdx != -1) {
        onIdx += keyOn.length();
        int endIdx = url.indexOf("&", onIdx);
        if (endIdx == -1) endIdx = url.length();
        onTimes[i] = url.substring(onIdx, endIdx);
        onTimes[i].replace("%3A", ":");
      }

      int offIdx = url.indexOf(keyOff);
      if (offIdx != -1) {
        offIdx += keyOff.length();
        int endIdx = url.indexOf("&", offIdx);
        if (endIdx == -1) endIdx = url.length();
        offTimes[i] = url.substring(offIdx, endIdx);
        offTimes[i].replace("%3A", ":");
      }

      timerEnabled[i] = false;

      int searchIdx = 0;
      while (true) {
        int timerIdx = url.indexOf(keyTimer, searchIdx);
        if (timerIdx == -1) break;

        timerIdx += keyTimer.length();
        int endIdx = url.indexOf("&", timerIdx);
        if (endIdx == -1) endIdx = url.length();
        String val = url.substring(timerIdx, endIdx);

        if (val == "1") {
          timerEnabled[i] = true;
          break;
        }

        searchIdx = endIdx;
      }
    }
  }

  bool fanChanged = false;
  int fanIdx = url.indexOf("fan=");
  if (fanIdx != -1) {
    fanIdx += 4;
    int endIdx = url.indexOf("&", fanIdx);
    if (endIdx == -1) endIdx = url.length();
    int val = url.substring(fanIdx, endIdx).toInt();
    setFanSpeed(val);
    fanChanged = true;
  }

  if (timerFormSubmitted || fanChanged) {
    saveSettings();
  }
}

void handleRelayCommands(const String& header) {
  for (int i = 0; i < NUM_RELAYS; i++) {
    String onCmd = "GET /" + String(i + 1) + "/on";
    String offCmd = "GET /" + String(i + 1) + "/off";
    if (header.indexOf(onCmd) != -1) {
      relayState[i] = "on";
      digitalWrite(relayPins[i], RELAY_ACTIVE_HIGH ? HIGH : LOW);
      Serial.println("Relais " + String(i + 1) + " MANUELL EIN");
    }
    if (header.indexOf(offCmd) != -1) {
      relayState[i] = "off";
      digitalWrite(relayPins[i], RELAY_ACTIVE_HIGH ? LOW : HIGH);
      Serial.println("Relais " + String(i + 1) + " MANUELL AUS");
    }
  }
}

void generateWebPage(WiFiClient& client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/html");
  client.println("Connection: close");
  client.println();

  client.println("<!DOCTYPE html><html><head>");
  client.println("<meta charset=\"UTF-8\">");
  client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
  client.println("<meta http-equiv=\"refresh\" content=\"60\">");
  client.println("<style>");
  client.println("html { font-family: Helvetica; text-align: center; }");
  client.println(".button { padding: 10px 25px; font-size: 20px; border: none; color: white; margin: 10px; cursor: pointer; }");
  client.println(".on-button { background-color: #4CAF50; }");
  client.println(".off-button { background-color: #9d1d14; }");
  client.println(".blue-button { background-color: #003366; }");
  client.println(".row { display: flex; justify-content: center; flex-wrap: wrap; }");
  client.println(".box { width: 20%; min-width: 200px; margin: 10px; border: 1px solid #ccc; padding: 10px; box-sizing: border-box; }");
  client.println("</style></head><body>");

  

  client.println("<h2>Plantformio</h2>");

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    client.println("<p><b>Uhrzeit:</b> Zeit konnte nicht geladen werden</p>");
  } else {
    char timeStr[64];
    strftime(timeStr, sizeof(timeStr), "%d.%m.%Y %H:%M:%S", &timeinfo);
    client.println("<p><b>Uhrzeit:</b> " + String(timeStr) + "</p>");
  }

  client.println("<div class='row'>");
  for (int i = 0; i < NUM_RELAYS; i++) {
    client.println("<div class='box'>");
    String statusFormatted = relayState[i];
    statusFormatted.setCharAt(0, toupper(statusFormatted.charAt(0))); // erstes Zeichen groß
    client.println("<p><b>Relais " + String(i + 1) + "</b><br>Status: " + statusFormatted + "</p>");
    String url = "/" + String(i + 1) + (relayState[i] == "off" ? "/on" : "/off");
    String label = relayState[i] == "off" ? "Einschalten" : "Ausschalten";
    String btnClass = relayState[i] == "off" ? "button on-button" : "button off-button";
    client.println("<a href='" + url + "'><button class='" + btnClass + "'>" + label + "</button></a>");
    client.println("</div>");
  }
  client.println("</div>");

  client.println("<h3>Zeitschaltuhr</h3>");
  client.println("<form action='/set' method='GET'>");
  client.println("<div class='row'>");
  for (int i = 0; i < NUM_RELAYS; i++) {
    client.println("<div class='box'>");
    client.println("<p><b>Relais " + String(i + 1) + "</b></p>");
    client.println("<p><label style='margin-left:4px;'>Ein:  <input type='time' name='on" + String(i + 1) + "' value='" + onTimes[i] + "' required></p>");
    client.println("<p>Aus: <input type='time' name='off" + String(i + 1) + "' value='" + offTimes[i] + "' required></p>");
    client.println("<input type='hidden' name='timer" + String(i + 1) + "' value='0'>");
    client.println("<p>Timer aktiv: <input type='checkbox' name='timer" + String(i + 1) + "' value='1'" + (timerEnabled[i] ? " checked" : "") + "></p>");
    client.println("</div>");
  }
  client.println("</div>");
  client.println("<input type='submit' class='button blue-button' value='Zeiten setzen'>");
  client.println("</form>");

  client.println("<h3>Lueftersteuerung</h3>");
  client.println("<form action='/set' method='GET'>");
  client.println("<p>Aktuell: <output id='fanVal'>" + String(fanSpeedPercent) + "</output>%</p>");
  client.println("<input type='range' min='0' max='100' value='" + String(fanSpeedPercent) + "' name='fan' oninput='fanVal.value = this.value' onchange='this.form.submit();'>");
  client.println("</form>");

  client.println("</body></html>");
}

void setup() {
  Serial.begin(115200);
  pinMode(statusLedPin, OUTPUT);
  digitalWrite(statusLedPin, LOW);

  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Verbindung fehlgeschlagen. Neustart...");
    delay(5000);
    ESP.restart();
  }

  Serial.println("Verbunden! IP: " + WiFi.localIP().toString());
  digitalWrite(statusLedPin, HIGH);

  for (int i = 0; i < NUM_RELAYS; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], RELAY_ACTIVE_HIGH ? LOW : HIGH);
  }

  ledcSetup(fanChannel, fanFrequency, fanResolution);
  ledcAttachPin(fanPin, fanChannel);

  server.begin();

  configTzTime("CET-1CEST,M3.5.0/2,M10.5.0/3", "pool.ntp.org", "time.nist.gov");

  loadStoredSettings();
  setFanSpeed(fanSpeedPercent);

  ArduinoOTA.setHostname("esp32-control");
  ArduinoOTA.begin();

  if (!MDNS.begin("esp32")) {
    Serial.println("mDNS starten fehlgeschlagen");
  } else {
    Serial.println("mDNS gestartet: http://esp32.local");
  }
}

void loop() {
  ArduinoOTA.handle();

  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 60000) {
    lastCheck = millis();
    checkRelaySchedule();
  }

  WiFiClient client = server.available();
  if (client) {
    header = "";
    String currentLine = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        header += c;

        if (c == '\n' && currentLine.length() == 0) {
          if (header.indexOf("GET /set?") >= 0) {
            int start = header.indexOf("GET ") + 4;
            int end = header.indexOf(" HTTP");
            if (end > start) {
              String urlParams = header.substring(start, end);
              handleTimeInputs(urlParams);
            }
          }

          handleRelayCommands(header);
          generateWebPage(client);
          client.flush();
          delay(1);
          break;
        }

        if (c == '\n') currentLine = "";
        else if (c != '\r') currentLine += c;
      }
    }
    client.stop();
  }
}
