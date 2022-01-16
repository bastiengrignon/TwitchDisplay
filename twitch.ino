/*******************************************************************
 *  Get Data from Twitch & show them on LED Matrix
 *  LED Matrix Pin -> ESP8266 Pin
 *  Vcc            -> 3v
 *  Gnd            -> Gnd
 *  DIN            -> D7
 *  CS             -> D4
 *  CLK            -> D5
 *  By Bastien Grignon
 *******************************************************************/

#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

#include "CACert.h"
#include "constants.h"

/******************
 * Time variables *
 ******************/
unsigned long lastTwitchUpdate{0};
unsigned long twitchUpdateInterval{30000}; // Time between requests (30s)
unsigned long lastAccessTokenUpdate{0};
unsigned long accessTokenUpdateInterval{0};
unsigned long lastTextUpdate{0};
unsigned long textUpdateInterval{15000};
bool toggleText{false};

const char *icons[] = { "&", "^" };
String twitchAccessToken{""};

typedef struct
{
    String followersNumber{""};
    String lastFollowerName{""};
    String streamerName{""};
    String viewCount{""};
    String liveViewerCount{""};
} TwitchInformation;

/******************
 *** Instances ****
 ******************/
AsyncWebServer server(SERVER_PORT);
WiFiClientSecure client;
MD_Parola display = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
TwitchInformation twitchInfos;

/******************
 *** Prototypes ***
 ******************/
String httpRequest(const String httpMethod, const String endpoint, const String data = "");

void setup() {
  Serial.begin(115200);
  initDisplay();
  initHTTPClient();
  initWiFi();
  initOTA();
  initWebserverRoutes();
}

void loop() {
  /*****************
   * Handle Twitch *
   *****************/
  twitchAccessToken = fetchTwitchAccessToken();
  if ((millis() - lastTwitchUpdate) > twitchUpdateInterval) {
    getTwitchUserInformation();
    getTwitchLastFollowerInformation();

    lastTwitchUpdate = millis();
  }
  /*****************
   ** Handle OTA ***
   *****************/
  ArduinoOTA.handle();

  static uint8_t intensity{0};
  static int8_t  intensityOffset{1};

  if (display.displayAnimate()) {
    if (display.getZoneStatus(0)) {
      intensity += intensityOffset;
      if (intensity == 15 || intensity == 0) intensityOffset = -intensityOffset;

      display.setIntensity(0, intensity);
      display.displayReset(0);
    }
    if (display.getZoneStatus(1)) {
      toggleText = not toggleText;

      display.setPause(1, toggleText ? 10000 : 0);
      display.setTextEffect(1, toggleText ? Constants::Display::DEFAULT_SCROLL_EFFECT : PA_SCROLL_LEFT, PA_SCROLL_LEFT);
      display.setTextBuffer(1, toggleText ? twitchInfos.followersNumber.c_str() : twitchInfos.lastFollowerName.c_str());
      display.displayReset(1);
    }
  }
}

/******************
 ****** Init ******
 ******************/
const void initDisplay() {
  display.begin(Constants::Display::numberOfZones);
  display.setZone(0, 0, 0);
  display.setZone(1, 1, MAX_DEVICES - 1);
  display.setIntensity(4);
  display.setSpeed(50);
  display.displayClear();
  display.addChar(0, '&', Constants::Display::twitchIcon);
  display.addChar(0, '^', Constants::Display::heart);
  display.displayZoneText(0, icons[0], Constants::Display::DEFAULT_ALIGNEMENT, 25, 50, PA_PRINT);
  display.displayZoneText(1, "", Constants::Display::DEFAULT_ALIGNEMENT, Constants::Display::DEFAULT_SCROLL_SPEED, Constants::Display::DEFAULT_SCROLL_PAUSE, Constants::Display::DEFAULT_SCROLL_EFFECT);
}

const void initWiFi() {
  Serial.println("Connecting to '" + Constants::Wifi::SSID + "'");
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(Constants::Wifi::SSID, Constants::Wifi::PASSWORD);

  while (not WiFi.isConnected()) {
    Serial.print(".");
    delay(100);
  }
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

const void initHTTPClient() {
    const uint16_t httpsPort{443};
    client.setInsecure();
    client.connect(Constants::Twitch::API, httpsPort);
    setCertificate(); // Seems to be useless
}

//Set Twitch.tv Root Certificate
const void setCertificate() {
  Serial.println("Set Secure Certificate");
  bool res{client.setCACert_P(caCert, caCertLen)};
  if (not res) {
    Serial.println("Failed to load root CA certificate!");
  }
}

const void initOTA() {
  ArduinoOTA.setHostname(OTA_NAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    display.displayClear();
    display.setTextAlignment(PA_CENTER);
    display.print(String(progress / (total / 100)));
  });
  ArduinoOTA.begin();
}

const void initWebserverRoutes() {
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, PUT, POST, DELETE");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");
  server.on("/", HTTP_GET, handleServerRoot);
  server.onNotFound(handleNotFound);
  server.begin();
}

/******************
 ***** Twitch *****
 ******************/
void getTwitchUserInformation() {
  DynamicJsonDocument doc(1024);
  String response = sendGETRequest(Constants::Twitch::API + "/helix/users?login=" + Constants::Twitch::LOGIN);
  deserializeJson(doc, response);
  JsonObject data = doc["data"][0];
  twitchInfos.streamerName = data["display_name"].as<String>();
  twitchInfos.viewCount = data["view_count"].as<long>();
}

void getTwitchLastFollowerInformation() {
  DynamicJsonDocument doc(1024);
  String response = sendGETRequest(Constants::Twitch::API + "/helix/users/follows?to_id=" + Constants::Twitch::LOGIN_ID + "&first=1");
  deserializeJson(doc, response);
  twitchInfos.followersNumber = doc["total"].as<String>();
  JsonObject data = doc["data"][0];
  twitchInfos.lastFollowerName = data["from_name"].as<String>();
}

void getTwitchStreamInformation() {
  DynamicJsonDocument doc(1024);
  String response = sendGETRequest(Constants::Twitch::API + "/helix/streams?user_login=" + Constants::Twitch::LOGIN_ID + "&first=1");
  deserializeJson(doc, response);
  serializeJsonPretty(doc, Serial);
}

void getTwitchSubscriptionInformation() {
  DynamicJsonDocument doc(1024);
  String response = sendGETRequest(Constants::Twitch::API + "/helix/subscriptions?broadcaster_id=" + Constants::Twitch::LOGIN_ID + "&first=1");
  deserializeJson(doc, response);
  serializeJsonPretty(doc, Serial);
}

const String fetchTwitchAccessToken() {
  static String accessToken{""};
  if ((millis() - lastAccessTokenUpdate) > accessTokenUpdateInterval) {
    const String postData = sendPOSTRequest(Constants::Twitch::API_REGISTER + "/oauth2/token", "client_id=" + (String) Constants::Twitch::CLIENT_ID + "&client_secret=" + (String) Constants::Twitch::CLIENT_SECRET + "&grant_type=client_credentials&scope=channel:read:subscriptions");
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, postData);
    accessToken = doc["access_token"].as<String>();
    accessTokenUpdateInterval = doc["expires_in"].as<long>() / 2;
    lastAccessTokenUpdate = millis();
  }
  return accessToken;
}

/******************
 **** Display *****
 ******************/
 // Todo : Learn why not working
const String thanksFollowDisplay(const String name) {
    return String("merci " + name + "pour le follow !!!");
}

/******************
 ****** HTTP ******
 ******************/
String sendGETRequest(const String endpoint) {
    return httpRequest("GET", endpoint);
}

const String sendPOSTRequest(const String endpoint, const String data) {
  return httpRequest("POST", endpoint, data);
}

String httpRequest(const String httpMethod, const String endpoint, const String data) {
  HTTPClient http;
  String payload{""};

  if (http.begin(client, endpoint)) {
    int httpResponseCode{-1};
    if (httpMethod == "POST") {
      httpResponseCode = http.POST(data);
    } else if (httpMethod == "GET") {
      http.addHeader("Host", Constants::Twitch::API);
      http.addHeader("Client-Id", Constants::Twitch::CLIENT_ID);
      http.addHeader("Authorization", "Bearer " + twitchAccessToken);
      http.addHeader("Cache-Control", "no-cache");
      httpResponseCode = http.GET();
    }

    if (httpResponseCode > 0 && (httpResponseCode == HTTP_CODE_OK || httpResponseCode == HTTP_CODE_MOVED_PERMANENTLY)) {
      payload = http.getString();
    } else {
      Serial.printf("Request %s Failed, code: %d, request: %s, data: %s, error: %s\n", httpMethod.c_str(), httpResponseCode, endpoint.c_str(), data.c_str(), http.errorToString(httpResponseCode).c_str());
    }
    http.end();
  } else {
    Serial.println("Unable to connect to internet");
  }
  return payload;
}

/******************
 *** Webserver ****
 ******************/
const void handleServerRoot(AsyncWebServerRequest *request) {
  String message{""};
  DynamicJsonDocument response(512);
  response["streamer"].set(twitchInfos.streamerName);
  response["followers"].set(twitchInfos.followersNumber);
  response["lastFollower"].set(twitchInfos.lastFollowerName);
  response["viewCount"].set(twitchInfos.viewCount);
  response["liveViewer"].set(twitchInfos.liveViewerCount);

  serializeJson(response, message);
  request->send(200, "application/json", message);
}

const void handleNotFound(AsyncWebServerRequest *request) {
  request->send(request->method() == HTTP_OPTIONS ? 200 : 200, "text/html", String(random(25)));
}
