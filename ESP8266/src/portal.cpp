#include "portal.h"
#include "Logging.h"
#include "master_i2c.h"
#include "utils.h"
#include "WateriusHttps.h"
#include "wifi_settings.h"

extern SlaveData data;
extern MasterI2C masterI2C;
extern Settings sett;
extern CalculatedData cdata;

#define SETUP_TIME_SEC 600UL

#define IMPULS_LIMIT_1 3 // Если пришло импульсов меньше 3, то перед нами 10л/имп. Если больше, то 1л/имп.

uint8_t get_auto_factor(uint32_t runtime_impulses, uint32_t impulses)
{
    return (runtime_impulses - impulses <= IMPULS_LIMIT_1) ? 10 : 1;
}

uint8_t get_factor(uint8_t combobox_factor, uint32_t runtime_impulses, uint32_t impulses, uint8_t cold_factor)
{

    switch (combobox_factor)
    {
    case AUTO_IMPULSE_FACTOR:
        return get_auto_factor(runtime_impulses, impulses);
    case AS_COLD_CHANNEL:
        return cold_factor;
    default:
        return combobox_factor; // 1, 10, 100
    }
}

void Portal::onGetRoot(AsyncWebServerRequest *request)
{
    LOG_INFO(F("AsyncWebServer GET /"));
    request->send(LittleFS, "/index.html");
}

void Portal::onGetScript(AsyncWebServerRequest *request)
{
    LOG_INFO(F("AsyncWebServer GET /"));
    request->send(LittleFS, "/script.js");
};

void Portal::onGetNetworks(AsyncWebServerRequest *request)
{
    LOG_INFO(F("AsyncWebServer GET /networks"));
    int n = WiFi.scanComplete();
    if (n == -2)
    {
        WiFi.scanNetworks(true);
        request->send(100, "", "");
    }
    else if (n)
    {
        String json = "";
        for (int i = 0; i < n; ++i)
        {
            LOG_INFO(WiFi.SSID(i) << " " << WiFi.RSSI(i));
            json += F("<label class='radcnt' onclick='c(this)'>");
            json += String(WiFi.SSID(i));
            json += F("<input type='radio' name='n'><span class='rmrk'></span><div role='img' class='q q-");
            json += String(int(round(map(WiFi.RSSI(i), -100, -50, 1, 4))));
            if (WiFi.encryptionType(WiFi.encryptionType(i)) != ENC_TYPE_NONE)
            {
                json += F(" l");
            }
            json += F("'></div></label>");
        }
        WiFi.scanDelete();
        if (WiFi.scanComplete() == -2)
        {
            WiFi.scanNetworks(true);
        }
        json += F("<br/>");
        request->send(200, "", json);
        json = String();
    }
};

void Portal::onGetConfig(AsyncWebServerRequest *request)
{
    LOG_INFO(F("AsyncWebServer GET /config"));
    AsyncResponseStream *response = request->beginResponseStream("text/html");
    response->addHeader("Server", "ESP Async Web Server");
    response->print("{ \"wmail\": \"");
    response->print(sett.waterius_email);
    response->print("\", \"whost\":\"");
    response->print(sett.waterius_host);
    LOG_INFO(F("config WiFi"));
    response->print("\", \"s\":\"");
    response->print(WiFi.SSID().c_str());
    response->print("\", \"p\":\"");
    response->print(WiFi.psk().c_str());
    LOG_INFO(F("config Blynk"));
    response->print("\", \"bkey\":\"");
    response->print(sett.blynk_key);
    response->print("\", \"bhost\":\"");
    response->print(sett.blynk_host);
    response->print("\", \"bemail\":\"");
    response->print(sett.blynk_email);
    response->print("\", \"btitle\":\"");
    response->print(sett.blynk_email_title);
    response->print("\", \"btemplate\":\"");
    response->print(sett.blynk_email_template);
    LOG_INFO(F("config MQTT"));
    response->print("\", \"mhost\":\"");
    response->print(sett.mqtt_host);
    response->print("\", \"mlogin\":\"");
    response->print(sett.mqtt_login);
    response->print("\", \"mpassword\":\"");
    response->print(sett.mqtt_password);
    response->print("\", \"mtopic\":\"");
    response->print(sett.mqtt_topic);
    response->print("\", \"mport\":\"");
    response->print(sett.mqtt_port);
    LOG_INFO(F("config IP"));
    response->print("\", \"mac\":\"");
    response->print(WiFi.macAddress());
    response->print("\", \"ip\":\"");
    response->print(printIP(sett.ip));
    response->print("\", \"gw\":\"");
    response->print(printIP(sett.gateway));
    response->print("\", \"sn\":\"");
    response->print(printIP(sett.mask));
    LOG_INFO(F("config counters"));
    response->print("\", \"mperiod\":\"");
    response->print(sett.wakeup_per_min);
    response->print("\", \"factorCold\":\"");
    response->print(sett.factor1);
    response->print("\", \"factorHot\":\"");
    response->print(sett.factor0);
    response->print("\", \"serialCold\":\"");
    response->print(sett.serial0);
    response->print("\", \"serialHot\":\"");
    response->print(sett.serial1);
    response->print("\", \"ch0\":\"");
    response->print(cdata.channel0, 3);
    response->print("\", \"ch1\":\"");
    response->print(cdata.channel1, 3);
    response->print("\"}");
    request->send(response);
};

void Portal::onGetStates(AsyncWebServerRequest *request)
{
    LOG_INFO(F("AsyncWebServer GET /states"));
    SlaveData runtime_data;
    String message;
    if (masterI2C.getSlaveData(runtime_data))
    {
        String state0good(F("\"\""));
        String state0bad(F("\"Не подключён\""));
        String state1good(F("\"\""));
        String state1bad(F("\"Не подключён\""));

        uint32_t delta0 = runtime_data.impulses0 - data.impulses0;
        uint32_t delta1 = runtime_data.impulses1 - data.impulses1;

        if (delta0 > 0)
        {
            state0good = F("\"Подключён\"");
            state0bad = F("\"\"");
        }
        if (delta1 > 0)
        {
            state1good = F("\"Подключён\"");
            state1bad = F("\"\"");
        }

        message = F("{\"state0good\": ");
        message += state0good;
        message += F(", \"state0bad\": ");
        message += state0bad;
        message += F(", \"state1good\": ");
        message += state1good;
        message += F(", \"state1bad\": ");
        message += state1bad;
        message += F(", \"elapsed\": ");
        message += String((uint32_t)(SETUP_TIME_SEC - millis() / 1000.0));
        message += F(", \"factor_cold_feedback\": ");
        message += String(get_auto_factor(runtime_data.impulses1, data.impulses1));
        message += F(", \"factor_hot_feedback\": ");
        message += String(get_auto_factor(runtime_data.impulses0, data.impulses0));
        message += F(", \"error\": \"");
        message += F("\", \"fail\":\"");
        message += _fail ? "1" : "";
        message += F("\"}");
    }
    else
    {
        message = F("{\"error\": \"Ошибка связи с МК\", \"factor_cold_feedback\": 1, \"factor_hot_feedback\": 1}");
    };
    request->send(200, "", message);
};

bool Portal::UpdateParamStr(AsyncWebServerRequest *request, const char *param_name, char *dest, size_t size)
{
    if (request->hasParam(param_name, true))
    {
        if (strcmp(dest, request->getParam(param_name, true)->value().c_str()) != 0)
        {
            return SetParamStr(request, param_name,dest,size);
        }else{
            LOG_INFO(F("No modify ") << param_name << F("=") << dest);
            return false;
        }
    }
    _fail = true;
    LOG_INFO(F("Fail ") << param_name);
    return false;
}

bool Portal::SetParamStr(AsyncWebServerRequest *request, const char *param_name, char *dest, size_t size)
{
    if (request->hasParam(param_name, true))
    {
        strncpy0(dest, request->getParam(param_name, true)->value().c_str(), size);
        LOG_INFO(F("Save ") << param_name << F("=") << dest);
        return true;
    }
    _fail = true;
    LOG_INFO(F("Fail ") << param_name);
    return false;
}

bool Portal::SetParamIP(AsyncWebServerRequest *request, const char *param_name, uint32_t *dest)
{
    if (request->hasParam(param_name, true))
    {
        IPAddress ip;
        if (ip.fromString(request->getParam(param_name, true)->value()))
        {
            *dest = ip.v4();
            LOG_INFO(F("Save ") << param_name << F("=") << ip.toString());
            return true;
        }
    }
    _fail = true;
    LOG_INFO(F("Fail ") << param_name);
    return false;
}

bool Portal::SetParamUInt(AsyncWebServerRequest *request, const char *param_name, uint16_t *dest)
{
    if (request->hasParam(param_name, true))
    {

        *dest = (uint16_t)(request->getParam(param_name, true)->value().toInt());
        LOG_INFO(F("Save ") << param_name << F("=") << *dest);
        return true;
    }
    _fail = true;
    LOG_INFO(F("Fail ") << param_name);
    return false;
}

bool Portal::SetParamByte(AsyncWebServerRequest *request, const char *param_name, uint8_t *dest)
{
    if (request->hasParam(param_name, true))
    {
        *dest = (uint8_t)(request->getParam(param_name, true)->value().toInt());
        LOG_INFO(F("Save ") << param_name << F("=") << *dest);
        return true;
    }
    _fail = true;
    LOG_INFO(F("Fail ") << param_name);
    return false;
}

bool Portal::SetParamFloat(AsyncWebServerRequest *request, const char *param_name, float *dest)
{
    if (request->hasParam(param_name, true))
    {
        *dest = (uint8_t)(request->getParam(param_name, true)->value().toFloat());
        LOG_INFO(F("Save ") << param_name << F("=") << *dest);
        return true;
    }
    _fail = true;
    LOG_INFO(F("Fail ") << param_name);
    return false;
}

void Portal::onPostWifiSave(AsyncWebServerRequest *request)
{
    LOG_INFO(F("AsyncWebServer POST /wifisave"));
    _fail = false;
    if (UpdateParamStr(request, PARAM_WMAIL, sett.waterius_email, EMAIL_LEN))
    {
        WateriusHttps::generateSha256Token(sett.waterius_key, WATERIUS_KEY_LEN, sett.waterius_email);
    }
    SetParamStr(request, PARAM_WHOST, sett.waterius_host, WATERIUS_HOST_LEN);

    SetParamStr(request, PARAM_BKEY, sett.blynk_key, BLYNK_KEY_LEN);
    SetParamStr(request, PARAM_BHOST, sett.blynk_host, BLYNK_HOST_LEN);
    SetParamStr(request, PARAM_BMAIL, sett.blynk_email, EMAIL_LEN);
    SetParamStr(request, PARAM_BTITLE, sett.blynk_email_title, BLYNK_EMAIL_TITLE_LEN);
    SetParamStr(request, PARAM_BTEMPLATE, sett.blynk_email_template, BLYNK_EMAIL_TEMPLATE_LEN);

    SetParamStr(request, PARAM_MHOST, sett.mqtt_host, MQTT_HOST_LEN);
    SetParamStr(request, PARAM_MLOGIN, sett.mqtt_login, MQTT_LOGIN_LEN);
    SetParamStr(request, PARAM_MPASSWORD, sett.mqtt_password, MQTT_PASSWORD_LEN);
    SetParamStr(request, PARAM_MTOPIC, sett.mqtt_topic, MQTT_TOPIC_LEN);
    SetParamUInt(request, PARAM_MPORT, &sett.mqtt_port);

    SetParamIP(request, PARAM_IP, &sett.ip);
    SetParamIP(request, PARAM_GW, &sett.gateway);
    SetParamIP(request, PARAM_SN, &sett.mask);

    SetParamUInt(request, PARAM_MPERIOD, &sett.wakeup_per_min);

    uint8_t combobox_factor = -1;
    if (SetParamByte(request, PARAM_FACTORCOLD, &combobox_factor))
    {
        sett.factor1 = get_factor(combobox_factor, data.impulses1, data.impulses1, 1);
        LOG_INFO("cold dropdown=" << combobox_factor);
        LOG_INFO("factorCold=" << sett.factor1);
    }
    if (SetParamByte(request, PARAM_FACTORHOT, &combobox_factor))
    {
        sett.factor0 = get_factor(combobox_factor, data.impulses1, data.impulses1, 1);
        LOG_INFO("hot dropdown=" << combobox_factor);
        LOG_INFO("factorHot=" << sett.factor0);
    }
    SetParamStr(request, PARAM_SERIALCOLD, sett.serial1, SERIAL_LEN);
    SetParamStr(request, PARAM_SERIALHOT, sett.serial0, SERIAL_LEN);

    if (SetParamFloat(request, PARAM_CH0, &sett.channel0_start))
    {
        sett.impulses0_start = data.impulses0;
        sett.impulses0_previous = sett.impulses0_start;
        LOG_INFO("impulses0=" << sett.impulses0_start);
    }
    if (SetParamFloat(request, PARAM_CH1, &sett.channel1_start))
    {
        sett.impulses1_start = data.impulses1;
        sett.impulses1_previous = sett.impulses1_start;
        LOG_INFO("impulses1=" << sett.impulses1_start);
    }

    // Запоминаем кол-во импульсов Attiny соответствующих текущим показаниям счетчиков
    if (!_fail)
    {
        sett.crc = FAKE_CRC;
        storeConfig(sett);
        _donesettings = true;
        request->send(200, "", "ok");
    }
    else
    {
        // request->send(LittleFS, "/fail.html");
        request->redirect("/");
    }
};

void Portal::onNotFound(AsyncWebServerRequest *request)
{
    request->send(404);
};

bool Portal::doneettings()
{
    return _donesettings;
}

void Portal::begin()
{
    _donesettings = false;
    _fail = false;
    server->begin();
}

void Portal::end()
{
    server->end();
}

Portal::Portal()
{
    _donesettings = false;
    _fail=false;
    /* web server*/
    server = new AsyncWebServer(80);
    server->on("/", HTTP_GET, std::bind(&Portal::onGetRoot, this, std::placeholders::_1));
    server->on("/script.js", HTTP_GET, std::bind(&Portal::onGetScript, this, std::placeholders::_1));
    server->on("/networks", HTTP_GET, std::bind(&Portal::onGetNetworks, this, std::placeholders::_1));
    server->on("/wifisave", HTTP_POST, std::bind(&Portal::onPostWifiSave, this, std::placeholders::_1));
    server->on("/states", HTTP_GET, std::bind(&Portal::onGetStates, this, std::placeholders::_1));
    server->on("/config", HTTP_GET, std::bind(&Portal::onGetConfig, this, std::placeholders::_1));
    server->onNotFound(std::bind(&Portal::onNotFound, this, std::placeholders::_1));
}

Portal::~Portal()
{
    server->~AsyncWebServer();
}