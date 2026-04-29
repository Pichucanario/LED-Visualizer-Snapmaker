/*
  Proyecto: LED Visualizer para Snapmaker U1 v11.57 (firmware Extended)
  Version: 11.57 - MEJORAS ESTÉTICAS WEB + FUNCIONALIDAD V11.56
    - Misma funcionalidad que v11.56 (anti-falsos errores, finished robusto,
      arcoíris rápido, botón PRUEBA, etc.)
    - Web rediseñada con CSS moderno, degradados animados, efecto neón,
      transiciones suaves, sin Google Fonts (tipografía del sistema)
  Autor: Israel Garcia Armas con DeepSeek
*/

#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <WebServer.h>
#include <SPIFFS.h>

#define NUM_LEDS 21
#define DATA_PIN 21

const char* printerIP = "192.168.1.54";   // CAMBIA A LA IP DE TU SNAPMAKER
const int moonrakerPort = 7125;

Adafruit_NeoPixel strip(NUM_LEDS, DATA_PIN, NEO_GRB + NEO_KHZ800);
WebServer server(80);

String currentState = "idle";
float progress = 0.0;
uint8_t globalBrightness = 130;
bool autoMode = true;
String currentFilename = "";
String forcedState = "";
unsigned long lastUpdate = 0;
unsigned long updateInterval = 500;

bool errorTimerActive = false;
unsigned long errorStartTime = 0;
const unsigned long ERROR_DURATION = 15000;

bool potentialError = false;
unsigned long potentialErrorStart = 0;
const unsigned long ERROR_DEBOUNCE_MS = 1000;

float targetBedTemp = 0.0;
float currentBedActual = 0.0;
float currentBedTarget = 0.0;

float extruderTemps[4] = {0,0,0,0};
float extruderTargets[4] = {0,0,0,0};

int currentExtruderIndex = -1;
String currentExtruderName = "";
int lastExtruderIndex = -1;
bool toolChanged = false;

bool toolChangeFlashActive = false;
unsigned long toolChangeFlashEnd = 0;
bool identifyFlashActive = false;
unsigned long identifyFlashEnd = 0;
uint32_t identifyColor = 0;

static float lastProgress = -1.0;
static unsigned long lastProgressChange = 0;
static unsigned long lastForceRefresh = 0;

bool prePrintingActive = false;
unsigned long prePrintingStart = 0;
const unsigned long PRE_PRINTING_DELAY_MS = 3000;

bool heatingDelayActive = false;
unsigned long heatingDelayStart = 0;
const unsigned long HEATING_DELAY_MS = 3000;

bool calibratingActive = false;
unsigned long lastWaveUpdate = 0;
int wavePosition = 0;
int waveDirection = 1;

uint32_t userToolColors[4] = {
  strip.Color(0, 255, 0),
  strip.Color(0, 255, 255),
  strip.Color(255, 200, 0),
  strip.Color(255, 0, 255)
};

String stateBeforePause = "";

const int NUM_STATES = 7;
const char* stateNames[NUM_STATES] = {"heating", "printing", "paused", "finished", "error", "idle", "calibrating"};
const char* stateLabels[NUM_STATES] = {"CALENTANDO", "IMPRIMIENDO", "PAUSA", "FINALIZADO", "ERROR", "REPOSO", "CALIBRACIONES"};

struct EffectParams {
  uint8_t type;
  uint8_t r, g, b;
  uint8_t r2, g2, b2;
  int speed;
  int hueStep;
};

EffectParams currentEffects[NUM_STATES];

const EffectParams defaultEffects[NUM_STATES] = {
  {1, 255, 140, 0,   0,0,0,   2000, 0},
  {0, 0,   255, 0,   0,0,50,  2000, 0},
  {2, 255, 200, 0,   0,0,0,   400,  0},
  {3, 0,   0,   0,   0,0,0,   20,   10},
  {2, 255, 0,   0,   0,0,0,   200,  0},
  {1, 0,   80,  60,  0,0,0,   3000, 0},
  {4, 0,   255, 0,   0,0,255, 80,   0}
};

// ========== DECLARACIONES ==========
void showBootEffect(int stage, bool success);
void finalBlink(bool success);
void loadConfig();
void saveConfig();
void applyEffectForState(String state, float prog);
void showLEDEffect();
bool updatePrinterStatusWithRetry(int maxRetries=2);
void setupWebServer();

// ========== CONFIGURACIÓN ==========
void loadConfig() {
  if (!SPIFFS.begin(true)) {
    for (int i = 0; i < NUM_STATES; i++) currentEffects[i] = defaultEffects[i];
    userToolColors[0] = strip.Color(0,255,0);
    userToolColors[1] = strip.Color(0,255,255);
    userToolColors[2] = strip.Color(255,200,0);
    userToolColors[3] = strip.Color(255,0,255);
    return;
  }
  File f = SPIFFS.open("/config.json", "r");
  if (!f) {
    for (int i = 0; i < NUM_STATES; i++) currentEffects[i] = defaultEffects[i];
    userToolColors[0] = strip.Color(0,255,0);
    userToolColors[1] = strip.Color(0,255,255);
    userToolColors[2] = strip.Color(255,200,0);
    userToolColors[3] = strip.Color(255,0,255);
    return;
  }
  DynamicJsonDocument doc(8192);
  deserializeJson(doc, f);
  f.close();
  for (int i = 0; i < NUM_STATES; i++) {
    String key = String(i);
    currentEffects[i].type   = doc[key]["type"]   | defaultEffects[i].type;
    currentEffects[i].r      = doc[key]["r"]      | defaultEffects[i].r;
    currentEffects[i].g      = doc[key]["g"]      | defaultEffects[i].g;
    currentEffects[i].b      = doc[key]["b"]      | defaultEffects[i].b;
    currentEffects[i].r2     = doc[key]["r2"]     | defaultEffects[i].r2;
    currentEffects[i].g2     = doc[key]["g2"]     | defaultEffects[i].g2;
    currentEffects[i].b2     = doc[key]["b2"]     | defaultEffects[i].b2;
    currentEffects[i].speed  = doc[key]["speed"]  | defaultEffects[i].speed;
    currentEffects[i].hueStep = doc[key]["hueStep"] | defaultEffects[i].hueStep;
  }
  if (doc.containsKey("toolColors")) {
    for (int i = 0; i < 4; i++) {
      String tc = "t" + String(i);
      if (doc["toolColors"].containsKey(tc)) {
        long rgb = doc["toolColors"][tc].as<long>();
        userToolColors[i] = rgb;
      } else {
        if (i==0) userToolColors[i] = strip.Color(0,255,0);
        else if (i==1) userToolColors[i] = strip.Color(0,255,255);
        else if (i==2) userToolColors[i] = strip.Color(255,200,0);
        else userToolColors[i] = strip.Color(255,0,255);
      }
    }
  } else {
    userToolColors[0] = strip.Color(0,255,0);
    userToolColors[1] = strip.Color(0,255,255);
    userToolColors[2] = strip.Color(255,200,0);
    userToolColors[3] = strip.Color(255,0,255);
  }
  currentEffects[2].r = 255;
  currentEffects[2].g = 200;
  currentEffects[2].b = 0;
}

void saveConfig() {
  DynamicJsonDocument doc(8192);
  for (int i = 0; i < NUM_STATES; i++) {
    String key = String(i);
    doc[key]["type"]   = currentEffects[i].type;
    doc[key]["r"]      = currentEffects[i].r;
    doc[key]["g"]      = currentEffects[i].g;
    doc[key]["b"]      = currentEffects[i].b;
    doc[key]["r2"]     = currentEffects[i].r2;
    doc[key]["g2"]     = currentEffects[i].g2;
    doc[key]["b2"]     = currentEffects[i].b2;
    doc[key]["speed"]  = currentEffects[i].speed;
    doc[key]["hueStep"] = currentEffects[i].hueStep;
  }
  JsonObject toolColorsObj = doc.createNestedObject("toolColors");
  for (int i = 0; i < 4; i++) {
    String tc = "t" + String(i);
    toolColorsObj[tc] = (long)userToolColors[i];
  }
  File f = SPIFFS.open("/config.json", "w");
  if (f) { serializeJson(doc, f); f.close(); }
}

void applyWaveEffect(uint32_t color1, uint32_t color2, int speedMs) {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate >= (unsigned long)speedMs) {
    lastUpdate = millis();
    wavePosition += waveDirection;
    if (wavePosition >= NUM_LEDS) {
      wavePosition = NUM_LEDS - 1;
      waveDirection = -1;
    } else if (wavePosition < 0) {
      wavePosition = 0;
      waveDirection = 1;
    }
  }
  for (int i = 0; i < NUM_LEDS; i++) {
    int distance = abs(i - wavePosition);
    float factor = 1.0 - (float)distance / (NUM_LEDS / 2);
    if (factor < 0) factor = 0;
    uint8_t r1 = (color1 >> 16) & 0xFF, g1 = (color1 >> 8) & 0xFF, b1 = color1 & 0xFF;
    uint8_t r2 = (color2 >> 16) & 0xFF, g2 = (color2 >> 8) & 0xFF, b2 = color2 & 0xFF;
    uint8_t r = r1 * (1-factor) + r2 * factor;
    uint8_t g = g1 * (1-factor) + g2 * factor;
    uint8_t b = b1 * (1-factor) + b2 * factor;
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
}

void applyEffectForState(String state, float prog) {
  if (errorTimerActive && state != "finished") state = "error";
  int idx = -1;
  for (int i = 0; i < NUM_STATES; i++) if (state == stateNames[i]) idx = i;
  if (idx < 0) return;
  EffectParams e = currentEffects[idx];

  if (idx == 1) { // printing
    int ledsLit = (prog / 100.0) * NUM_LEDS;
    float breath = (sin(millis() * 2 * PI / e.speed) + 1) / 2;
    uint32_t baseColor;
    if (currentExtruderIndex >= 0 && currentExtruderIndex <= 3) {
      baseColor = userToolColors[currentExtruderIndex];
    } else {
      baseColor = strip.Color(0, 255, 0);
    }
    uint8_t r = (baseColor >> 16) & 0xFF;
    uint8_t g = (baseColor >> 8) & 0xFF;
    uint8_t b = baseColor & 0xFF;
    uint8_t rBar = r * (0.5 + breath * 0.5);
    uint8_t gBar = g * (0.5 + breath * 0.5);
    uint8_t bBar = b * (0.5 + breath * 0.5);
    for (int i = 0; i < NUM_LEDS; i++) {
      if (i < ledsLit) strip.setPixelColor(i, strip.Color(rBar, gBar, bBar));
      else strip.setPixelColor(i, strip.Color(e.r2, e.g2, e.b2));
    }
    strip.show();
    return;
  }

  if (idx == 6) { // calibrating
    applyWaveEffect(strip.Color(e.r, e.g, e.b), strip.Color(e.r2, e.g2, e.b2), e.speed);
    return;
  }

  if (e.type == 0) {
    for (int i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, strip.Color(e.r, e.g, e.b));
  } else if (e.type == 1) {
    float intensity = (sin(millis() * 2 * PI / e.speed) + 1) / 2;
    for (int i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, strip.Color(e.r * intensity, e.g * intensity, e.b * intensity));
  } else if (e.type == 2) {
    bool on = (millis() % (e.speed * 2)) < e.speed;
    for (int i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, on ? strip.Color(e.r, e.g, e.b) : 0);
  } else if (e.type == 3 && idx == 3) {
    static unsigned long lastUpdate = 0;
    static int hueOffset = 0;
    if (millis() - lastUpdate >= (unsigned long)e.speed) {
      lastUpdate = millis();
      hueOffset = (hueOffset + e.hueStep) % 360;
    }
    for (int i = 0; i < NUM_LEDS; i++) {
      int hue = (hueOffset + i * 360 / NUM_LEDS) % 360;
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(hue * 182)));
    }
  }
  strip.show();
}

void showLEDEffect() {
  if (identifyFlashActive) {
    static unsigned long lastBlink = 0;
    if (millis() - lastBlink > 200) {
      lastBlink = millis();
      static bool on = false;
      on = !on;
      for (int i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, on ? identifyColor : 0);
      strip.show();
    }
    if (millis() > identifyFlashEnd) identifyFlashActive = false;
    return;
  }
  if (toolChangeFlashActive) {
    if (millis() < toolChangeFlashEnd) {
      uint32_t color = (currentExtruderIndex >=0 && currentExtruderIndex <=3) ? userToolColors[currentExtruderIndex] : strip.Color(255,255,255);
      for (int i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, color);
      strip.show();
      return;
    } else {
      toolChangeFlashActive = false;
    }
  }
  if (!autoMode && forcedState != "") {
    applyEffectForState(forcedState, progress);
  } else {
    applyEffectForState(currentState, progress);
  }
  strip.setBrightness(globalBrightness);
}

bool updatePrinterStatusWithRetry(int maxRetries) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi desconectado, intentando reconectar...");
    WiFi.reconnect();
    delay(500);
    return false;
  }
  for (int attempt = 0; attempt <= maxRetries; attempt++) {
    HTTPClient http;
    String url = "http://" + String(printerIP) + ":" + String(moonrakerPort) + 
                 "/printer/objects/query?print_stats&display_status&heater_bed&virtual_sdcard&toolhead&extruder&extruder1&extruder2&extruder3";
    http.begin(url);
    http.setTimeout(4000);
    int httpCode = http.GET();
    if (httpCode == 200) {
      String payload = http.getString();
      http.end();
      DynamicJsonDocument doc(16384);
      deserializeJson(doc, payload);
      String printState = doc["result"]["status"]["print_stats"]["state"] | "standby";
      currentFilename = doc["result"]["status"]["print_stats"]["filename"] | "";
      const char* extruderObjNames[] = {"extruder", "extruder1", "extruder2", "extruder3"};
      for (int i = 0; i < 4; i++) {
        if (doc["result"]["status"].containsKey(extruderObjNames[i])) {
          extruderTemps[i] = doc["result"]["status"][extruderObjNames[i]]["temperature"] | 0.0;
          extruderTargets[i] = doc["result"]["status"][extruderObjNames[i]]["target"] | 0.0;
        } else {
          extruderTemps[i] = 0.0;
          extruderTargets[i] = 0.0;
        }
      }
      int newExtruderIndex = -1;
      if (doc["result"]["status"].containsKey("toolhead") && doc["result"]["status"]["toolhead"].containsKey("extruder")) {
        String activeExtruder = doc["result"]["status"]["toolhead"]["extruder"].as<String>();
        if (activeExtruder == "extruder") newExtruderIndex = 0;
        else if (activeExtruder == "extruder1") newExtruderIndex = 1;
        else if (activeExtruder == "extruder2") newExtruderIndex = 2;
        else if (activeExtruder == "extruder3") newExtruderIndex = 3;
      }
      if (newExtruderIndex == -1) {
        for (int i = 0; i < 4; i++) {
          if (extruderTargets[i] > 50.0) { newExtruderIndex = i; break; }
        }
        if (newExtruderIndex == -1) {
          float maxTemp = -1;
          for (int i = 0; i < 4; i++) {
            if (extruderTemps[i] > maxTemp) { maxTemp = extruderTemps[i]; newExtruderIndex = i; }
          }
        }
      }
      if (newExtruderIndex != currentExtruderIndex && newExtruderIndex != -1) {
        currentExtruderIndex = newExtruderIndex;
        toolChanged = true;
        toolChangeFlashActive = true;
        toolChangeFlashEnd = millis() + 500;
        showLEDEffect();
      } else {
        toolChanged = false;
      }
      if (currentExtruderIndex == 0) currentExtruderName = "T0 (Extrusor 1)";
      else if (currentExtruderIndex == 1) currentExtruderName = "T1 (Extrusor 2)";
      else if (currentExtruderIndex == 2) currentExtruderName = "T2 (Extrusor 3)";
      else if (currentExtruderIndex == 3) currentExtruderName = "T3 (Extrusor 4)";
      else currentExtruderName = "T?";
      currentBedActual = doc["result"]["status"]["heater_bed"]["temperature"] | 0.0;
      currentBedTarget = doc["result"]["status"]["heater_bed"]["target"] | 0.0;
      float progress_primary = doc["result"]["status"]["print_stats"]["progress"] | -1.0;
      float progress_secondary = doc["result"]["status"]["display_status"]["progress"] | -1.0;
      float progress_manual = -1.0;
      float print_duration = doc["result"]["status"]["print_stats"]["print_duration"] | 0.0;
      float total_duration = doc["result"]["status"]["print_stats"]["total_duration"] | 0.0;
      if (total_duration > 0.0 && print_duration > 0.0) {
        progress_manual = (print_duration / total_duration) * 100.0;
        if (progress_manual > 100.0) progress_manual = 100.0;
      }
      float newProgress = -1.0;
      if (progress_primary >= 0.0 && progress_primary <= 1.0) {
        newProgress = progress_primary * 100.0;
      } else if (progress_secondary >= 0.0 && progress_secondary <= 1.0) {
        newProgress = progress_secondary * 100.0;
      } else if (progress_manual >= 0.0) {
        newProgress = progress_manual;
      } else {
        newProgress = 0.0;
      }
      newProgress = round(newProgress * 10.0) / 10.0;
      if (printState == "printing") {
        if (newProgress == lastProgress) {
          if (millis() - lastProgressChange > 10000 && millis() - lastForceRefresh > 15000) {
            Serial.println("⚠️ Progreso congelado! Forzando resincronización...");
            lastForceRefresh = millis();
            HTTPClient forceHttp;
            forceHttp.begin("http://" + String(printerIP) + ":" + String(moonrakerPort) + "/printer/objects/query?print_stats");
            forceHttp.GET();
            forceHttp.end();
          }
        } else {
          lastProgressChange = millis();
          lastProgress = newProgress;
        }
      }
      // Máquina de estados (idéntica a v11.56)
      if (autoMode && !errorTimerActive) {
        if (printState == "paused") {
          if (currentState != "paused" && currentState != "error" && currentState != "finished" && currentState != "idle") {
            stateBeforePause = currentState;
            currentState = "paused";
            Serial.println("⏸️ Pausa detectada. Guardado estado previo: " + stateBeforePause);
          }
        } else if (printState == "printing" && currentState == "paused") {
          if (stateBeforePause != "") {
            currentState = stateBeforePause;
            stateBeforePause = "";
            Serial.println("▶️ Reanudada. Restaurando estado: " + currentState);
          } else {
            currentState = "printing";
            Serial.println("▶️ Reanudada. Volviendo a IMPRIMIENDO.");
          }
          if (currentState == "heating") {
            heatingDelayActive = true;
            heatingDelayStart = millis();
          }
        }
        if ((currentState == "idle" || currentState == "finished") && !prePrintingActive && !heatingDelayActive && !calibratingActive) {
          if (printState == "printing" || (currentBedTarget > 0 && currentFilename != "" && currentBedTarget > 20.0)) {
            prePrintingActive = true;
            prePrintingStart = millis();
            currentState = "preprinting";
            progress = 0.0;
            Serial.println("⏳ Pre-impresión: mostrando printing 0% durante 3 segundos...");
          }
        }
        if (prePrintingActive && (millis() - prePrintingStart >= PRE_PRINTING_DELAY_MS)) {
          prePrintingActive = false;
          heatingDelayActive = true;
          heatingDelayStart = millis();
          currentState = "printing";
          progress = 0.0;
          Serial.println("🔥 Fin preprinting. Iniciando retardo de 3s antes de calentar...");
        }
        if (heatingDelayActive && !prePrintingActive) {
          if (millis() - heatingDelayStart >= HEATING_DELAY_MS) {
            heatingDelayActive = false;
            currentState = "heating";
            targetBedTemp = currentBedTarget;
            Serial.println("🔥 Calentando cama...");
          } else {
            if (currentState != "printing") currentState = "printing";
            progress = 0.0;
          }
        }
        if (currentState == "heating") {
          if (currentBedActual >= targetBedTemp - 1.0) {
            calibratingActive = true;
            currentState = "calibrating";
            wavePosition = 0;
            waveDirection = 1;
            Serial.println("⚙️ Calibraciones (wave) hasta que comience la impresión...");
          }
        }
        if (currentState == "calibrating") {
          if (newProgress > 0.0 && printState == "printing") {
            calibratingActive = false;
            currentState = "printing";
            Serial.println("🖨️ Impresión en curso (progreso > 0).");
          }
        }
        if (currentState == "printing") {
          if (printState == "complete") {
            currentState = "finished";
            progress = 100.0;
            prePrintingActive = false;
            heatingDelayActive = false;
            calibratingActive = false;
            stateBeforePause = "";
            Serial.println("🏁 Impresión completada. Estado FINALIZADO.");
          } else if (printState == "paused") {
            if (currentState != "paused") {
              stateBeforePause = currentState;
              currentState = "paused";
            }
          } else {
            progress = newProgress;
          }
        }
        if (currentState == "finished") {
          if (printState != "complete" && currentFilename.isEmpty()) {
            currentState = "idle";
            progress = 0.0;
            Serial.println("🔄 Trabajo completado en impresora. Volviendo a REPOSO.");
          }
        }
        if (currentState != "finished") {
          if (printState == "standby" || printState == "error" || printState == "cancelled") {
            if (!potentialError && !errorTimerActive && (currentState == "printing" || currentState == "heating" || currentState == "paused" || currentState == "calibrating")) {
              potentialError = true;
              potentialErrorStart = millis();
              Serial.println("⚠️ Posible error detectado, esperando " + String(ERROR_DEBOUNCE_MS) + "ms para confirmar...");
            }
          } else {
            potentialError = false;
          }
          if (potentialError && (millis() - potentialErrorStart >= ERROR_DEBOUNCE_MS)) {
            errorTimerActive = true;
            errorStartTime = millis();
            prePrintingActive = false;
            heatingDelayActive = false;
            calibratingActive = false;
            stateBeforePause = "";
            Serial.println("⚠️ Error confirmado. Estado ERROR 15s.");
            currentState = "error";
            potentialError = false;
          }
        }
      }
      if (errorTimerActive && (millis() - errorStartTime >= ERROR_DURATION)) {
        errorTimerActive = false;
        currentState = "idle";
        Serial.println("✅ Fin error.");
      }
      if (currentState == "printing") {
        if (!heatingDelayActive && !prePrintingActive) progress = newProgress;
        else progress = 0.0;
      } else if (currentState == "finished") {
        progress = 100.0;
      }
      static unsigned long lastDebug = 0;
      if (millis() - lastDebug >= 5000) {
        Serial.print("Estado: "); Serial.print(currentState);
        Serial.print("  Progreso: "); Serial.print(progress);
        Serial.print("  Herramienta: "); Serial.println(currentExtruderName);
        lastDebug = millis();
      }
      return true;
    } else {
      http.end();
      if (attempt < maxRetries) {
        Serial.println("⚠️ Error HTTP " + String(httpCode) + ", reintentando en 500ms...");
        delay(500);
      } else {
        Serial.println("❌ Error HTTP persistente: " + String(httpCode));
        if (currentState != "finished") {
          if (!potentialError && !errorTimerActive && (currentState == "printing" || currentState == "heating" || currentState == "paused" || currentState == "calibrating")) {
            potentialError = true;
            potentialErrorStart = millis();
          }
        }
        return false;
      }
    }
  }
  return false;
}

void showBootEffect(int stage, bool success) {
  const uint32_t LIGHT_BLUE = strip.Color(0, 100, 255);
  const uint32_t YELLOW = strip.Color(255, 200, 0);
  const uint32_t MAGENTA = strip.Color(255, 0, 255);
  const uint32_t GREEN = strip.Color(0, 255, 0);
  const uint32_t RED = strip.Color(255, 0, 0);
  const int snakeLength = 10;
  const int centerLed = NUM_LEDS / 2;
  for (int step = 0; step <= centerLed + snakeLength; step++) {
    strip.clear();
    for (int i = 0; i < step && i < NUM_LEDS; i++) {
      int brightness = 255 - (step - i) * 25;
      if (brightness < 50) brightness = 50;
      strip.setPixelColor(i, strip.Color(0, 0, brightness));
    }
    for (int i = 0; i < step && (NUM_LEDS - 1 - i) >= 0; i++) {
      int led = NUM_LEDS - 1 - i;
      int brightness = 255 - (step - i) * 25;
      if (brightness < 50) brightness = 50;
      strip.setPixelColor(led, strip.Color(0, 0, brightness));
    }
    strip.show();
    delay(15);
  }
  uint32_t stageColor;
  if (!success) stageColor = RED;
  else {
    switch(stage) {
      case 1: stageColor = LIGHT_BLUE; break;
      case 2: stageColor = YELLOW; break;
      case 3: stageColor = MAGENTA; break;
      default: stageColor = GREEN;
    }
  }
  for (int i = 0; i < 6; i++) {
    strip.clear();
    if (i % 2 == 0) strip.setPixelColor(centerLed, stageColor);
    strip.show();
    delay(200);
  }
  strip.clear();
  strip.show();
  delay(300);
}

void finalBlink(bool success) {
  const uint32_t GREEN = strip.Color(0, 255, 0);
  const uint32_t RED = strip.Color(255, 0, 0);
  uint32_t color = success ? GREEN : RED;
  for (int i = 0; i < 6; i++) {
    for (int p = 0; p < NUM_LEDS; p++) strip.setPixelColor(p, color);
    strip.show(); delay(150);
    for (int p = 0; p < NUM_LEDS; p++) strip.setPixelColor(p, 0);
    strip.show(); delay(150);
  }
}

// ========== PÁGINA WEB CON ESTILOS MEJORADOS (sin Google Fonts) ==========
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=yes">
  <title>Snapmaker U1 | LED Visualizer</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      background: radial-gradient(circle at 20% 30%, #0a0f2a, #03050b);
      font-family: system-ui, -apple-system, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;
      padding: 20px;
      display: flex;
      justify-content: center;
      align-items: center;
      min-height: 100vh;
      animation: bgShift 12s ease infinite;
    }
    @keyframes bgShift {
      0% { background-position: 0% 0%; }
      50% { background-position: 100% 100%; }
      100% { background-position: 0% 0%; }
    }
    .glass-card {
      backdrop-filter: blur(12px);
      background: rgba(15, 25, 45, 0.65);
      border-radius: 56px;
      padding: 28px 24px;
      width: 100%;
      max-width: 600px;
      border: 1px solid rgba(0, 255, 255, 0.3);
      box-shadow: 0 20px 40px rgba(0,0,0,0.4), 0 0 0 1px rgba(0,255,255,0.1), 0 0 20px rgba(0,255,255,0.1);
      transition: box-shadow 0.4s ease;
      text-align: center;
    }
    .glass-card:hover {
      box-shadow: 0 25px 45px rgba(0,0,0,0.5), 0 0 0 1px cyan, 0 0 30px rgba(0,255,255,0.3);
    }
    .title-wrapper { text-align: center; margin-bottom: 5px; }
    h1 {
      font-size: 2rem;
      font-weight: 700;
      background: linear-gradient(135deg, #FFFFFF, #00FFFF, #2a9eff);
      -webkit-background-clip: text;
      background-clip: text;
      color: transparent;
      letter-spacing: -0.5px;
      display: inline-block;
      margin: 0;
    }
    .beta {
      font-size: 0.7rem;
      background: rgba(255,170,0,0.25);
      padding: 2px 10px;
      border-radius: 30px;
      margin-left: 8px;
      color: #ffaa00;
      font-weight: 500;
      display: inline-block;
      backdrop-filter: blur(2px);
    }
    .credit {
      font-size: 0.7rem;
      color: #a0c0ff;
      margin-bottom: 20px;
      opacity: 0.9;
      text-align: center;
    }
    .status-card {
      background: rgba(0,0,0,0.45);
      border-radius: 40px;
      padding: 20px;
      margin-bottom: 28px;
      border: 1px solid rgba(0,255,255,0.25);
      backdrop-filter: blur(4px);
      text-align: center;
    }
    .status-label {
      font-size: 0.7rem;
      letter-spacing: 2px;
      color: #7fc9ff;
      text-transform: uppercase;
    }
    .status-value {
      font-size: 2rem;
      font-weight: 700;
      color: #0ff;
      text-shadow: 0 0 10px #0ff5;
      animation: softGlow 2s ease-in-out infinite;
      word-break: break-word;
    }
    @keyframes softGlow {
      0% { text-shadow: 0 0 5px #0ff5; }
      50% { text-shadow: 0 0 15px #0ffa; }
      100% { text-shadow: 0 0 5px #0ff5; }
    }
    .progress-bar-bg {
      background: #1e2a3e;
      border-radius: 60px;
      height: 18px;
      margin-top: 12px;
      overflow: hidden;
      box-shadow: inset 0 1px 4px rgba(0,0,0,0.5);
    }
    .progress-fill {
      width: 0%;
      height: 100%;
      background: linear-gradient(90deg, #2effb0, #ffcc33);
      border-radius: 60px;
      transition: width 0.3s cubic-bezier(0.2, 0.9, 0.4, 1.1);
      box-shadow: 0 0 6px rgba(46,255,176,0.5);
    }
    .progress-text {
      font-size: 0.8rem;
      text-align: right;
      color: #ffffff;
      font-weight: 500;
      margin-top: 6px;
    }
    .filename {
      font-size: 0.65rem;
      color: #aac8ff;
      word-break: break-all;
      margin-top: 8px;
      text-align: center;
      font-style: italic;
    }
    .temp-row {
      display: flex;
      justify-content: center;
      gap: 30px;
      margin: 20px 0;
      background: rgba(0,0,0,0.3);
      border-radius: 40px;
      padding: 12px 16px;
      backdrop-filter: blur(4px);
      color: #ffffff;
    }
    .temp-item {
      text-align: center;
      font-size: 1rem;
      font-weight: 600;
      color: #ffffff;
    }
    .temp-label { margin-right: 6px; opacity: 0.8; }
    .all-tools-panel {
      margin: 15px 0;
      background: rgba(0,0,0,0.25);
      border-radius: 30px;
      padding: 12px;
      font-size: 0.8rem;
      color: #ffffff;
    }
    .tool-row {
      display: flex;
      justify-content: space-around;
      gap: 8px;
      flex-wrap: wrap;
    }
    .tool-card {
      background: rgba(0,0,0,0.45);
      border-radius: 25px;
      padding: 8px 12px;
      min-width: 105px;
      text-align: center;
      color: #ffffff;
      backdrop-filter: blur(2px);
      border: 1px solid rgba(0,255,255,0.2);
      transition: all 0.2s;
    }
    .tool-active {
      background: rgba(0,255,0,0.15);
      border: 1px solid #0f0;
      box-shadow: 0 0 8px #0f0;
    }
    .tool-temp { font-weight: bold; font-size: 1rem; }
    .slider-container {
      margin: 24px 0 20px;
      text-align: left;
    }
    .slider-container label {
      display: flex;
      justify-content: space-between;
      color: #e0f0ff;
      font-size: 0.8rem;
      margin-bottom: 6px;
    }
    input[type="range"] {
      width: 100%;
      height: 5px;
      background: linear-gradient(90deg, #0ff, #6a5acd);
      border-radius: 5px;
      -webkit-appearance: none;
    }
    input[type="range"]::-webkit-slider-thumb {
      -webkit-appearance: none;
      width: 20px;
      height: 20px;
      background: white;
      border-radius: 50%;
      border: 2px solid #0ff;
      box-shadow: 0 0 6px cyan;
      cursor: pointer;
    }
    .button-row {
      display: flex;
      flex-wrap: wrap;
      gap: 10px;
      justify-content: center;
      margin: 20px 0;
    }
    button {
      background: linear-gradient(145deg, #1f3b6c, #0b1a2f);
      border: none;
      padding: 8px 16px;
      border-radius: 60px;
      color: white;
      font-weight: 600;
      font-size: 0.75rem;
      cursor: pointer;
      transition: all 0.2s ease;
      box-shadow: 0 2px 6px rgba(0,0,0,0.3);
      border: 1px solid rgba(0,255,255,0.3);
    }
    button:hover {
      background: linear-gradient(145deg, #2a4a8a, #102540);
      transform: translateY(-2px);
      box-shadow: 0 5px 12px rgba(0,0,0,0.4);
    }
    button:active { transform: scale(0.96); }
    .btn-auto { background: linear-gradient(145deg, #2c3e66, #0f1a2e); border-color: #0ff; }
    .btn-identify { background: #ff8c00; margin-left: 8px; }
    .config-panel {
      background: rgba(0,0,0,0.4);
      border-radius: 35px;
      padding: 15px;
      margin-top: 20px;
      text-align: left;
      color: #ffffff;
      backdrop-filter: blur(4px);
      border: 1px solid rgba(0,255,255,0.15);
    }
    .config-title {
      color: #0ff;
      margin-bottom: 12px;
      font-weight: 600;
      font-size: 0.9rem;
    }
    select, input {
      width: 100%;
      margin: 6px 0;
      padding: 8px;
      border-radius: 40px;
      border: none;
      background: #1e2a3e;
      color: #ffffff;
      font-family: inherit;
    }
    .row-flex {
      display: flex;
      gap: 10px;
      align-items: center;
    }
    .row-flex span { color: #cccccc; }
    .badge {
      font-size: 0.65rem;
      background: rgba(0,166,196,0.5);
      border-radius: 40px;
      padding: 4px 12px;
      display: inline-block;
      margin-top: 12px;
      color: #ffffff;
      text-shadow: 0 1px 2px rgba(0,0,0,0.5);
      font-weight: 500;
    }
    footer {
      font-size: 0.6rem;
      margin-top: 30px;
      color: #8fafcf;
      text-align: center;
    }
    #toast {
      visibility: hidden;
      min-width: 250px;
      background-color: #111;
      color: #0ff;
      text-align: center;
      border-radius: 40px;
      padding: 10px 16px;
      position: fixed;
      bottom: 30px;
      left: 50%;
      transform: translateX(-50%);
      font-size: 0.8rem;
      z-index: 1000;
      backdrop-filter: blur(8px);
      box-shadow: 0 0 12px rgba(0,255,255,0.6);
      border: 1px solid cyan;
    }
    #toast.show {
      visibility: visible;
      animation: fadein 0.4s, fadeout 0.5s 2.5s;
    }
    @keyframes fadein {
      from {bottom: 0; opacity: 0;}
      to {bottom: 30px; opacity: 1;}
    }
    @keyframes fadeout {
      from {bottom: 30px; opacity: 1;}
      to {bottom: 0; opacity: 0;}
    }
  </style>
</head>
<body>
<div class="glass-card">
  <div class="title-wrapper">
    <h1>SNAPMAKER U1</h1><span class="beta">BETA</span>
  </div>
  <div class="credit">LED Visualizer · Israel García Armas con DeepSeek · v11.57</div>
  <div class="status-card">
    <div class="status-label">ESTADO ACTUAL</div>
    <div class="status-value" id="printerState">---</div>
    <div class="progress-bar-bg"><div class="progress-fill" id="progressFill"></div></div>
    <div class="progress-text" id="progressPercent">0%</div>
    <div class="filename" id="filename"></div>
  </div>
  
  <div class="temp-row">
    <div class="temp-item"><span class="temp-label" id="extruderLabel">Extrusor 🌡️</span> <span id="extruderTemp">0</span>°C / <span id="extruderTarget">0</span>°C</div>
    <div class="temp-item"><span class="temp-label">Cama 🌡️</span> <span id="bedTemp">0</span>°C / <span id="bedTarget">0</span>°C</div>
  </div>
  
  <div class="all-tools-panel">
    <div style="font-size:0.7rem; margin-bottom:5px;">TEMPERATURAS EXTRUSORES</div>
    <div class="tool-row" id="toolsContainer"></div>
  </div>
  
  <div class="slider-container">
    <label>💡 BRILLO GENERAL <span id="brightnessVal">100%</span></label>
    <input type="range" id="brightnessSlider" min="0" max="100" value="100">
  </div>
  
  <div class="button-row">
    <button id="btnHeat" style="background:#ff8c00">🔥 CALENTANDO</button>
    <button id="btnPrint" style="background:#006400">🖨️ IMPRIMIENDO</button>
    <button id="btnPause" style="background:#b8860b">⏸️ PAUSA</button>
    <button id="btnFinished" style="background:#d4af37;color:#000">🏁 FINALIZADO</button>
    <button id="btnError" style="background:#8b0000">⚠️ ERROR</button>
    <button id="btnIdle" style="background:#00008b">💤 REPOSO</button>
    <button id="btnCalibrating" style="background:#2e8b57">⚙️ CALIBRACIONES</button>
    <button id="btnAuto" class="btn-auto">🤖 MODO AUTO</button>
    <button id="btnIdentify" class="btn-identify">🔍 IDENTIFICAR HERRAMIENTA</button>
  </div>

  <div class="badge" id="modeBadge">🔧 Modo automático</div>

  <div class="config-panel">
    <div class="config-title">🎨 COLORES POR HERRAMIENTA</div>
    <div id="toolColorsPanel"></div>
    <button id="saveToolColorsBtn" style="margin-top:10px; background:#2c3e66;">💾 GUARDAR COLORES HERRAMIENTAS</button>
  </div>

  <div class="config-panel" id="configPanel"></div>
  <footer>⚡ Efectos configurables · Polling 500ms · print_stats.progress</footer>
</div>

<div id="toast"></div>

<script>
  const stateNames = ["heating", "printing", "paused", "finished", "error", "idle", "calibrating"];
  const stateLabels = ["CALENTANDO", "IMPRIMIENDO", "PAUSA", "FINALIZADO", "ERROR", "REPOSO", "CALIBRACIONES"];
  
  let currentTool = -1;
  let lastTool = -1;
  let autoMode = true;
  let pruebaTimeout = null;
  
  function showToast(message) {
    const toast = document.getElementById('toast');
    toast.innerText = message;
    toast.className = "show";
    setTimeout(() => { toast.className = toast.className.replace("show", ""); }, 3000);
  }
  
  const stateElem = document.getElementById('printerState');
  const progressFill = document.getElementById('progressFill');
  const progressPercent = document.getElementById('progressPercent');
  const filenameElem = document.getElementById('filename');
  const extruderLabelSpan = document.getElementById('extruderLabel');
  const extruderTempSpan = document.getElementById('extruderTemp');
  const extruderTargetSpan = document.getElementById('extruderTarget');
  const bedTempSpan = document.getElementById('bedTemp');
  const bedTargetSpan = document.getElementById('bedTarget');
  const brightnessSlider = document.getElementById('brightnessSlider');
  const brightnessVal = document.getElementById('brightnessVal');
  const modeBadge = document.getElementById('modeBadge');
  const toolsContainer = document.getElementById('toolsContainer');
  
  function updateToolsUI(temps, targets, activeIndex) {
    toolsContainer.innerHTML = '';
    const toolNames = ["T0 (Extrusor 1)", "T1 (Extrusor 2)", "T2 (Extrusor 3)", "T3 (Extrusor 4)"];
    for(let i=0; i<4; i++) {
      const card = document.createElement('div');
      card.className = 'tool-card';
      if (activeIndex === i) card.classList.add('tool-active');
      card.innerHTML = `<div>${toolNames[i]}</div><div class="tool-temp">${Math.round(temps[i])}°C</div><div style="font-size:0.6rem;">/${Math.round(targets[i])}°C</div>`;
      toolsContainer.appendChild(card);
    }
  }
  
  function updateStatusUI(data) {
    let labels = {heating:"CALENTANDO", printing:"IMPRIMIENDO", paused:"PAUSA", finished:"FINALIZADO", error:"ERROR", idle:"REPOSO", calibrating:"CALIBRACIONES"};
    stateElem.innerText = labels[data.state] || data.state;
    progressFill.style.width = data.progress+'%';
    progressPercent.innerText = Math.round(data.progress)+'%';
    if(data.filename) filenameElem.innerText = data.filename;
    modeBadge.innerText = autoMode ? '🤖 Modo automático' : '🎮 Modo manual';
    
    if(data.extruderName) extruderLabelSpan.innerText = data.extruderName + " 🌡️";
    else extruderLabelSpan.innerText = "Extrusor 🌡️";
    if(data.extruderTemp !== undefined) extruderTempSpan.innerText = Math.round(data.extruderTemp);
    if(data.extruderTarget !== undefined) extruderTargetSpan.innerText = Math.round(data.extruderTarget);
    if(data.bedTemp !== undefined) bedTempSpan.innerText = Math.round(data.bedTemp);
    if(data.bedTarget !== undefined) bedTargetSpan.innerText = Math.round(data.bedTarget);
    
    if(data.allTemps && data.allTargets && data.activeTool !== undefined) {
      updateToolsUI(data.allTemps, data.allTargets, data.activeTool);
      const toolNamesShort = ["T0", "T1", "T2", "T3"];
      if(lastTool !== -1 && lastTool !== data.activeTool && data.activeTool >= 0) {
        showToast(`🔧 Herramienta cambiada a ${toolNamesShort[data.activeTool]} (Extrusor ${data.activeTool+1})`);
      }
      lastTool = data.activeTool;
    } else if(data.allTemps) {
      updateToolsUI(data.allTemps, data.allTargets || [0,0,0,0], -1);
    }
  }
  
  function pollStatus() {
    fetch('/api/status')
      .then(r => r.json())
      .then(data => updateStatusUI(data))
      .catch(e => console.warn('Polling error:', e));
  }
  
  function setupUI() {
    brightnessSlider.oninput = e => {
      let p = e.target.value;
      brightnessVal.innerText = p+'%';
      fetch('/brightness?value='+Math.round(p*2.55));
    };
    document.getElementById('btnHeat').onclick = () => { autoMode=false; fetch('/force?state=heating'); };
    document.getElementById('btnPrint').onclick = () => { autoMode=false; fetch('/force?state=printing'); };
    document.getElementById('btnPause').onclick = () => { autoMode=false; fetch('/force?state=paused'); };
    document.getElementById('btnFinished').onclick = () => { autoMode=false; fetch('/force?state=finished'); };
    document.getElementById('btnError').onclick = () => { autoMode=false; fetch('/force?state=error'); };
    document.getElementById('btnIdle').onclick = () => { autoMode=false; fetch('/force?state=idle'); };
    document.getElementById('btnCalibrating').onclick = () => { autoMode=false; fetch('/force?state=calibrating'); };
    document.getElementById('btnAuto').onclick = () => { autoMode=true; fetch('/auto'); };
    document.getElementById('btnIdentify').onclick = () => { fetch('/identifyTool'); };
  }

  function loadToolColors() {
    fetch('/getToolColors')
      .then(r => r.json())
      .then(data => {
        const panel = document.getElementById('toolColorsPanel');
        panel.innerHTML = '';
        const toolNames = ["T0 (Extrusor 1)", "T1 (Extrusor 2)", "T2 (Extrusor 3)", "T3 (Extrusor 4)"];
        for (let i=0; i<4; i++) {
          const colorHex = data[i];
          const div = document.createElement('div');
          div.style.marginBottom = '12px';
          div.innerHTML = `<label>${toolNames[i]}:</label><input type="color" id="toolColor_${i}" value="${colorHex}" style="width:100%">`;
          panel.appendChild(div);
        }
        for (let i=0; i<4; i++) {
          const picker = document.getElementById(`toolColor_${i}`);
          picker.oninput = () => {
            fetch(`/previewToolColor?tool=${i}&color=${picker.value.substring(1)}`);
          };
        }
      });
  }

  document.getElementById('saveToolColorsBtn').addEventListener('click', () => {
    let colors = [];
    for (let i=0; i<4; i++) {
      const picker = document.getElementById(`toolColor_${i}`);
      colors.push(picker.value.substring(1));
    }
    fetch('/saveToolColors', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ colors: colors })
    }).then(() => {
      showToast('✅ Colores de herramientas guardados');
    });
  });
  
  function previewState(state, wasAutoMode) {
    fetch('/force?state=' + state);
    if (pruebaTimeout) clearTimeout(pruebaTimeout);
    pruebaTimeout = setTimeout(() => {
      if (wasAutoMode) {
        fetch('/auto');
        autoMode = true;
        modeBadge.innerText = '🤖 Modo automático';
        showToast('🔁 Modo automático restaurado');
      } else {
        showToast('🎬 Prueba finalizada, se mantiene modo manual');
      }
      pruebaTimeout = null;
    }, 5000);
  }

  function loadConfig() {
    fetch('/getEffectsConfig').then(r=>r.json()).then(cfg=>{
      const container = document.getElementById('configPanel');
      container.innerHTML = '';
      for(let idx=0; idx<stateNames.length; idx++) {
        let st = stateNames[idx], label = stateLabels[idx], data = cfg[st];
        let div = document.createElement('div');
        div.style.marginBottom = '20px';
        div.style.borderBottom = '1px solid rgba(0,255,255,0.2)';
        div.style.paddingBottom = '10px';
        div.innerHTML = `<div style="color:#ffaa00; margin-bottom:5px;">${label}</div>`;
        if(st == "printing") {
          let barColor = `#${(data.r<16?'0':'')+data.r.toString(16)}${(data.g<16?'0':'')+data.g.toString(16)}${(data.b<16?'0':'')+data.b.toString(16)}`;
          let bgColor = `#${(data.r2<16?'0':'')+data.r2.toString(16)}${(data.g2<16?'0':'')+data.g2.toString(16)}${(data.b2<16?'0':'')+data.b2.toString(16)}`;
          div.innerHTML += `<div>🎨 Color barra:</div><input type="color" id="barColor_${idx}" value="${barColor}">`;
          div.innerHTML += `<div>🎨 Color fondo:</div><input type="color" id="bgColor_${idx}" value="${bgColor}">`;
          div.innerHTML += `<div>⏱️ Velocidad respiración (ms):</div><div class="row-flex"><input type="range" id="speed_${idx}" min="100" max="5000" step="50" value="${data.speed}"><span>${data.speed}</span></div>`;
        } else {
          let tipo = data.type, color = `#${(data.r<16?'0':'')+data.r.toString(16)}${(data.g<16?'0':'')+data.g.toString(16)}${(data.b<16?'0':'')+data.b.toString(16)}`;
          let options = `<option value="0" ${tipo==0?'selected':''}>🎨 Color fijo</option><option value="1" ${tipo==1?'selected':''}>🌀 Respiración</option><option value="2" ${tipo==2?'selected':''}>⚡ Parpadeo</option>`;
          if(st == "finished") options += `<option value="3" ${tipo==3?'selected':''}>🌈 Arcoíris</option>`;
          if(st == "calibrating") options += `<option value="4" ${tipo==4?'selected':''}>🌊 Wave (verde/azul)</option>`;
          div.innerHTML += `<select id="type_${idx}">${options}</select>`;
          div.innerHTML += `<input type="color" id="color_${idx}" value="${color}">`;
          if(st == "finished") {
            div.innerHTML += `<div>🎯 Paso de tono (grados):</div><input type="number" id="hueStep_${idx}" min="1" max="30" step="1" value="${data.hueStep}" style="width:100%">`;
          }
          if(st == "calibrating") {
            let color2 = `#${(data.r2<16?'0':'')+data.r2.toString(16)}${(data.g2<16?'0':'')+data.g2.toString(16)}${(data.b2<16?'0':'')+data.b2.toString(16)}`;
            div.innerHTML += `<div>🎨 Color secundario:</div><input type="color" id="color2_${idx}" value="${color2}">`;
          }
          div.innerHTML += `<div>⏱️ Velocidad (ms):</div><div class="row-flex"><input type="range" id="speed_${idx}" min="10" max="5000" step="10" value="${data.speed}"><span>${data.speed}</span></div>`;
        }
        let btnContainer = document.createElement('div');
        btnContainer.style.display = 'flex';
        btnContainer.style.gap = '8px';
        btnContainer.style.marginTop = '8px';
        
        let saveBtn = document.createElement('button'); saveBtn.innerText = '💾 GUARDAR'; saveBtn.style.background = '#2c3e66'; saveBtn.style.flex = '1';
        saveBtn.onclick = () => { 
          if(st == "printing") { 
            let barColor = document.getElementById(`barColor_${idx}`).value.substring(1), bgColor = document.getElementById(`bgColor_${idx}`).value.substring(1), speed = document.getElementById(`speed_${idx}`).value; 
            fetch(`/saveEffect?state=${st}&color=${barColor}&bgColor=${bgColor}&speed=${speed}`); 
          } else { 
            let type = document.getElementById(`type_${idx}`).value, color = document.getElementById(`color_${idx}`).value.substring(1), speed = document.getElementById(`speed_${idx}`).value;
            let extra = "";
            if(st == "finished") {
              let hueStep = document.getElementById(`hueStep_${idx}`).value;
              extra = `&hueStep=${hueStep}`;
            }
            if(st == "calibrating") {
              let color2 = document.getElementById(`color2_${idx}`).value.substring(1);
              extra = `&color2=${color2}`;
            }
            fetch(`/saveEffect?state=${st}&type=${type}&color=${color}&speed=${speed}${extra}`);
          } 
          setTimeout(()=>loadConfig(), 500);
        };
        let resetBtn = document.createElement('button'); resetBtn.innerText = '↺ RESTAURAR'; resetBtn.style.background = '#ff8c00'; resetBtn.style.flex = '1';
        resetBtn.onclick = () => { fetch(`/resetEffect?state=${st}`).then(()=>loadConfig()); };
        let testBtn = document.createElement('button'); testBtn.innerText = '🎬 PRUEBA'; testBtn.style.background = '#4CAF50'; testBtn.style.flex = '1';
        testBtn.onclick = () => {
          let wasAuto = autoMode;
          previewState(st, wasAuto);
        };
        btnContainer.appendChild(saveBtn);
        btnContainer.appendChild(resetBtn);
        btnContainer.appendChild(testBtn);
        div.appendChild(btnContainer);
        container.appendChild(div);
        
        if(st == "printing") {
          document.getElementById(`barColor_${idx}`).oninput = () => { 
            let barColor = document.getElementById(`barColor_${idx}`).value.substring(1), bgColor = document.getElementById(`bgColor_${idx}`).value.substring(1), speed = document.getElementById(`speed_${idx}`).value; 
            fetch(`/previewEffect?state=${st}&color=${barColor}&bgColor=${bgColor}&speed=${speed}`);
          };
          document.getElementById(`bgColor_${idx}`).oninput = () => { 
            let barColor = document.getElementById(`barColor_${idx}`).value.substring(1), bgColor = document.getElementById(`bgColor_${idx}`).value.substring(1), speed = document.getElementById(`speed_${idx}`).value; 
            fetch(`/previewEffect?state=${st}&color=${barColor}&bgColor=${bgColor}&speed=${speed}`);
          };
          document.getElementById(`speed_${idx}`).oninput = (e) => { 
            document.getElementById(`speed_${idx}`).nextSibling.innerText = e.target.value; 
            let barColor = document.getElementById(`barColor_${idx}`).value.substring(1), bgColor = document.getElementById(`bgColor_${idx}`).value.substring(1), speed = e.target.value; 
            fetch(`/previewEffect?state=${st}&color=${barColor}&bgColor=${bgColor}&speed=${speed}`);
          };
        } else {
          if(document.getElementById(`type_${idx}`)) document.getElementById(`type_${idx}`).onchange = () => { 
            let type = document.getElementById(`type_${idx}`).value, color = document.getElementById(`color_${idx}`).value.substring(1), speed = document.getElementById(`speed_${idx}`).value;
            let extra = "";
            if(st == "finished") {
              let hueStep = document.getElementById(`hueStep_${idx}`).value;
              extra = `&hueStep=${hueStep}`;
            }
            if(st == "calibrating") {
              let color2 = document.getElementById(`color2_${idx}`).value.substring(1);
              extra = `&color2=${color2}`;
            }
            fetch(`/previewEffect?state=${st}&type=${type}&color=${color}&speed=${speed}${extra}`);
          };
          if(document.getElementById(`color_${idx}`)) document.getElementById(`color_${idx}`).oninput = () => { 
            let type = document.getElementById(`type_${idx}`).value, color = document.getElementById(`color_${idx}`).value.substring(1), speed = document.getElementById(`speed_${idx}`).value;
            let extra = "";
            if(st == "finished") {
              let hueStep = document.getElementById(`hueStep_${idx}`).value;
              extra = `&hueStep=${hueStep}`;
            }
            if(st == "calibrating") {
              let color2 = document.getElementById(`color2_${idx}`).value.substring(1);
              extra = `&color2=${color2}`;
            }
            fetch(`/previewEffect?state=${st}&type=${type}&color=${color}&speed=${speed}${extra}`);
          };
          if(st == "finished" && document.getElementById(`hueStep_${idx}`)) {
            document.getElementById(`hueStep_${idx}`).onchange = () => {
              let type = document.getElementById(`type_${idx}`).value, color = document.getElementById(`color_${idx}`).value.substring(1), speed = document.getElementById(`speed_${idx}`).value, hueStep = document.getElementById(`hueStep_${idx}`).value;
              fetch(`/previewEffect?state=${st}&type=${type}&color=${color}&speed=${speed}&hueStep=${hueStep}`);
            };
          }
          if(st == "calibrating" && document.getElementById(`color2_${idx}`)) {
            document.getElementById(`color2_${idx}`).oninput = () => {
              let type = document.getElementById(`type_${idx}`).value, color = document.getElementById(`color_${idx}`).value.substring(1), color2 = document.getElementById(`color2_${idx}`).value.substring(1), speed = document.getElementById(`speed_${idx}`).value;
              fetch(`/previewEffect?state=${st}&type=${type}&color=${color}&color2=${color2}&speed=${speed}`);
            };
          }
          if(document.getElementById(`speed_${idx}`)) document.getElementById(`speed_${idx}`).oninput = (e) => { 
            document.getElementById(`speed_${idx}`).nextSibling.innerText = e.target.value; 
            let type = document.getElementById(`type_${idx}`).value, color = document.getElementById(`color_${idx}`).value.substring(1), speed = e.target.value;
            let extra = "";
            if(st == "finished") {
              let hueStep = document.getElementById(`hueStep_${idx}`).value;
              extra = `&hueStep=${hueStep}`;
            }
            if(st == "calibrating") {
              let color2 = document.getElementById(`color2_${idx}`).value.substring(1);
              extra = `&color2=${color2}`;
            }
            fetch(`/previewEffect?state=${st}&type=${type}&color=${color}&speed=${speed}${extra}`);
          };
        }
      }
    });
  }
  
  pollStatus();
  setInterval(pollStatus, 500);
  setupUI();
  loadConfig();
  loadToolColors();
</script>
</body>
</html>
)rawliteral";

void setupWebServer() {
  server.on("/", []() { server.send(200, "text/html", index_html); });
  server.on("/api/status", []() {
    String stateToShow = (errorTimerActive) ? "error" : currentState;
    if (prePrintingActive || heatingDelayActive) stateToShow = "printing";
    String allTemps = "[";
    String allTargets = "[";
    for (int i = 0; i < 4; i++) {
      allTemps += String(extruderTemps[i]);
      allTargets += String(extruderTargets[i]);
      if (i < 3) { allTemps += ","; allTargets += ","; }
    }
    allTemps += "]";
    allTargets += "]";
    String json = "{\"state\":\"" + stateToShow + "\",\"progress\":" + String(progress) + ",\"filename\":\"" + currentFilename + "\"" +
                  ",\"extruderName\":\"" + currentExtruderName + "\"" +
                  ",\"extruderTemp\":" + String(extruderTemps[currentExtruderIndex >=0 ? currentExtruderIndex : 0]) +
                  ",\"extruderTarget\":" + String(extruderTargets[currentExtruderIndex >=0 ? currentExtruderIndex : 0]) +
                  ",\"bedTemp\":" + String(currentBedActual) +
                  ",\"bedTarget\":" + String(currentBedTarget) +
                  ",\"activeTool\":" + String(currentExtruderIndex) +
                  ",\"allTemps\":" + allTemps +
                  ",\"allTargets\":" + allTargets +
                  ",\"toolChanged\":" + (toolChanged ? "true" : "false") + "}";
    server.send(200, "application/json", json);
    toolChanged = false;
  });
  server.on("/brightness", []() { if (server.hasArg("value")) globalBrightness = server.arg("value").toInt(); strip.setBrightness(globalBrightness); server.send(200, "text/plain", "OK"); });
  server.on("/force", []() { 
    if (server.hasArg("state")) { 
      String state = server.arg("state");
      autoMode = false; 
      forcedState = state; 
      errorTimerActive = false; 
      prePrintingActive = false; 
      heatingDelayActive = false;
      calibratingActive = false;
      stateBeforePause = "";
      if (state == "calibrating") {
        wavePosition = 0;
        waveDirection = 1;
      }
    } 
    server.send(200, "text/plain", "OK"); 
  });
  server.on("/auto", []() { autoMode = true; forcedState = ""; errorTimerActive = false; prePrintingActive = false; heatingDelayActive = false; calibratingActive = false; stateBeforePause = ""; server.send(200, "text/plain", "OK"); });
  server.on("/identifyTool", []() {
    if (currentExtruderIndex >= 0 && currentExtruderIndex <= 3) {
      identifyFlashActive = true;
      identifyFlashEnd = millis() + 1500;
      identifyColor = userToolColors[currentExtruderIndex];
      Serial.println("Identificando herramienta activa");
    }
    server.send(200, "text/plain", "OK");
  });
  
  server.on("/getToolColors", []() {
    DynamicJsonDocument doc(1024);
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < 4; i++) {
      uint32_t col = userToolColors[i];
      char hex[7];
      sprintf(hex, "%02X%02X%02X", (col>>16)&0xFF, (col>>8)&0xFF, col&0xFF);
      arr.add(String(hex));
    }
    String resp;
    serializeJson(arr, resp);
    server.send(200, "application/json", resp);
  });
  server.on("/previewToolColor", []() {
    if (server.hasArg("tool") && server.hasArg("color")) {
      int tool = server.arg("tool").toInt();
      if (tool >=0 && tool <4) {
        long rgb = strtol(server.arg("color").c_str(), NULL, 16);
        uint32_t newColor = strip.Color((rgb>>16)&0xFF, (rgb>>8)&0xFF, rgb&0xFF);
        userToolColors[tool] = newColor;
        if (currentExtruderIndex == tool) {
          showLEDEffect();
        }
      }
    }
    server.send(200, "text/plain", "OK");
  });
  server.on("/saveToolColors", HTTP_POST, []() {
    if (server.hasArg("plain")) {
      DynamicJsonDocument doc(512);
      deserializeJson(doc, server.arg("plain"));
      JsonArray colors = doc["colors"];
      for (int i = 0; i < 4 && i < colors.size(); i++) {
        String hex = colors[i].as<String>();
        long rgb = strtol(hex.c_str(), NULL, 16);
        userToolColors[i] = strip.Color((rgb>>16)&0xFF, (rgb>>8)&0xFF, rgb&0xFF);
      }
      saveConfig();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Bad request");
    }
  });

  server.on("/getEffectsConfig", []() { 
    DynamicJsonDocument doc(4096); 
    for (int i=0; i<NUM_STATES; i++) { 
      JsonObject obj = doc.createNestedObject(stateNames[i]); 
      obj["type"] = currentEffects[i].type; 
      obj["r"] = currentEffects[i].r; 
      obj["g"] = currentEffects[i].g; 
      obj["b"] = currentEffects[i].b; 
      obj["r2"] = currentEffects[i].r2; 
      obj["g2"] = currentEffects[i].g2; 
      obj["b2"] = currentEffects[i].b2; 
      obj["speed"] = currentEffects[i].speed; 
      obj["hueStep"] = currentEffects[i].hueStep; 
    } 
    String resp; 
    serializeJson(doc, resp); 
    server.send(200, "application/json", resp); 
  });
  
  server.on("/previewEffect", []() { 
    if (server.hasArg("state")) { 
      String state = server.arg("state"); 
      int idx = -1; 
      for (int i=0; i<NUM_STATES; i++) if (state == stateNames[i]) idx = i; 
      if (idx >= 0) { 
        EffectParams p = currentEffects[idx]; 
        if (server.hasArg("type")) p.type = server.arg("type").toInt(); 
        if (server.hasArg("color") && server.arg("color").length() == 6) { 
          long c = strtol(server.arg("color").c_str(), NULL, 16); 
          p.r = (c>>16)&0xFF; 
          p.g = (c>>8)&0xFF; 
          p.b = c&0xFF; 
        } 
        if (server.hasArg("color2") && server.arg("color2").length() == 6) { 
          long c = strtol(server.arg("color2").c_str(), NULL, 16); 
          p.r2 = (c>>16)&0xFF; 
          p.g2 = (c>>8)&0xFF; 
          p.b2 = c&0xFF; 
        } 
        if (server.hasArg("bgColor") && server.arg("bgColor").length() == 6) { 
          long c = strtol(server.arg("bgColor").c_str(), NULL, 16); 
          p.r2 = (c>>16)&0xFF; 
          p.g2 = (c>>8)&0xFF; 
          p.b2 = c&0xFF; 
        } 
        if (server.hasArg("speed")) p.speed = server.arg("speed").toInt(); 
        if (server.hasArg("hueStep")) p.hueStep = server.arg("hueStep").toInt(); 
        currentEffects[idx] = p; 
        showLEDEffect(); 
      } 
    } 
    server.send(200, "text/plain", "OK"); 
  });
  
  server.on("/saveEffect", []() { 
    if (server.hasArg("state")) { 
      String state = server.arg("state"); 
      int idx = -1; 
      for (int i=0; i<NUM_STATES; i++) if (state == stateNames[i]) idx = i; 
      if (idx >= 0) { 
        if (server.hasArg("type")) currentEffects[idx].type = server.arg("type").toInt(); 
        if (server.hasArg("color") && server.arg("color").length() == 6) { 
          long c = strtol(server.arg("color").c_str(), NULL, 16); 
          currentEffects[idx].r = (c>>16)&0xFF; 
          currentEffects[idx].g = (c>>8)&0xFF; 
          currentEffects[idx].b = c&0xFF; 
        } 
        if (server.hasArg("color2") && server.arg("color2").length() == 6) { 
          long c = strtol(server.arg("color2").c_str(), NULL, 16); 
          currentEffects[idx].r2 = (c>>16)&0xFF; 
          currentEffects[idx].g2 = (c>>8)&0xFF; 
          currentEffects[idx].b2 = c&0xFF; 
        } 
        if (server.hasArg("bgColor") && server.arg("bgColor").length() == 6) { 
          long c = strtol(server.arg("bgColor").c_str(), NULL, 16); 
          currentEffects[idx].r2 = (c>>16)&0xFF; 
          currentEffects[idx].g2 = (c>>8)&0xFF; 
          currentEffects[idx].b2 = c&0xFF; 
        } 
        if (server.hasArg("speed")) currentEffects[idx].speed = server.arg("speed").toInt(); 
        if (server.hasArg("hueStep")) currentEffects[idx].hueStep = server.arg("hueStep").toInt(); 
        saveConfig(); 
      } 
    } 
    server.send(200, "text/plain", "OK"); 
  });
  
  server.on("/resetEffect", []() { 
    if (server.hasArg("state")) { 
      String state = server.arg("state"); 
      int idx = -1; 
      for (int i=0; i<NUM_STATES; i++) if (state == stateNames[i]) idx = i; 
      if (idx >= 0) { 
        currentEffects[idx] = defaultEffects[idx]; 
        saveConfig(); 
      } 
    } 
    server.send(200, "text/plain", "OK"); 
  });
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== SNAPMAKER U1 LED VISUALIZER v11.57 ===");
  
  strip.begin();
  strip.show();
  strip.setBrightness(globalBrightness);
  
  showBootEffect(1, true);
  WiFiManager wifiManager;
  wifiManager.autoConnect("Snapmaker-Lights");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  
  showBootEffect(2, true);
  HTTPClient testHttp;
  testHttp.begin("http://" + String(printerIP) + ":" + String(moonrakerPort) + "/printer/info");
  bool moonrakerOk = (testHttp.GET() == 200);
  testHttp.end();
  if (!moonrakerOk) {
    Serial.println("Moonraker ERROR");
    showBootEffect(2, false);
    finalBlink(false);
    return;
  }
  Serial.println("Moonraker OK");
  
  showBootEffect(3, true);
  loadConfig();

  setupWebServer();
  server.begin();
  Serial.print("Web: http://");
  Serial.println(WiFi.localIP());
  
  updatePrinterStatusWithRetry(2);
  Serial.println("Sistema listo.\n");
  
  finalBlink(true);
  delay(1000);
}

void loop() {
  server.handleClient();
  if (millis() - lastUpdate >= updateInterval) {
    if (autoMode) updatePrinterStatusWithRetry(2);
    lastUpdate = millis();
  }
  showLEDEffect();
  delay(5);
}