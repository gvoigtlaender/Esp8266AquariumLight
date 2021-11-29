/* Copyright 2019 Georg Voigtlaender gvoigtlaender@googlemail.com */
/*
 */
#include <Arduino.h>

#include <string>
using std::string;
#include "config.h"
#include <ctime>
#include <iostream>
#include <list>
#include <map>
#include <vector>

string APPNAMEVER = APPNAME + string(" ") + VERSION_STRING;

#pragma region SysLog
#include <CSyslog.h>
CSyslog *m_pSyslog = NULL;
#pragma endregion

#include <CControl.h>

#include <CMqtt.h>
CMqtt *m_pMqtt = NULL;

#include <CWifi.h>
CWifi *m_pWifi = NULL;

#if USE_DISPLAY == 1
#include <CDisplay.h>
CDisplayBase *m_pDisplay = NULL;
#endif

#include <CButton.h>
CButton *m_pButton = NULL;

#include <CLed.h>
CLed *m_pLed = NULL;

#include <CLight.h>
CLight *m_pLight = NULL;

#include <CNtp.h>
CNtp *m_pNtp = NULL;

#include <CBase.h>

#pragma region configuration
#include <CConfiguration.h>
CConfiguration *m_pConfig = NULL;
CConfigKey<string> *m_pDeviceName = NULL;
#pragma endregion

#pragma region WebServer
#include <CUpdater.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>

ESP8266WebServer server(80);
// const size_t szhtml_content_buffer_size = 2800;
// static char szhtml_content_buffer[szhtml_content_buffer_size] = {0};

#include <CUpdater.h>
CUpdater *m_pUpdater = NULL;
string sStylesCss = "styles.css";
string sJavascriptJs = "javascript.js";

string sHtmlHead =
    "<link rel=\"stylesheet\" type=\"text/css\"  href=\"/" + sStylesCss +
    "\">\n"
    "<script language=\"javascript\" type=\"text/javascript\" src=\"/" +
    sJavascriptJs +
    "\"></script>\n"
    "<meta name=\"viewport\" content=\"width=device-width, "
    "initial-scale=1.0, maximum-scale=1.0, user-scalable=0\">\n"
    "<meta charset='utf-8'><meta name=\"viewport\" "
    "content=\"width=device-width,initial-scale=1,user-scalable=no\"/>\n";

#if defined _OLD_CODE
void handleConfigure() {
  CControl::Log(CControl::I, "handleConfigure, args=%d, method=%d",
                server.args(), (int)server.method());
  if (server.args() > 0) {
    m_pConfig->handleArgs(&server, sHtmlHead.c_str(), APPNAMEVER.c_str());
  } else {
    // server.send(200, "text/html", INDEX_HTML);
    m_pConfig->GetHtmlForm(sHtmlHead.c_str(), APPNAMEVER.c_str());
    CControl::Log(CControl::I, "sending %u bytes",
                  strlen(szhtml_content_buffer));
    server.send(200, "text/html", szhtml_content_buffer);
  }
  CControl::Log(CControl::I, "handleConfigure done");
}
void handleConfigureGET() {
  CControl::Log(CControl::I, "handleConfigureGET, args=%d, method=%d",
                server.args(), (int)server.method());
  handleConfigure();
}
void handleConfigurePOST() {
  CControl::Log(CControl::I, "handleConfigurePOST, args=%d, method=%d",
                server.args(), (int)server.method());
  handleConfigure();
}

void handleRoot() {
  CControl::Log(CControl::I, "handleRoot, args=%d", server.args());
  if (server.args() > 0) {
    // m_pConfig->handleArgs(&server);
    for (int n = 0; n < server.args(); n++) {
      CControl::Log(CControl::I, "Arg %d: %s = %s", n,
                    server.argName(n).c_str(), server.arg(n).c_str());
    }
    if (server.argName(0) == String("m") && server.arg(0) == String("1")) {
      if (server.argName(1) == String("o")) {
        int n = atoi(server.arg(1).c_str());
        if (n >= CLight::eAutomatic && n <= CLight::eOff)
          m_pLight->SwitchControlMode((CLight::E_LIGHTCONTROLMODE)n);
      }
    }
    if (server.argName(0) == String("rst") && server.arg(0) == String("")) {
      server.send(
          200, "text/html",
          "<META http-equiv=\"refresh\" content=\"10;URL=/\">Rebooting...");
      delay(100);
      ESP.restart();
    }

  } else {
    // server.send(200, "text/html", INDEX_HTML);
    std::string sTitle = "---"; // APPNAMEVER;
    memset(szhtml_content_buffer, 0, szhtml_content_buffer_size);
    snprintf(szhtml_content_buffer, szhtml_content_buffer_size,
             "<!DOCTYPE HTML>\n");

    snprintf(szhtml_content_buffer + strlen(szhtml_content_buffer),
             szhtml_content_buffer_size - strlen(szhtml_content_buffer),
             "<html>\n");
    snprintf(szhtml_content_buffer + strlen(szhtml_content_buffer),
             szhtml_content_buffer_size - strlen(szhtml_content_buffer),
             "<head>\n");
    snprintf(szhtml_content_buffer + strlen(szhtml_content_buffer),
             szhtml_content_buffer_size - strlen(szhtml_content_buffer),
             sHtmlHead.c_str());
    snprintf(szhtml_content_buffer + strlen(szhtml_content_buffer),
             szhtml_content_buffer_size - strlen(szhtml_content_buffer),
             "<script language=\"javascript\" type=\"text/javascript\" "
             "src=\"mainpage.js\"></script>\n");
    snprintf(szhtml_content_buffer + strlen(szhtml_content_buffer),
             szhtml_content_buffer_size - strlen(szhtml_content_buffer),
             "<title id=title>%s</title>\n", sTitle.c_str());
    snprintf(szhtml_content_buffer + strlen(szhtml_content_buffer),
             szhtml_content_buffer_size - strlen(szhtml_content_buffer),
             "</head>\n");
    snprintf(szhtml_content_buffer + strlen(szhtml_content_buffer),
             szhtml_content_buffer_size - strlen(szhtml_content_buffer),
             "<body>\n");
    snprintf(szhtml_content_buffer + strlen(szhtml_content_buffer),
             szhtml_content_buffer_size - strlen(szhtml_content_buffer),
             "<noscript>To use %s, please enable JavaScript<br></noscript>\n",
             APPNAME);
    snprintf(
        szhtml_content_buffer + strlen(szhtml_content_buffer),
        szhtml_content_buffer_size - strlen(szhtml_content_buffer),
        "<div "
        "style='text-align:center;display:inline-block;color:#eaeaea;min-width:"
        "340px;'>\n");
    snprintf(szhtml_content_buffer + strlen(szhtml_content_buffer),
             szhtml_content_buffer_size - strlen(szhtml_content_buffer),
             "<h1 id=title2>%s</h1>\n", sTitle.c_str());
    snprintf(szhtml_content_buffer + strlen(szhtml_content_buffer),
             szhtml_content_buffer_size - strlen(szhtml_content_buffer),
             "<h2 id=devicename>%s</h2>",
             "---" /*m_pDeviceName->m_pTValue->m_Value.c_str()*/);
    // html_content_buffer += "<a href=\"configure\">configure</a>\n";
    snprintf(szhtml_content_buffer + strlen(szhtml_content_buffer),
             szhtml_content_buffer_size - strlen(szhtml_content_buffer),
             "<fieldset>\n");
    snprintf(szhtml_content_buffer + strlen(szhtml_content_buffer),
             szhtml_content_buffer_size - strlen(szhtml_content_buffer),
             "<legend>Light Mode</legend>\n");
    snprintf(szhtml_content_buffer + strlen(szhtml_content_buffer),
             szhtml_content_buffer_size - strlen(szhtml_content_buffer),
             "<button onclick='sw(\"o=0\");' "
             "class='button'>Automatic</button><p><p>\n");
    snprintf(szhtml_content_buffer + strlen(szhtml_content_buffer),
             szhtml_content_buffer_size - strlen(szhtml_content_buffer),
             "<div id=but3d style=\"display: block;\"></div>\n");

    snprintf(szhtml_content_buffer + strlen(szhtml_content_buffer),
             szhtml_content_buffer_size - strlen(szhtml_content_buffer),
             "<button onclick='sw(\"o=2\");' "
             "class='button'>White</button><p>\n");
    snprintf(szhtml_content_buffer + strlen(szhtml_content_buffer),
             szhtml_content_buffer_size - strlen(szhtml_content_buffer),
             "<button onclick='sw(\"o=1\");' "
             "class='button'>Blue</button><p>\n");
    snprintf(szhtml_content_buffer + strlen(szhtml_content_buffer),
             szhtml_content_buffer_size - strlen(szhtml_content_buffer),
             "<button onclick='sw(\"o=3\");' "
             "class='button'>Off</button><p>\n");
    snprintf(szhtml_content_buffer + strlen(szhtml_content_buffer),
             szhtml_content_buffer_size - strlen(szhtml_content_buffer),
             "</fieldset>\n");
    snprintf(szhtml_content_buffer + strlen(szhtml_content_buffer),
             szhtml_content_buffer_size - strlen(szhtml_content_buffer),
             "<fieldset>\n");

    snprintf(szhtml_content_buffer + strlen(szhtml_content_buffer),
             szhtml_content_buffer_size - strlen(szhtml_content_buffer),
             "<legend>Status</legend>\n");

    snprintf(szhtml_content_buffer + strlen(szhtml_content_buffer),
             szhtml_content_buffer_size - strlen(szhtml_content_buffer),
             "<div id=\"status\"> ... loading </div>\n");

    snprintf(szhtml_content_buffer + strlen(szhtml_content_buffer),
             szhtml_content_buffer_size - strlen(szhtml_content_buffer),
             "</fieldset>\n");

    snprintf(szhtml_content_buffer + strlen(szhtml_content_buffer),
             szhtml_content_buffer_size - strlen(szhtml_content_buffer),
             "<fieldset>\n");

    snprintf(szhtml_content_buffer + strlen(szhtml_content_buffer),
             szhtml_content_buffer_size - strlen(szhtml_content_buffer),
             "<legend>Administration</legend>\n");
    snprintf(
        szhtml_content_buffer + strlen(szhtml_content_buffer),
        szhtml_content_buffer_size - strlen(szhtml_content_buffer),
        "<p>\n"
        "<form id=but3 style=\"display: block;\" action='configure' "
        "method='get'>\n"
        "<button>Configuration</button>\n"
        "</form>\n"
        "</p>\n"
        "<p>\n"
        "<form id=but4 style=\"display: block;\" action='in' "
        "method='get'>\n"
        "</form>\n"
        "</p>\n"
        "<p>\n"
        "<form id=but5 style =\"display: block;\" action='update' "
        "method='get'>\n"
        "<button>Firmware Upgrade</button>\n"
        "</form>\n"
        "</p>\n"
        "<p>\n"
        "<form id=but14 style=\"display: block;\" action='cs' method='get'>\n"
        "</form>\n"
        "</p>\n"
        "<p>\n"
        "<form id=but0 style=\"display: block;\" action='.' method='get' "
        "onsubmit='return confirm(\"Confirm Restart\");'>\n"
        "<button name='rst' class='button bred'>Restart</button>\n"
        "</form>\n"
        "</p>\n");
    snprintf(szhtml_content_buffer + strlen(szhtml_content_buffer),
             szhtml_content_buffer_size - strlen(szhtml_content_buffer),
             "</div>\n");
    snprintf(szhtml_content_buffer + strlen(szhtml_content_buffer),
             szhtml_content_buffer_size - strlen(szhtml_content_buffer),
             "</fieldset>\n");
    snprintf(szhtml_content_buffer + strlen(szhtml_content_buffer),
             szhtml_content_buffer_size - strlen(szhtml_content_buffer),
             "</body>\n");
    snprintf(szhtml_content_buffer + strlen(szhtml_content_buffer),
             szhtml_content_buffer_size - strlen(szhtml_content_buffer),
             "</html>\n");
    server.send(200, "text/html", szhtml_content_buffer);
  }
  CControl::Log(CControl::I, "handleRoot done, buffersize=%u",
                strlen(szhtml_content_buffer));
}
#endif

void handleStatusUpdate() {
  // CControl::Log(CControl::I, "handleStatusUpdate, args=%d", server.args());
  // string sUpdate = "";
  CheckFreeHeap();
  time_t m_RawTime;
  struct tm *m_pTimeInfo;
  time(&m_RawTime);
  m_pTimeInfo = localtime(&m_RawTime);

  // asctime(m_pTimeInfo);
  char mbstr[100];
  std::strftime(mbstr, sizeof(mbstr), "%A %c", m_pTimeInfo);
  CheckFreeHeap();
  // std::map<string, string> oStates;
  std::vector<std::pair<string, string>> oStates;
  oStates.push_back(
      std::make_pair("Light Mode", m_pLight->m_pMqtt_ControlMode->getValue()));
  oStates.push_back(std::make_pair("Light State", m_pLight->m_sState));
  oStates.push_back(std::make_pair(
      "Time to change", TimeToTimeString(m_pLight->m_nTimeToStateChangeS)));
  oStates.push_back(std::make_pair(
      "White Value", to_string_with_precision(m_pLight->m_dValueWhiteP_, 2)));
  oStates.push_back(std::make_pair(
      "Blue Value", to_string_with_precision(m_pLight->m_dValueBlueP_, 2)));
  oStates.push_back(std::make_pair("Heap", std::to_string(g_uiHeap)));
  oStates.push_back(std::make_pair("Heap Min", std::to_string(g_uiHeapMin)));

  oStates.push_back(std::make_pair(
      "FS Free", std::to_string(LittleFS_GetFreeSpaceKb()) + string("kB")));

  CheckFreeHeap();

  string sContent = "";
  sContent += string(mbstr) + string("\n");

  sContent += "<table>\n";
  sContent += "<tr style=\"height:2px\"><th></th><th></th></tr>\n";

  for (unsigned int n = 0; n < oStates.size(); n++) {
    sContent += "<tr><td>" + oStates[n].first + "</td><td>" +
                oStates[n].second + "</td></tr>\n";
  }

  sContent += "</td></tr>\n";
  sContent += "</table>\n";
  CheckFreeHeap();
  server.send(200, "text/html", sContent.c_str());
  CControl::Log(CControl::I, "handleStatusUpdate done, buffersize=%u",
                sContent.length());
}

void handleTitle() {
  CControl::Log(CControl::I, "handleTitle");
  server.send(200, "text/html", APPNAMEVER.c_str());
}
void handleDeviceName() {
  CControl::Log(CControl::I, "handleDeviceName");
  server.send(200, "text/html", m_pDeviceName->m_pTValue->m_Value.c_str());
}

void handleSwitch() {
  CControl::Log(CControl::I, "handleSwitch, args=%d", server.args());
  if (server.args() > 0) {
    // m_pConfig->handleArgs(&server);
    for (int n = 0; n < server.args(); n++) {
      CControl::Log(CControl::I, "Arg %d: %s = %s", n,
                    server.argName(n).c_str(), server.arg(n).c_str());
    }
    if (server.argName(0) == String("o")) {
      int n = atoi(server.arg(0).c_str());
      if (n >= CLight::eAutomatic && n <= CLight::eOff)
        m_pLight->SwitchControlMode((CLight::E_LIGHTCONTROLMODE)n);
    }
    if (server.argName(0) == String("rst") && server.arg(0) == String("")) {
      ESP.restart();
    }
  }
}
void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}
#pragma endregion

void SetupServer() {
  string sStylesCssUri = "/" + sStylesCss;
  server.serveStatic(sStylesCssUri.c_str(), LittleFS, sStylesCss.c_str());
  string sJavascriptJsUri = "/" + sJavascriptJs;
  server.serveStatic(sJavascriptJsUri.c_str(), LittleFS, sJavascriptJs.c_str());
  server.serveStatic("/mainpage.js", LittleFS, "mainpage.js");
  server.on("/statusupdate.html", handleStatusUpdate);
  server.on("/title", handleTitle);
  server.on("/devicename", handleDeviceName);
  // server.on("/configure", handleConfigure);

  m_pConfig->SetupServer(&server, false);
  // server.on("/", handleRoot);
  server.serveStatic("/", LittleFS, "index.html");
  server.on("/switch", handleSwitch);
  server.onNotFound(handleNotFound);
}

void wifisetupfailed() {
  // Set WiFi to station mode and disconnect from an AP if it was previously
  // connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  delay(100);

  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0) {
    Serial.println("no networks found");
  } else {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i) {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");

      std::string ssid = WiFi.SSID(i).c_str();
      m_pWifi->m_pWifiSsid->m_pTValue->m_Choice.push_back(ssid);
      delay(10);
    }
  }

  WiFi.softAP(APPNAME);

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  SetupServer();
  server.begin();
  Serial.println("HTTP server started");
  Serial.println("");

  while (true) {
    server.handleClient();
  }

  return;
}
unsigned int nMillisLast = 0;
void setup(void) {
  nMillisLast = millis();

  Serial.begin(74880);
  CControl::Log(CControl::I, "::setup()");
#if defined DEBUG_ESP_PORT
  CControl::Log(CControl::D, "DEBUG_ESP_PORT");
#endif
#if defined DEBUG_ESP_HTTP_SERVER
  CControl::Log(CControl::D, "DEBUG_ESP_HTTP_SERVER");
#endif

  Serial.println(ESP.getFreeHeap(), DEC);
  // szhtml_content_buffer = (char *)malloc(szhtml_content_buffer_size);
  // CControl::Log(CControl::D, "szhtml_content_buffer size=%u",
  // szhtml_content_buffer_size);

  m_pConfig =
      new CConfiguration("/config.json", APPNAMEVER.c_str(), sHtmlHead.c_str());
  m_pDeviceName = new CConfigKey<string>("Device", "Name", SHORTNAME);

  m_pWifi = new CWifi(APPNAME /*WLAN_SSID, WLAN_PASSWD*/);
  m_pMqtt = new CMqtt();
  m_pNtp = new CNtp();
  m_pSyslog = new CSyslog(APPNAME, SHORTNAME);
  m_pLight = new CLight();
  m_pButton = new CButton(PIN_BTN);
  m_pLed = new CLed(PIN_LED);

  m_pConfig->load();
  CControl::Log(CControl::I, "loading config took %ldms",
                millis() - nMillisLast);

  m_pMqtt->setClientName(m_pDeviceName->m_pTValue->m_Value.c_str());
  m_pSyslog->m_pcsDeviceName = m_pDeviceName->m_pTValue->m_Value.c_str();

  nMillisLast = millis();

  new CMqttValue("SYSTEM/APPNAME", APPNAME);
  new CMqttValue("SYSTEM/Version", VERSION_STRING);

  if (m_pWifi->m_pWifiSsid->m_pTValue->m_Value.empty() ||
      m_pWifi->m_pWifiPassword->m_pTValue->m_Value.empty()) {
    wifisetupfailed();
  }

  CControl::Log(CControl::I, "creating hardware took %ldms",
                millis() - nMillisLast);
  nMillisLast = millis();

  CControl::Log(CControl::I, "starting server");
  SetupServer();
  nMillisLast = millis();
  CControl::Log(CControl::I, "server started");

  m_pUpdater =
      new CUpdater(&server, "/update", APPNAMEVER.c_str(), sHtmlHead.c_str());

  CControl::Log(CControl::I, "setup()");
  if (!CControl::Setup()) {
    // server.on("/", handleConfigure);
    m_pConfig->SetupServer(&server, true);
    server.begin();
    CControl::Log(CControl::I, "HTTP server started");
    CControl::Log(CControl::I, "");
    return;
  }
  Serial.println(ESP.getFreeHeap(), DEC);
  CControl::Log(CControl::I, "startup completed");
}

bool bStarted = false;
uint64_t nMillis = millis() + 1000;
void ServerStart() {

  if (WiFi.status() != WL_CONNECTED) {
    nMillis = millis() + 2000;
    return;
  }

  IPAddress oIP = WiFi.localIP();
  String sIP = oIP.toString();

  server.begin();

  CControl::Log(CControl::I, "Connect to http://%s", sIP.c_str());

  bStarted = true;
  nMillis = millis() + 200000;
}

void loop(void) {
  if (bStarted) {
    server.handleClient();
    /*
    if (nMillis < millis()) {
      bStarted = false;
      server.close();
    }
    */
    // httpServer.handleClient();
    MDNS.update();
  } else {
    if (nMillis < millis()) {
      if (digitalRead(0) == LOW || true) {
        ServerStart();
      }
    }
  }

  CControl::Control();

  switch (m_pButton->getButtonState()) {
  case CButton::eNone:
    break;

  case CButton::ePressed:
    break;

  case CButton::eClick:
    m_pLight->OnButtonClick();
    // m_pLed->AddBlinkTask(CLed::BLINK_1);
    m_pButton->setButtonState(CButton::eNone);
    break;

  case CButton::eDoubleClick:
    m_pButton->setButtonState(CButton::eNone);
    break;

  case CButton::eLongClick:
    m_pLight->SwitchControlMode(CLight::eAutomatic);
    m_pButton->setButtonState(CButton::eNone);
    break;

  case CButton::eVeryLongClick:
    m_pButton->setButtonState(CButton::eNone);
    break;
  }

  CheckFreeHeap();
}
