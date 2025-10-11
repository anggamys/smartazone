#include "mqtt_manager.h"

MqttManager::MqttManager(const char *wifiSsid,
                         const char *wifiPass,
                         const char *mqttServer,
                         uint16_t mqttPort,
                         const char *mqttUser,
                         const char *mqttPass)
    : ssid(wifiSsid),
      password(wifiPass),
      server(mqttServer),
      user(mqttUser),
      pass(mqttPass),
      port(mqttPort),
      mqttClient(wifiClient)
{
}

void MqttManager::begin()
{
    Serial.println("[MQTT] Starting WiFi...");
    connectWiFi();

    mqttClient.setServer(server, port);
    lastReconnectAttempt = 0;
}

void MqttManager::loop()
{
    if (!mqttClient.connected())
    {
        unsigned long now = millis();
        if (now - lastReconnectAttempt > RECONNECT_INTERVAL)
        {
            lastReconnectAttempt = now;
            if (connectMQTT())
                lastReconnectAttempt = 0;
        }
    }
    else
    {
        mqttClient.loop();
    }
}

bool MqttManager::isConnected() const
{
    return const_cast<PubSubClient &>(mqttClient).connected();
}

bool MqttManager::publish(const char *topic, const String &payload)
{
    if (!mqttClient.connected())
        return false;

    bool ok = mqttClient.publish(topic, payload.c_str());
    if (ok)
        Serial.printf("[MQTT] Published → %s: %s\n", topic, payload.c_str());
    else
        Serial.printf("[MQTT] Publish failed → %s\n", topic);
    return ok;
}

void MqttManager::connectWiFi()
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.printf("[WiFi] Connecting to %s", ssid);

    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 30)
    {
        delay(500);
        Serial.print(".");
        retry++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.printf("\n[WiFi] Connected. IP: %s\n", WiFi.localIP().toString().c_str());
    }
    else
    {
        Serial.println("\n[WiFi] Failed to connect!");
    }
}

bool MqttManager::connectMQTT()
{
    Serial.printf("[MQTT] Connecting to %s:%u ...\n", server, port);
    String clientId = "esp32-" + String((uint32_t)ESP.getEfuseMac(), HEX);

    bool connected = false;
    if (user && pass)
        connected = mqttClient.connect(clientId.c_str(), user, pass);
    else
        connected = mqttClient.connect(clientId.c_str());

    if (connected)
    {
        Serial.println("[MQTT] Connected");
        return true;
    }
    else
    {
        Serial.printf("[MQTT] Failed rc=%d\n", mqttClient.state());
        return false;
    }
}
