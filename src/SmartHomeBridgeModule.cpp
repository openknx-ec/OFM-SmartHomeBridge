#include <Arduino.h>
#include <knx.h>
#include "hardware.h"
#ifndef SMARTHOMEBRIDGE_DEVICESONLY
#include <WiFi.h>
#include <NetworkModule.h>
#include "HomeKitBridge.h"
#include "HueBridge.h"
#endif
#include "SmartHomeBridgeModule.h"
#include "./Switch/KnxChannelSwitch.h"
#include "./Dimmer/KnxChannelDimmer.h"
#include "./RGB/KnxChannelRGB.h"
#include "./Rolladen/KnxChannelRolladen.h"
#include "./Jalousie/KnxChannelJalousie.h"
#include "./Thermostat/KnxChannelThermostat.h"
#include "./Display/KnxChannelDisplay.h"
#include "./Sensor/KnxChannelSensor.h"
#include "./Fan/KnxChannelFan.h"
#include "./DoorWindow/KnxChannelDoorWindow.h"

#include "knxprod.h"
#include "CP1252ToUTF8.h"

SmartHomeBridgeModule::SmartHomeBridgeModule()
{
}

const std::string SmartHomeBridgeModule::name()
{
  return "SmartHomeBridgeModule";
}

void SmartHomeBridgeModule::showInformations()
{
#ifdef MODULE_SmartHomeBridge_Version
  openknx.logger.logWithPrefixAndValues(logPrefix(), "Smart Home Bridge %s", MODULE_SmartHomeBridge_Version);
#else
  openknx.logger.logWithPrefix(logPrefix(), "Smart Home Bridge");
#endif
}

const std::string SmartHomeBridgeModule::version()
{
  return MODULE_SmartHomeBridge_Version;
}

SmartHomeBridgeModule::~SmartHomeBridgeModule()
{
  if (_utf8Name != nullptr)
  {
    delete _utf8Name;
    _utf8Name = nullptr;
  }
}

const char *SmartHomeBridgeModule::getNameInUTF8()
{
  return _utf8Name;
}

void SmartHomeBridgeModule::setup(bool configured)
{
  if (bridgeInterfaces == nullptr)
    bridgeInterfaces = new DynamicPointerArray<BridgeBase>();

  ChannelOwnerModule::setup(configured);
}

void SmartHomeBridgeModule::setup()
{
  setNumberOfChannels(ParamBRI_VisibleChannels);
#ifdef SMARTHOMEBRIDGE_DEVICESONLY
  logDebugP("Setup Devices");
#else
  logDebugP("Setup Bridge");
#endif
  _utf8Name = convert1252ToUTF8((const char *)ParamBRI_BridgeName);

#ifndef SMARTHOMEBRIDGE_DEVICESONLY
  webServer = new WebServer(webServerPort);

  bool homeKitEnabled = ParamBRI_HomeKitEnabled;
  if (homeKitEnabled)
  {
    logDebugP("Homekit enabled");
    addBridge(new HomeKitBridge());
  }

  bool hueEnabled = ParamBRI_HueEnabled;
  if (hueEnabled)
  {
    logDebugP("Hue enabled");
    addBridge(new HueBridge());
  }
#else
  startBridge();
#endif

  // Do not call base class here, because this creates the channels
}

void SmartHomeBridgeModule::addBridge(BridgeBase *bridge)
{
  if (bridgeInterfaces == nullptr)
    bridgeInterfaces = new DynamicPointerArray<BridgeBase>();
  bridgeInterfaces->push_back(bridge);
}

OpenKNX::Channel *SmartHomeBridgeModule::createChannel(uint8_t _channelIndex /* this parameter is used in macros, do not rename */)
{

  uint8_t deviceType = ParamBRI_CHDeviceType;
  if (ParamBRI_CHDisableChannel && deviceType != 0)
  {
    logInfoP("Device: %d - Disabled", _channelIndex + 1);
    return nullptr;
  }
  KnxChannelBase *channel = nullptr;
  switch (deviceType)
  {
  case 0:
  {
    logInfoP("Device: %d - Inactive", _channelIndex + 1);
    return nullptr;
  }
  case 10:
  case 11:
    channel = new KnxChannelSwitch(_channelIndex);
    break;
  case 20:
    switch (ParamBRI_CHLightType)
    {
    case 0:
      channel = new KnxChannelSwitch(_channelIndex);
      break;
    case 1:
      channel = new KnxChannelDimmer(_channelIndex);
      break;
    case 2:
      channel = new KnxChannelRGB(_channelIndex);
      break;
    default:
      logInfoP("Device: %d - Unkown type subdevice %d for %d", _channelIndex + 1, ParamBRI_CHLightType, deviceType);
      return nullptr;
    }
    break;
  case 30:
    channel = new KnxChannelJalousie(_channelIndex);
    break;
  case 31:
  case 32:
    channel = new KnxChannelRolladen(_channelIndex);
    break;
  case 50:
    channel = new KnxChannelThermostat(_channelIndex);
    break;
  case 60:
  case 61:
  case 62:
    channel = new KnxChannelDisplay(_channelIndex);
    break;
  case 70:
  case 71:
  case 72:
  case 73:
  case 74:
  case 75:
  case 76:
    channel = new KnxChannelSensor(_channelIndex);
    break;
  case 80:
    channel = new KnxChannelFan(_channelIndex);
    break;
  case 90:
  case 91:
  case 92:
    channel = new KnxChannelDoorWindow(_channelIndex);
    break;
  default:
    logInfoP("Device: %d - Unkown device type %d", _channelIndex + 1, deviceType);
    return nullptr;
  }
  logInfoP("Device: %d - %s", _channelIndex + 1, channel->name().c_str());
  for (auto it = bridgeInterfaces->begin(); it != bridgeInterfaces->end(); ++it)
  {
    channel->createBridgeDevice(**it);
  }
  return channel;  
  
}

KnxChannelBase* SmartHomeBridgeModule::getChannel(uint8_t channelIndex)
{
  if (_pChannels == nullptr)
    return nullptr;
  if (channelIndex >= getNumberOfUsedChannels())
    return nullptr;

  return (KnxChannelBase*)_pChannels[channelIndex];
}

bool SmartHomeBridgeModule::processCommand(const std::string cmd, bool diagnoseKo)
{
  if (bridgeInterfaces != nullptr)
  {
    for (auto it = bridgeInterfaces->begin(); it != bridgeInterfaces->end(); ++it)
    {
      if ((*it)->processCommand(cmd, diagnoseKo))
        return true;
    }
  }
  return false;
}

void SmartHomeBridgeModule::showHelp()
{
  if (bridgeInterfaces != nullptr)
  {
    for (auto it = bridgeInterfaces->begin(); it != bridgeInterfaces->end(); ++it)
      (*it)->showHelp();
  }
}

void SmartHomeBridgeModule::startBridge()
{
  started = true;
#ifndef SMARTHOMEBRIDGE_DEVICESONLY

  logDebugP("Start webserver");
  webServer = new WebServer(80);
  // serve pages
  webServer->on("/", HTTP_GET, [this]()
                { this->serveHomePage(); });
  webServer->on("/updateFW", HTTP_GET, [this]()
                { this->serveFirmwareUpdatePage(); });
  webServer->on("/progMode", HTTP_POST, [this]()
                { this->serveProgModePage(); });
  webServer->on("/reboot", HTTP_POST, [this]()
                { this->serveRebootPage(); });
  // handling uploading firmware file
  webServer->on(
      "/update", HTTP_POST, [this]()
      {
    webServer->sendHeader("Connection", "close");
    webServer->send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart(); },
      [this]()
      {
        HTTPUpload &upload = webServer->upload();
        if (upload.status == UPLOAD_FILE_START)
        {
          Serial.printf("Update: %s\n", upload.filename.c_str());
          if (!Update.begin(UPDATE_SIZE_UNKNOWN))
          { // start with max available size
            Update.printError(Serial);
          }
        }
        else if (upload.status == UPLOAD_FILE_WRITE)
        {
          /* flashing firmware to ESP*/
          if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
          {
            Update.printError(Serial);
          }
        }
        else if (upload.status == UPLOAD_FILE_END)
        {
          if (Update.end(true))
          { // true to set the size to the current progress
            Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
          }
          else
          {
            Update.printError(Serial);
          }
        }
      });
#endif
  logDebugP("Initialize briges");
  for (auto it = bridgeInterfaces->begin(); it != bridgeInterfaces->end(); ++it)
    (*it)->initialize(this);

  createChannels();

#ifndef SMARTHOMEBRIDGE_DEVICESONLY
  for (auto it = bridgeInterfaces->begin(); it != bridgeInterfaces->end(); ++it)
    (*it)->initWebServer(*webServer);

  webServer->enableDelay(false);
#endif
  for (auto it = bridgeInterfaces->begin(); it != bridgeInterfaces->end(); ++it)
    (*it)->start(this);

#ifndef SMARTHOMEBRIDGE_DEVICESONLY
  webServer->begin();
#endif
}

void SmartHomeBridgeModule::loop()
{
#ifndef SMARTHOMEBRIDGE_DEVICESONLY

  bool connected = openknxNetwork.connected();
  if (connected && !started)
  {
    startBridge();
  }
  if (webServer != nullptr)
    webServer->handleClient();
#endif
  for (auto it = bridgeInterfaces->begin(); it != bridgeInterfaces->end(); ++it)
    (*it)->loop();

  ChannelOwnerModule::loop();
}

#ifdef OPENKNX_DUALCORE
void SmartHomeBridgeModule::loop1()
{
  for (auto it = bridgeInterfaces->begin(); it != bridgeInterfaces->end(); ++it)
    (*it)->loop1();

  ChannelOwnerModule::loop1();
}
#endif

void SmartHomeBridgeModule::processInputKo(GroupObject &ko)
{
  if (bridgeInterfaces != nullptr)
  {
    for (auto it = bridgeInterfaces->begin(); it != bridgeInterfaces->end(); ++it)
      (*it)->processInputKo(ko);
  }
  ChannelOwnerModule::processInputKo(ko);
}

#ifndef SMARTHOMEBRIDGE_DEVICESONLY
WebServer *SmartHomeBridgeModule::getWebServer()
{
  return webServer;
}

uint16_t SmartHomeBridgeModule::getWebServerPort()
{
  return webServerPort;
}

const char *firmwareUpdatePage =
    "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
    "<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
    "<input type='file' name='update'>"
    "<input type='submit' value='Update'>"
    "</form>"
    "<div id='prg'>progress: 0%</div>"
    "<script>"
    "$('form').submit(function(e){"
    "e.preventDefault();"
    "var form = $('#upload_form')[0];"
    "var data = new FormData(form);"
    " $.ajax({"
    "url: '/update',"
    "type: 'POST',"
    "data: data,"
    "contentType: false,"
    "processData:false,"
    "xhr: function() {"
    "var xhr = new window.XMLHttpRequest();"
    "xhr.upload.addEventListener('progress', function(evt) {"
    "if (evt.lengthComputable) {"
    "var per = evt.loaded / evt.total;"
    "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
    "}"
    "}, false);"
    "return xhr;"
    "},"
    "success:function(d, s) {"
    "console.log('success!')"
    "},"
    "error: function (a, b, c) {"
    "}"
    "});"
    "});"
    "</script>";

void SmartHomeBridgeModule::serveFirmwareUpdatePage()
{
  webServer->send(200, "text/html;charset=UTF-8", firmwareUpdatePage);
}

void SmartHomeBridgeModule::serveRebootPage()
{
  String res = "<!DOCTYPE html><html lang=\"en\"><meta charset=\"UTF-8\"><meta http-equiv=\"refresh\" content=\"20;url=/\"><title>";
  res + "Smart Home Bridge Reboot";
  res += "</title><body>";
  res += "<br>Smart Home Bridge is rebooting...</br>";
  res += "</body>";
  webServer->send(200, "text/html;charset=UTF-8", res);
  vTaskDelay(1000);
  openknx.restart();
}

void SmartHomeBridgeModule::serveProgModePage()
{
  auto progMode = webServer->arg("progMode") == "1";
  if (progMode)
    knx.progMode(true);
  else
    knx.progMode(false);
  String res = "<!DOCTYPE html><html lang=\"en\"><meta charset=\"UTF-8\"><meta http-equiv=\"refresh\" content=\"3;url=/\"><title>";
  res + "Smart Home Bridge Prog Mode";
  res += "</title><body>";
  res += "<br>Prog mode ";
  res += progMode ? "activated" : "deactivated";
  res += "</br>";
  res += "</body>";
  webServer->send(200, "text/html;charset=UTF-8", res);
}

void SmartHomeBridgeModule::serveHomePage()
{
    auto name = String(getNameInUTF8());
    name.replace("<", "&lt;"); name.replace(">", "&gt;"); name.replace("&", "&amp;"); name.replace("\"", "&quot;");

    String res = R"rawliteral(
    <!DOCTYPE html>
    <html lang="en">
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>)rawliteral" + name + R"rawliteral(</title>
        <style>
            body { font-family: Arial, sans-serif; text-align: center; background: #121212; color: #fff; margin: 0; padding: 20px; }
            h1 { color: #4CAF50; font-size: 2em; margin: 0; }
            .logo { width: 100px; height: auto; margin-bottom: 20px; }
            table { width: 90%; max-width: 600px; margin: 20px auto; border-collapse: collapse; background: #222; border-radius: 8px; overflow: hidden; }
            th, td { padding: 12px; border-bottom: 1px solid #444; }
            th { background: #333; font-weight: bold; }
            tr:hover { background: #444; }
            button { padding: 10px 15px; margin: 10px; border: none; cursor: pointer; border-radius: 5px; font-size: 16px; }
            .btn-primary { background: #4CAF50; color: white; }
            .btn-danger { background: #d9534f; color: white; }
            .btn-warning { background: #f0ad4e; color: white; }
            .btn:hover { opacity: 0.8; }
            .container { max-width: 800px; margin: auto; }
        </style>
    </head>
    <body>
        <div class="container">
            <!-- OpenKNX Logo -->
            <img alt="" src="data:image/svg+xml;base64,PHN2ZyB3aWR0aD0iMzIiIGhlaWdodD0iMzIiIHZpZXdCb3g9IjAgMCAzMiAzMiIgZmlsbD0ibm9uZSIgeG1sbnM9Imh0dHA6Ly93d3cudzMub3JnLzIwMDAvc3ZnIj4KPHJlY3QgeD0iMCIgeT0iMCIgd2lkdGg9IjMyIiBoZWlnaHQ9IjMyIiBmaWxsPSJ3aGl0ZSIvPgo8bGluZSB4MT0iNyIgeTE9IjIwIiB4Mj0iNyIgeTI9IjE1IiBzdHJva2U9IiM0NDk4NDEiIHN0cm9rZS13aWR0aD0iMiIvPgo8bGluZSB5MT0iMTYiIHgyPSIzMiIgeTI9IjE2IiBzdHJva2U9IiM0NDk4NDEiIHN0cm9rZS13aWR0aD0iMiIvPgo8cmVjdCB4PSIyIiB5PSIyMSIgd2lkdGg9IjEwIiBoZWlnaHQ9IjEwIiBmaWxsPSIjNDQ5ODQxIi8+CjxyZWN0IHg9IjIwIiB5PSIxIiB3aWR0aD0iMTAiIGhlaWdodD0iMTAiIGZpbGw9IiM0NDk4NDEiLz4KPHJlY3QgeD0iMjIiIHk9IjIzIiB3aWR0aD0iNiIgaGVpZ2h0PSI2IiBzdHJva2U9ImJsYWNrIiBzdHJva2Utd2lkdGg9IjIiLz4KPHJlY3QgeD0iNCIgeT0iMyIgd2lkdGg9IjYiIGhlaWdodD0iNiIgc3Ryb2tlPSJibGFjayIgc3Ryb2tlLXdpZHRoPSIyIi8+Cjwvc3ZnPgo=" />
            <h1>OpenKNX SmartHome Bridge</h1>
            <p>© OpenKNX, Michael Geramb, 2023-)rawliteral" + (__DATE__ + sizeof(__DATE__) - 5) + R"rawliteral(</p>
            <table>
    )rawliteral";

    auto addRow = [&](const String& key, const String& value) {
        res += "<tr><td>" + key + "</td><td>" + value + "</td></tr>";
    };

    addRow("Name", name);
#ifdef DEVICE_NAME
    addRow("Gerätename", String(DEVICE_NAME));
#endif
#ifdef DEVICE_ID
    addRow("Geräte ID", String(DEVICE_ID));
#endif
    addRow("IP", openknxNetwork.localIP().toString());
    addRow("Subnetz", openknxNetwork.subnetMask().toString());
    addRow("Gateway", openknxNetwork.gatewayIP().toString());
    addRow("WiFi SSID", WiFi.SSID());
    addRow("WiFi Signal", String(WiFi.RSSI()) + " dBm");
    addRow("DNS", openknxNetwork.nameServerIP().toString());

    char etsType[5];
    sprintf(etsType, "%02X%02X", MAIN_OpenKnxId, MAIN_ApplicationNumber);
    addRow("ETS Gerätetype", "0x" + String(etsType));
    addRow("ETS App Version", String(MAIN_ApplicationVersion));
    addRow("KNX Address", openknx.info.humanIndividualAddress().c_str());
    addRow("KNX Serial", openknx.info.humanSerialNumber().c_str());
    addRow("KNX Version", String(KNX_Version));
    addRow("Firmware", String(MAIN_Version));
    addRow("Common Version", String(MODULE_Common_Version));
    addRow("LogicModul Version", String(MODULE_LogicModule_Version));
    addRow("Arduino Version", String(ESP_ARDUINO_VERSION_MAJOR) + "." +
                              String(ESP_ARDUINO_VERSION_MINOR) + "." +
                              String(ESP_ARDUINO_VERSION_PATCH));
    addRow("IDF Version", String(ESP_IDF_VERSION_MAJOR) + "." +
                          String(ESP_IDF_VERSION_MINOR) + "." +
                          String(ESP_IDF_VERSION_PATCH));

    addRow("Chip Revision", String(ESP.getChipRevision()));
    addRow("SDK Version", String(ESP.getSdkVersion()));
    addRow("CPU Frequenz", String(ESP.getCpuFreqMHz()) + " MHz");
    addRow("CPU Temperatur", String(temperatureRead()) + " °C");
    addRow("CPU Kerne", String(ESP.getChipCores()));
    addRow("Flash", String(ESP.getFlashChipSize() / 1024) + " KB");
    if(ESP.getFreePsram() > 0)
    {
      addRow("PSRAM frei", String(ESP.getFreePsram() / 1024) + " von " + String(ESP.getPsramSize() / 1024) + " KB");
      addRow("PSRAM Min. frei", String(ESP.getMinFreePsram()) + " Bytes");
    }
    addRow("Heap", String(ESP.getFreeHeap()) + " von " + String(ESP.getHeapSize()) + " Bytes");
    addRow("Heap freier Heapblock", String(ESP.getMaxAllocHeap()) + " Bytes");
    addRow("Heap minimaler freier Heap", String(ESP.getMinFreeHeap()) + " Bytes");
    addRow("Stack Nutzung", String(8192 - uxTaskGetStackHighWaterMark(nullptr)) + " von 8192 Bytes");
    addRow("Verwendete Kanäle", String(getNumberOfUsedChannels()) + " von " + String(BRI_ChannelCount));
    addRow("Laufzeit", String(millis() / 3600000) + ":" +
                       String((millis() / 60000) % 60) + ":" +
                       String((millis() / 1000) % 60) + " (h:m:s)");

    // 0 = normal, 1 = power on, 2 = external, 3 = software, 4 = hardware, 5 = watchdog, 6 = exception
    addRow("Reset Grund", String(esp_reset_reason() == 1 ? "Power On" : esp_reset_reason() == 2 ? "External" : esp_reset_reason() == 3 ? "Software" : esp_reset_reason() == 4 ? "Hardware" : esp_reset_reason() == 5 ? "Watchdog" : esp_reset_reason() == 6 ? "Exception" : "Normal"));
    

    res += "</table>";
    
    res += "<h2>Bridges:</h2>"; // HomeKit, Hue, etc.
    for (auto it = bridgeInterfaces->begin(); it != bridgeInterfaces->end(); ++it)
    {
      (*it)->getInformation(res);
      res += "<br>";
    }

    res += "<h2>Steuerung</h2>";
    res += R"rawliteral(
            <form method='post' action='/progMode'>
                <button class='btn-warning' name='progMode' value=')rawliteral" + 
                String(knx.progMode() ? "0" : "1") + R"rawliteral('>)rawliteral" +
                String(knx.progMode() ? "Stopp KNX Programmierung" : "Start KNX Programmierung") + 
                R"rawliteral(</button>
            </form>
            <form method='post' action='/reboot'>
                <button class='btn-danger'>Gerät neustarten</button>
            </form>
            <form method='get' action='/updateFW'>
                <button class='btn-primary'>Firmware Update</button>
            </form>
        </div>
    </body>
    </html>
    )rawliteral";

    webServer->send(200, "text/html;charset=UTF-8", res);
}
#endif // SMARTHOMEBRIDGE_DEVICESONLY

SmartHomeBridgeModule openknxSmartHomeBridgeModule;