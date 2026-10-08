// Boîte à histoires NFC - version IDE Arduino
// Mode test SANS matériel (ni SD, ni PN532) : laisser la ligne suivante active.
// Version finale (SD + PN532 + MAX98357A) : mettre la ligne en commentaire.
// #define TEST_MODE

// Boîte à histoires NFC - ESP32-S3 N8R2 + PN532 (I2C) + MAX98357A + SD_MMC
// Carte NFC -> dossier MP3 -> playlist. Gestion via interface web.
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Wire.h>
#include <Adafruit_PN532.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <ArduinoJson.hpp>
#include <Audio.h>
#include <vector>
#include <algorithm>
#include <stdarg.h>
#include <Preferences.h>
#include <esp_system.h>
#include <esp_log.h>

SET_LOOP_TASK_STACK_SIZE(16 * 1024);   // pile de loop() portée à 16 Ko (8 Ko par défaut)

// IMPORTANT : ESP32-audioI2S utilise une tâche audio interne pour le décodage/I2S,
// mais audio.loop() doit TOUJOURS être appelée depuis la tâche Arduino loop().
// Ne pas utiliser la présence de setAudioTaskCore() pour décider de supprimer audio.loop().
// Voir la documentation officielle de la bibliothèque.

// Mode test SANS matériel (ni SD, ni PN532) : env PlatformIO "test" (-DTEST_MODE)
// -> stockage en flash (LittleFS), cartes NFC simulées, commandes via le moniteur série
#ifdef TEST_MODE
  #include <LittleFS.h>
  #define FSYS LittleFS
#else
  #include <SPI.h>
  #include <SD.h>
  #define FSYS SD
#endif

// ---------- Config ----------
const char* WIFI_SSID = "TON_SSID";
const char* WIFI_PASS = "TON_MDP";
const char* HOSTNAME  = "boitehistoires";   // http://boitehistoires.local

// MAX98357A (I2S)
#define I2S_BCLK 5
#define I2S_LRC  6
#define I2S_DOUT 7
// microSD en SPI (module : GND, MISO, CLK, MOSI, CS, 3V3)
#define SD_PIN_SCK  39
#define SD_PIN_MOSI 38
#define SD_PIN_MISO 40
#define SD_PIN_CS   41
// PN532 en I2C
#define PN_SDA 8
#define PN_SCL 9
#define PN_IRQ 17
#define PN_RST 18
// 5 boutons (entre la broche et GND) : reculer, avancer, play/pause, volume -, volume +
const uint8_t BTN_PINS[5] = {10, 11, 12, 13, 14};
#define VOL_MAX 21

// ---------- Globals ----------
Audio audio;
Adafruit_PN532 nfc(PN_IRQ, PN_RST);
AsyncWebServer server(80);
#ifndef TEST_MODE
SPIClass sdSPI(HSPI);               // bus SPI dédié à la carte SD
#endif

std::vector<String> playlist;
String curFolder;
int curTrack = -1;
bool playing = false, paused = false;
uint32_t trackStart = 0;
int volume = 10;

JsonDocument cards;                 // { "UID_HEX": "dossier" }
String pairingFolder;               // dossier en attente d'association
uint32_t pairingUntil = 0;

JsonDocument radios;                // [ {"n":"nom","u":"url"} ]
bool radioMode = false;             // true : un flux radio est la source courante
String curRadioUrl;

// partagé tâche NFC -> loop
volatile bool newCard = false;
char cardBuf[16];
char lastUid[16] = "";               // dernière carte lue (affichée dans la page)
// partagé web -> loop
volatile bool cmdPlayFlag = false, cmdStop = false;
volatile int cmdVolume = -1;
volatile int cmdCtl = 0;            // 1 = précédent, 2 = suivant, 3 = play/pause (depuis la page)
String cmdPlayFolder;

// ---------- Interface web ----------
#include "index_html.h"

// ---------- Journal de debug (onglet Debug de la page) ----------
#define LOG_LINES 60
#define LOG_LEN 120
char logBuf[LOG_LINES][LOG_LEN];
volatile uint32_t logId = 0;                 // numéro de la prochaine ligne
portMUX_TYPE logMux = portMUX_INITIALIZER_UNLOCKED;
vprintf_like_t oldVprintf = nullptr;
Preferences prefs;                           // mémorise les plantages (survit aux redémarrages)
uint32_t crashCount = 0;
String lastCrash;
bool sdOk = false;
uint32_t sdMB = 0;

void logAdd(const char* line) {
  portENTER_CRITICAL(&logMux);
  char* dst = logBuf[logId % LOG_LINES];
  strncpy(dst, line, LOG_LEN - 1);
  dst[LOG_LEN - 1] = 0;
  logId = logId + 1;
  portEXIT_CRITICAL(&logMux);
}

void dbg(const String& s) {
  Serial.println(s);
  char line[LOG_LEN];
  snprintf(line, sizeof(line), "%lus  %s", (unsigned long)(millis() / 1000), s.c_str());
  logAdd(line);
}

void dbgf(const char* fmt, ...) {
  char tmp[LOG_LEN];
  va_list ap; va_start(ap, fmt);
  vsnprintf(tmp, sizeof(tmp), fmt, ap);
  va_end(ap);
  dbg(String(tmp));
}

// Capte aussi les messages d'erreur du système (Wire, SD, Wi-Fi...)
int logHook(const char* fmt, va_list args) {
  static char raw[LOG_LEN * 2];      // tampons statiques : la pile des tâches système reste libre
  static char clean[LOG_LEN];
  va_list copy; va_copy(copy, args);
  portENTER_CRITICAL(&logMux);
  vsnprintf(raw, sizeof(raw), fmt, copy);
  size_t j = 0;
  for (size_t i = 0; raw[i] && j < LOG_LEN - 1; i++) {
    if (raw[i] == 0x1B) { while (raw[i] && raw[i] != 'm') i++; continue; }   // couleurs ANSI
    if (raw[i] == '\n' || raw[i] == '\r') continue;
    clean[j++] = raw[i];
  }
  clean[j] = 0;
  if (j) logAdd(clean);
  portEXIT_CRITICAL(&logMux);
  va_end(copy);
  return oldVprintf ? oldVprintf(fmt, args) : 0;
}

const char* resetReasonStr() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:   return "mise sous tension";
    case ESP_RST_EXT:       return "reset externe (bouton EN)";
    case ESP_RST_SW:        return "redemarrage logiciel";
    case ESP_RST_PANIC:     return "PLANTAGE (panic)";
    case ESP_RST_INT_WDT:   return "PLANTAGE (watchdog interruption)";
    case ESP_RST_TASK_WDT:  return "PLANTAGE (watchdog tache)";
    case ESP_RST_WDT:       return "PLANTAGE (watchdog)";
    case ESP_RST_BROWNOUT:  return "BROWNOUT (alimentation trop faible)";
    case ESP_RST_DEEPSLEEP: return "reveil de veille";
    default:                return "inconnue";
  }
}

// ---------- Trace avant plantage (conservée après un redémarrage logiciel) ----------
#define CRUMB_MAGIC 0xB0B0CAFEu
typedef struct {
  uint32_t magic;
  uint32_t heap, minHeap, stkLoop, stkNfc, stkWeb, up;
  char sLoop[24], sWeb[24], sNfc[24];
} Crumb;
RTC_NOINIT_ATTR Crumb crumb;
Crumb prevCrumb;                      // état juste avant le dernier redémarrage
bool havePrev = false;
TaskHandle_t loopHandle = nullptr, nfcHandle = nullptr;

inline void crumbLoop(const char* t) { strncpy(crumb.sLoop, t, 23); }
inline void crumbWeb(const char* t)  { strncpy(crumb.sWeb, t, 23); }
inline void crumbNfc(const char* t)  { strncpy(crumb.sNfc, t, 23); }

void crumbTick() {                    // appelée dans loop() : instantané chaque seconde
  static uint32_t t = 0;
  if (millis() - t < 1000) return;
  t = millis();
  crumb.up = millis() / 1000;
  crumb.heap = ESP.getFreeHeap();
  crumb.minHeap = ESP.getMinFreeHeap();
  if (loopHandle) crumb.stkLoop = uxTaskGetStackHighWaterMark(loopHandle);
  if (nfcHandle) crumb.stkNfc = uxTaskGetStackHighWaterMark(nfcHandle);
  TaskHandle_t w = xTaskGetHandle("async_tcp");
  if (w) crumb.stkWeb = uxTaskGetStackHighWaterMark(w);
}

// ---------- Accès SD protégé (audio / serveur web) ----------
// La page web et la lecture audio tournent sur des tâches différentes : on évite
// qu'elles utilisent la carte SD en même temps (cause probable de plantage).
SemaphoreHandle_t sdMutex = nullptr;
volatile int webSD = 0;             // nombre d'accès SD en cours hors lecture audio

struct SdLock {
  SdLock()  { __atomic_add_fetch(&webSD, 1, __ATOMIC_SEQ_CST); xSemaphoreTakeRecursive(sdMutex, portMAX_DELAY); }
  ~SdLock() { xSemaphoreGiveRecursive(sdMutex); __atomic_sub_fetch(&webSD, 1, __ATOMIC_SEQ_CST); }
};

// Copie de l'état de lecture lisible par la page sans toucher aux variables de la lecture
char statSrc[96] = "";
char statTrack[96] = "";
volatile int statIdx = -1, statCount = 0;

void publishStatus() {
  strncpy(statSrc, curFolder.c_str(), sizeof(statSrc) - 1);
  statSrc[sizeof(statSrc) - 1] = 0;
  if (curTrack >= 0 && curTrack < (int)playlist.size()) strncpy(statTrack, playlist[curTrack].c_str(), sizeof(statTrack) - 1);
  else statTrack[0] = 0;
  statTrack[sizeof(statTrack) - 1] = 0;
  statIdx = curTrack;
  statCount = (int)playlist.size();
}

// ---------- Utilitaires SD ----------
bool okName(const String& s) {
  return s.length() > 0 && s.indexOf('/') < 0 && s.indexOf('\\') < 0 && s != ".." && s != ".";
}

bool isMp3(const String& n) {
  String l = n; l.toLowerCase();
  return l.endsWith(".mp3") && !n.startsWith(".");
}

std::vector<String> listDirs() {
  SdLock lk;
  std::vector<String> v;
  File r = FSYS.open("/");
  if (!r) return v;
  while (true) {
    File f = r.openNextFile();
    if (!f) break;
    String n = f.name();
    if (f.isDirectory() && !n.startsWith(".") && !n.startsWith("System")) v.push_back(n);
    f.close();
  }
  r.close();
  std::sort(v.begin(), v.end());
  return v;
}

std::vector<String> listMp3(const String& folder) {
  SdLock lk;
  std::vector<String> v;
  File r = FSYS.open("/" + folder);
  if (!r) return v;
  while (true) {
    File f = r.openNextFile();
    if (!f) break;
    String n = f.name();
    if (!f.isDirectory() && isMp3(n)) v.push_back(n);
    f.close();
  }
  r.close();
  std::sort(v.begin(), v.end());
  return v;
}

void rmDirRecursive(const String& folder) {
  SdLock lk;
  for (auto& n : listMp3(folder)) FSYS.remove("/" + folder + "/" + n);
  // autres fichiers éventuels
  File r = FSYS.open("/" + folder);
  std::vector<String> others;
  while (r) {
    File f = r.openNextFile();
    if (!f) break;
    others.push_back(String(f.name()));
    f.close();
  }
  if (r) r.close();
  for (auto& n : others) FSYS.remove("/" + folder + "/" + n);
  FSYS.rmdir("/" + folder);
}

// ---------- Cache de la liste des dossiers (évite de relire la SD à chaque rafraîchissement de la page) ----------
struct FolderInfo { String name; std::vector<String> files; };
std::vector<FolderInfo> folderCache;
volatile bool cacheDirty = true;

void refreshCache() {
  folderCache.clear();
  for (auto& f : listDirs()) {
    FolderInfo fi; fi.name = f; fi.files = listMp3(f);
    folderCache.push_back(fi);
  }
  cacheDirty = false;
}

// ---------- Cartes ----------
void loadCards() {
  SdLock lk;
  File f = FSYS.open("/cards.json");
  if (f) { deserializeJson(cards, f); f.close(); }
  if (!cards.is<JsonObject>()) cards.to<JsonObject>();
}

void saveCards() {
  SdLock lk;
  File f = FSYS.open("/cards.json", FILE_WRITE);
  if (f) { serializeJson(cards, f); f.close(); }
}

void unpairFolder(const String& folder) {
  std::vector<String> keys;
  for (JsonPair kv : cards.as<JsonObject>())
    if (kv.value().as<String>() == folder) keys.push_back(String(kv.key().c_str()));
  for (auto& k : keys) cards.remove(k);
}

String uidFor(const String& folder) {
  for (JsonPair kv : cards.as<JsonObject>())
    if (kv.value().as<String>() == folder) return String(kv.key().c_str());
  return "";
}

// ---------- Radios web ----------
void loadRadios() {
  SdLock lk;
  File f = FSYS.open("/radios.json");
  if (f) {
    deserializeJson(radios, f);
    f.close();
  } else {
    // Le fichier n'existe pas encore : on le crée proprement.
    File nf = FSYS.open("/radios.json", FILE_WRITE);
    if (nf) nf.close();
  }
  if (!radios.is<JsonArray>()) radios.to<JsonArray>();
}

void saveRadios() {
  SdLock lk;
  File f = FSYS.open("/radios.json", FILE_WRITE);
  if (f) { serializeJson(radios, f); f.close(); }
}

String radioUrl(const String& name) {
  for (JsonObject o : radios.as<JsonArray>())
    if (o["n"].as<String>() == name) return o["u"].as<String>();
  return "";
}

// ---------- Lecture ----------
void playTrack() {
  if (curTrack < 0 || curTrack >= (int)playlist.size()) return;
  String path = "/" + curFolder + "/" + playlist[curTrack];
  { SdLock lk; audio.connecttoFS(FSYS, path.c_str()); }
  playing = true; paused = false; trackStart = millis();
  dbg("Lecture : " + path);
  publishStatus();
}

void stopPlayback() {
  if (audio.isRunning() || paused) audio.stopSong();   // évite d'arrêter deux fois
  playing = false; paused = false;
}

void startFolder(const String& folder) {
  auto files = listMp3(folder);
  if (files.empty()) { dbg("Dossier vide : " + folder); return; }
  radioMode = false; playlist = files; curFolder = folder; curTrack = 0;
  playTrack();
}

void playRadio(const String& url) {
  audio.connecttohost(url.c_str());
  playing = true; paused = false; trackStart = millis();
  dbg("Radio : " + url);
}

void startRadio(const String& name) {
  String u = radioUrl(name);
  if (u.length() == 0) { dbg("Radio introuvable : " + name); return; }
  radioMode = true; curFolder = "radio:" + name; curRadioUrl = u;
  playlist.clear(); curTrack = -1;
  playRadio(u);
  publishStatus();
}

void nextTrack(int dir) {
  if (radioMode || (!playing && playlist.empty())) return;
  int n = curTrack + dir;
  if (n < 0) n = 0;
  if (n >= (int)playlist.size()) { dbg("Fin de playlist"); stopPlayback(); return; }
  curTrack = n;
  playTrack();
}

void togglePause() {
  if (radioMode) {                    // radio : arrêt / redémarrage du flux
    if (playing) stopPlayback(); else playRadio(curRadioUrl);
    return;
  }
  if (!playing) return;
  audio.pauseResume();
  paused = !paused;
}

// ---------- Tâche NFC ----------
void nfcTask(void*) {
  bool present = false;
  uint32_t lastSeen = 0;
  for (;;) {
    crumbNfc("lecture i2c");
    uint8_t uid[7]; uint8_t len = 0;
    if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &len, 100)) {
      lastSeen = millis();
      if (!present) {          // un seul événement par pose de carte
        present = true;
        for (uint8_t i = 0; i < len; i++) sprintf(cardBuf + i * 2, "%02X", uid[i]);
        newCard = true;
      }
    } else if (present && millis() - lastSeen > 800) {
      present = false;
    }
    crumbNfc("attente");
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

#ifdef TEST_MODE
void simCard(const String& uid) {
  strncpy(cardBuf, uid.c_str(), sizeof(cardBuf) - 1);
  cardBuf[sizeof(cardBuf) - 1] = 0;
  newCard = true;
}

// Moniteur série : n = suivant, p = précédent, t = pause, + / - = volume,
// autre texte = UID de carte simulée (ex : AABBCCDD)
void handleSerial() {
  static String buf;
  while (Serial.available()) {
    char c = Serial.read();
    if (c != '\n' && c != '\r') { buf += c; continue; }
    buf.trim();
    if (buf == "n") nextTrack(+1);
    else if (buf == "p") nextTrack(-1);
    else if (buf == "t") togglePause();
    else if (buf == "+") cmdVolume = min(volume + 1, VOL_MAX);
    else if (buf == "-") cmdVolume = max(volume - 1, 0);
    else if (buf.length()) { buf.toUpperCase(); simCard(buf); }
    buf = "";
  }
}
#endif

void handleNfc() {
  if (!newCard) return;
  String uid = String(cardBuf);
  newCard = false;
  strncpy(lastUid, uid.c_str(), sizeof(lastUid) - 1);
  lastUid[sizeof(lastUid) - 1] = 0;
  dbg("Carte : " + uid);
  if (pairingFolder.length() && millis() < pairingUntil) {
    unpairFolder(pairingFolder);
    cards.remove(uid);
    cards[uid] = pairingFolder;
    saveCards();
    pairingFolder = "";
    return;
  }
  pairingFolder = "";
  String folder = cards[uid].as<String>();
  if (folder.length() == 0 || folder == "null") { dbg("Carte inconnue"); return; }
  if (playing && folder == curFolder) togglePause();
  else if (folder.startsWith("radio:")) startRadio(folder.substring(6));
  else startFolder(folder);
}

// ---------- Entrées physiques ----------
void handleButtons() {
  static uint32_t last[5] = {0};
  for (int i = 0; i < 5; i++) {
    uint32_t gap = (i >= 3) ? 150 : 250;   // volume : répétition si le bouton reste enfoncé
    if (digitalRead(BTN_PINS[i]) == LOW && millis() - last[i] > gap) {
      last[i] = millis();
      switch (i) {
        case 0: nextTrack(-1); break;       // reculer
        case 1: nextTrack(+1); break;       // avancer
        case 2: togglePause(); break;       // play / pause
        case 3: volume = max(volume - 1, 0);       audio.setVolume(volume); break;
        case 4: volume = min(volume + 1, VOL_MAX); audio.setVolume(volume); break;
      }
    }
  }
}

void handleWebCmd() {
  if (cmdCtl) {
    int c = cmdCtl; cmdCtl = 0;
    if (c == 1) nextTrack(-1);
    else if (c == 2) nextTrack(+1);
    else if (c == 3) togglePause();
  }
  if (cmdVolume >= 0) { volume = constrain((int)cmdVolume, 0, VOL_MAX); audio.setVolume(volume); cmdVolume = -1; }
  if (cmdStop) { cmdStop = false; stopPlayback(); }
  if (cmdPlayFlag) {
    cmdPlayFlag = false;
    if (cmdPlayFolder.startsWith("radio:")) startRadio(cmdPlayFolder.substring(6));
    else startFolder(cmdPlayFolder);
  }
}

void handleEnd() {
  if (!playing || paused) return;
  if (radioMode) {                      // flux coupé : on se reconnecte
    if (millis() - trackStart > 8000 && !audio.isRunning()) {
      dbg("Radio : flux coupe, reconnexion");
      playRadio(curRadioUrl);
    }
    return;
  }
  if (millis() - trackStart > 1000 && !audio.isRunning()) {
    dbgf("Fin de piste %d/%d apres %lu s", curTrack + 1, (int)playlist.size(), (unsigned long)((millis() - trackStart) / 1000));
    nextTrack(+1);
  }
}

// ---------- Serveur web ----------
String param(AsyncWebServerRequest* r, const char* n) {
  return r->hasParam(n) ? r->getParam(n)->value() : String();
}

void setupWeb() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* r) { r->send(200, "text/html", INDEX_HTML); });

  server.on("/api/state", HTTP_GET, [](AsyncWebServerRequest* r) {
    crumbWeb("state");
    if (pairingFolder.length() && millis() > pairingUntil) pairingFolder = "";
    JsonDocument d;
    d["pairing"] = pairingFolder;
    d["playing"] = playing ? String(statSrc) : String("");
    JsonArray arr = d["folders"].to<JsonArray>();
    if (cacheDirty) { SdLock lk; refreshCache(); }
    for (auto& fi : folderCache) {
      JsonObject o = arr.add<JsonObject>();
      o["name"] = fi.name;
      o["uid"] = uidFor(fi.name);
      JsonArray fl = o["files"].to<JsonArray>();
      for (auto& n : fi.files) fl.add(n);
    }
    JsonArray ra = d["radios"].to<JsonArray>();
    for (JsonObject o : radios.as<JsonArray>()) {
      String nm = o["n"].as<String>();
      JsonObject x = ra.add<JsonObject>();
      x["name"] = nm; x["url"] = o["u"].as<String>(); x["uid"] = uidFor("radio:" + nm);
    }
    String out; serializeJson(d, out);
    r->send(200, "application/json", out);
  });

  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* r) {
    crumbWeb("status");
    if (pairingFolder.length() && millis() > pairingUntil) pairingFolder = "";
    JsonDocument d;
    d["volume"] = volume;
    d["paused"] = paused;
    d["playing"] = playing ? String(statSrc) : String("");
    d["track"] = playing ? String(statTrack) : String("");
    d["src"] = String(statSrc);
    d["idx"] = statIdx;
    d["count"] = statCount;
    d["last"] = String(lastUid);
    d["pairing"] = pairingFolder;
    String out; serializeJson(d, out);
    r->send(200, "application/json", out);
  });

  server.on("/api/log", HTTP_GET, [](AsyncWebServerRequest* r) {
    crumbWeb("log");
    uint32_t next = logId;
    uint32_t since = (uint32_t)param(r, "since").toInt();
    uint32_t start = next > LOG_LINES ? next - LOG_LINES : 0;
    if (since > next) since = 0;            // l'ESP a redémarré
    if (since < start) since = start;
    JsonDocument d;
    d["next"] = next;
    d["up"] = (uint32_t)(millis() / 1000);
    d["heap"] = ESP.getFreeHeap();
    d["minheap"] = ESP.getMinFreeHeap();
    d["rssi"] = WiFi.RSSI();
    d["ip"] = (WiFi.getMode() == WIFI_AP) ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
    d["reset"] = resetReasonStr();
    d["crashes"] = crashCount;
    d["lastcrash"] = lastCrash;
    d["sd"] = sdOk;
    d["sdmb"] = sdMB;
    d["psram"] = (uint32_t)ESP.getPsramSize();
    d["psfree"] = (uint32_t)ESP.getFreePsram();
    d["stkL"] = crumb.stkLoop; d["stkN"] = crumb.stkNfc; d["stkW"] = crumb.stkWeb;
    if (havePrev) {
      JsonObject pv = d["prev"].to<JsonObject>();
      pv["up"] = prevCrumb.up; pv["heap"] = prevCrumb.heap; pv["minheap"] = prevCrumb.minHeap;
      pv["stkLoop"] = prevCrumb.stkLoop; pv["stkNfc"] = prevCrumb.stkNfc; pv["stkWeb"] = prevCrumb.stkWeb;
      pv["sLoop"] = String(prevCrumb.sLoop); pv["sWeb"] = String(prevCrumb.sWeb); pv["sNfc"] = String(prevCrumb.sNfc);
    }
    JsonArray ln = d["lines"].to<JsonArray>();
    for (uint32_t i = since; i < next; i++) ln.add(String(logBuf[i % LOG_LINES]));
    String out; serializeJson(d, out);
    r->send(200, "application/json", out);
  });

  server.on("/api/clearcrash", HTTP_POST, [](AsyncWebServerRequest* r) {
    crumbWeb("clearcrash");
    crashCount = 0; lastCrash = "";
    prefs.putUInt("crash", 0); prefs.putString("last", "");
    r->send(200, "text/plain", "ok");
  });

  server.on("/api/ctl", HTTP_POST, [](AsyncWebServerRequest* r) {
    crumbWeb("ctl");
    String c = param(r, "c");
    if (c == "prev") cmdCtl = 1;
    else if (c == "next") cmdCtl = 2;
    else if (c == "pp") cmdCtl = 3;
    r->send(200, "text/plain", "ok");
  });

  server.on("/api/assign", HTTP_POST, [](AsyncWebServerRequest* r) {
    crumbWeb("assign");
    String u = param(r, "uid"), f = param(r, "folder");
    u.toUpperCase();
    if (u.length() && okName(f)) {
      unpairFolder(f);
      cards.remove(u);
      cards[u] = f;
      saveCards();
    }
    r->send(200, "text/plain", "ok");
  });

  server.on("/api/volume", HTTP_POST, [](AsyncWebServerRequest* r) {
    crumbWeb("volume");
    String v = param(r, "v");
    if (v.length()) cmdVolume = constrain(v.toInt(), 0, VOL_MAX);
    r->send(200, "text/plain", "ok");
  });

  server.on("/api/radioadd", HTTP_POST, [](AsyncWebServerRequest* r) {
    crumbWeb("radioadd");
    String n = param(r, "name"), u = param(r, "url");
    if (okName(n) && (u.startsWith("http://") || u.startsWith("https://"))) {
      JsonArray a = radios.as<JsonArray>();
      bool found = false;
      for (JsonObject o : a) if (o["n"].as<String>() == n) { o["u"] = u; found = true; }
      if (!found) { JsonObject o = a.add<JsonObject>(); o["n"] = n; o["u"] = u; }
      saveRadios();
    }
    r->send(200, "text/plain", "ok");
  });

  server.on("/api/radiodel", HTTP_POST, [](AsyncWebServerRequest* r) {
    crumbWeb("radiodel");
    String n = param(r, "name");
    if (okName(n)) {
      JsonArray a = radios.as<JsonArray>();
      for (size_t i = 0; i < a.size(); i++)
        if (a[i]["n"].as<String>() == n) { a.remove(i); break; }
      if (String(statSrc) == "radio:" + n) cmdStop = true;
      unpairFolder("radio:" + n); saveCards(); saveRadios();
    }
    r->send(200, "text/plain", "ok");
  });

  server.on("/api/mkdir", HTTP_POST, [](AsyncWebServerRequest* r) {
    crumbWeb("mkdir");
    String f = param(r, "folder");
    if (okName(f) && !f.startsWith("radio:")) { SdLock lk; FSYS.mkdir("/" + f); }
    cacheDirty = true;
    r->send(200, "text/plain", "ok");
  });

  server.on("/api/rmdir", HTTP_POST, [](AsyncWebServerRequest* r) {
    crumbWeb("rmdir");
    String f = param(r, "folder");
    if (okName(f)) {
      if (f == String(statSrc)) cmdStop = true;
      unpairFolder(f); saveCards();
      rmDirRecursive(f);
      cacheDirty = true;
    }
    r->send(200, "text/plain", "ok");
  });

  server.on("/api/rmfile", HTTP_POST, [](AsyncWebServerRequest* r) {
    crumbWeb("rmfile");
    String f = param(r, "folder"), n = param(r, "name");
    if (okName(f) && okName(n)) { SdLock lk; FSYS.remove("/" + f + "/" + n); }
    cacheDirty = true;
    r->send(200, "text/plain", "ok");
  });

  server.on("/api/pair", HTTP_POST, [](AsyncWebServerRequest* r) {
    crumbWeb("pair");
    String f = param(r, "folder");
    if (okName(f)) { pairingFolder = f; pairingUntil = millis() + 30000; }
    r->send(200, "text/plain", "ok");
  });

  server.on("/api/unpair", HTTP_POST, [](AsyncWebServerRequest* r) {
    crumbWeb("unpair");
    String f = param(r, "folder");
    if (okName(f)) { unpairFolder(f); saveCards(); }
    r->send(200, "text/plain", "ok");
  });

  server.on("/api/play", HTTP_POST, [](AsyncWebServerRequest* r) {
    crumbWeb("play");
    String f = param(r, "folder");
    if (okName(f)) { cmdPlayFolder = f; cmdPlayFlag = true; }
    r->send(200, "text/plain", "ok");
  });

  server.on("/api/upload", HTTP_POST,
    [](AsyncWebServerRequest* r) { r->send(200, "text/plain", "ok"); },
    [](AsyncWebServerRequest* r, String filename, size_t index, uint8_t* data, size_t len, bool final) {
      String folder = param(r, "folder");
      if (!okName(folder) || !okName(filename) || !isMp3(filename)) return;
      crumbWeb("upload");
      static File upFile;   // un seul upload à la fois (la page les envoie l'un après l'autre)
      SdLock lk;
      if (index == 0) upFile = FSYS.open("/" + folder + "/" + filename, FILE_WRITE);
      if (upFile) upFile.write(data, len);
      if (final && upFile) { upFile.close(); cacheDirty = true; }
    });

#ifdef TEST_MODE
  // Simuler une carte depuis le navigateur : /api/simcard?uid=AABBCCDD
  server.on("/api/simcard", HTTP_GET, [](AsyncWebServerRequest* r) {
    crumbWeb("simcard");
    String u = param(r, "uid"); u.toUpperCase();
    if (u.length()) simCard(u);
    r->send(200, "text/plain", "carte simulee : " + u);
  });
#endif

  server.begin();
}

// ---------- Setup / loop ----------
void setup() {
  Serial.begin(115200);
  sdMutex = xSemaphoreCreateRecursiveMutex();
  if (crumb.magic == CRUMB_MAGIC && esp_reset_reason() != ESP_RST_POWERON) { prevCrumb = crumb; havePrev = true; }
  memset(&crumb, 0, sizeof(crumb));
  crumb.magic = CRUMB_MAGIC;
  loopHandle = xTaskGetCurrentTaskHandle();
  oldVprintf = esp_log_set_vprintf(logHook);
  prefs.begin("bh", false);
  crashCount = prefs.getUInt("crash", 0);
  lastCrash = prefs.getString("last", "");
  esp_reset_reason_t rr = esp_reset_reason();
  if (rr == ESP_RST_PANIC || rr == ESP_RST_INT_WDT || rr == ESP_RST_TASK_WDT || rr == ESP_RST_WDT || rr == ESP_RST_BROWNOUT) {
    crashCount++; lastCrash = resetReasonStr();
    prefs.putUInt("crash", crashCount); prefs.putString("last", lastCrash);
  }
  dbgf("Demarrage : %s", resetReasonStr());
  for (auto p : BTN_PINS) pinMode(p, INPUT_PULLUP);

#ifdef TEST_MODE
  sdOk = LittleFS.begin(true);
  if (!sdOk) dbg("LittleFS : echec");
  else sdMB = (uint32_t)(LittleFS.totalBytes() / (1024UL * 1024UL));
  dbg("MODE TEST : stockage flash, NFC simule");
#else
  sdSPI.begin(SD_PIN_SCK, SD_PIN_MISO, SD_PIN_MOSI, SD_PIN_CS);
  sdOk = SD.begin(SD_PIN_CS, sdSPI, 8000000);     // 8 MHz (descendre à 4000000 si instable)
  if (!sdOk) dbg("SD : echec (verifier cablage, FAT32)");
  else { dbg("SD OK"); sdMB = (uint32_t)(SD.cardSize() / (1024ULL * 1024ULL)); }
#endif
  loadCards();
  loadRadios();

  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(volume);
  dbg("Audio : audio.loop() sera appelee en permanence depuis loop()");

#ifndef TEST_MODE
  Wire.begin(PN_SDA, PN_SCL);
  nfc.begin();
  uint32_t v = nfc.getFirmwareVersion();
  if (!v) {
    dbg("PN532 : non detecte -> NFC desactive (verifier SDA/SCL, alim, switchs I2C)");
  } else {
    dbgf("PN532 OK, firmware %d.%d", (int)((v >> 16) & 0xFF), (int)((v >> 8) & 0xFF));
    nfc.SAMConfig();
    xTaskCreatePinnedToCore(nfcTask, "nfc", 8192, nullptr, 1, &nfcHandle, 0);
  }

#endif

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) delay(200);
  if (WiFi.status() != WL_CONNECTED) {          // repli : point d'accès
    WiFi.mode(WIFI_AP);
    WiFi.softAP("BoiteHistoires", "histoires");
    dbg("AP : 192.168.4.1");
  } else {
    dbg("IP : " + WiFi.localIP().toString());
  }
  MDNS.begin(HOSTNAME);
  setupWeb();
}

void loop() {
  // IMPORTANT : la bibliothèque ESP32-audioI2S exige que audio.loop()
  // soit appelée depuis la tâche Arduino loop(), même lorsqu'elle utilise
  // sa propre tâche audio pour le décodage/I2S.
  //
  // Pour les fichiers locaux, on protège également l'accès SD avec le même
  // mutex que le serveur Web. Ainsi, une requête Web ne peut pas modifier/lire
  // la FAT pendant qu'audio.loop() manipule le fichier audio.
  crumbLoop("audio");
  if (radioMode) {
    audio.loop();
  } else if (sdOk) {
    xSemaphoreTakeRecursive(sdMutex, portMAX_DELAY);
    audio.loop();
    xSemaphoreGiveRecursive(sdMutex);
  } else {
    audio.loop();
  }

  // Laisse les autres tâches FreeRTOS s'exécuter. La documentation de
  // ESP32-audioI2S demande au minimum un vTaskDelay(1) entre les appels.
  vTaskDelay(1);

  crumbLoop("nfc");     handleNfc();
  crumbLoop("cmd web"); handleWebCmd();
  crumbLoop("boutons"); handleButtons();
  crumbLoop("fin piste"); handleEnd();
  crumbTick();
#ifdef TEST_MODE
  handleSerial();
#endif
}
