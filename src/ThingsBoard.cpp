/*
  ThingsBoard.cpp - Library for sending data to the ThingsBoard.
  Based on PubSub MQTT library.
  Created by Olender M. Oct 2018.
  Released into the public domain.
  https://thingsboard.io/
*/

#include "ThingsBoard.h"

/*----------------------------------------------------------------------------*/

bool Telemetry::serializeKeyval(JsonVariant &jsonObj) const {
  if (m_key) {
    // Little trick to make sure all key assignment will work as expected
    JsonObject obj = jsonObj.to<JsonObject>();

    switch (m_type) {
      case TYPE_BOOL:
        obj[m_key] = m_value.boolean;
      break;
      case TYPE_INT:
        obj[m_key] = m_value.integer;
      break;
      case TYPE_REAL:
        obj[m_key] = m_value.real;
      break;
      case TYPE_STR:
        obj[m_key] = m_value.str;
      break;
      default:
      break;
    }
  } else {
    switch (m_type) {
      case TYPE_BOOL:
        jsonObj.set(m_value.boolean);
      break;
      case TYPE_INT:
        jsonObj.set(m_value.integer);
      break;
      case TYPE_REAL:
        jsonObj.set(m_value.real);
      break;
      case TYPE_STR:
        jsonObj.set(m_value.str);
      break;
      default:
      break;
    }
  }
  return true;
}

/*----------------------------------------------------------------------------*/

ThingsBoard *ThingsBoard::m_subscribedInstance;

bool ThingsBoard::connect(const char *host, const char *access_token, int port) {
  if (!host || !access_token) {
    return false;
  }
  this->RPC_Unsubscribe(); // Cleanup any subscriptions
  m_client.setServer(host, port);
  return m_client.connect("TbDev", access_token, NULL);
}

bool ThingsBoard::RPC_Subscribe(const RPC_Callback *callbacks, size_t callbacks_size) {
  // Too much callbacks
  if (callbacks_size > sizeof(m_rpcCallbacks) / sizeof(*m_rpcCallbacks)) {
    return false;
  }

  // MQTT PubSub library can call only stand-alone function.
  // Instance pointer is required to proxy RPC call to a user object.
  if (ThingsBoard::m_subscribedInstance) {
    return false;
  }

  if (!m_client.subscribe("v1/devices/me/rpc/request/+")) {
    return false;
  }

  ThingsBoard::m_subscribedInstance = this;
  for (size_t i = 0; i < callbacks_size; ++i) {
    m_rpcCallbacks[i] = callbacks[i];
  }

  m_client.setCallback(ThingsBoard::on_message);

  return true;
}

/*----------------------------------------------------------------------------*/

bool ThingsBoard::sendDataArray(const Telemetry *data, size_t data_count, bool telemetry) {
  char payload[64];
  {
    StaticJsonDocument<64> jsonBuffer;
    JsonVariant object = jsonBuffer.to<JsonVariant>();

    for (size_t i = 0; i < data_count; ++i) {
      data[i].serializeKeyval(object);
    }

    serializeJson(object, payload, sizeof(payload));
  }

  return telemetry ? sendTelemetryJson(payload) : sendAttributeJSON(payload);
}

void ThingsBoard::process_message(char* topic, uint8_t* payload, unsigned int length) {
  // Decode JSON request
  RPC_Response r;
  {
    StaticJsonDocument<100> jsonBuffer;
    DeserializationError error = deserializeJson(jsonBuffer, payload, length);
    if (error) {
      Serial.print("[SDK] unable to de-serialize RPC ");
      return;
    }
    const JsonObject &data = jsonBuffer.as<JsonObject>();

    const char *methodName = data["method"];

    if (methodName) {
      Serial.print("[SDK] received RPC ");
      Serial.println(methodName);
    } else {
      Serial.println("[SDK] RPC method is NULL");
      return;
    }

    for (size_t i = 0; i < sizeof(m_rpcCallbacks) / sizeof(*m_rpcCallbacks); ++i) {
      if (m_rpcCallbacks[i].m_cb && !strcmp(m_rpcCallbacks[i].m_name, methodName)) {

        Serial.print("[SDK] calling RPC ");
        Serial.println(methodName);

        // Do not inform client, if parameter field is missing for some reason
        if (!data.containsKey("params")) {
          Serial.println("[SDK] no parameters passed with RPC, passing null JSON");
        }
        // Getting non-existing field from JSON should automatically
        // set JSONVariant to null
        r = m_rpcCallbacks[i].m_cb(data["params"]);
        break;
      }
    }
  }
  {
    // Fill in response
    char payload[64] = {0};
    StaticJsonDocument<64> respBuffer;
    JsonVariant resp_obj = respBuffer.to<JsonVariant>();

    r.serializeKeyval(resp_obj);
    serializeJson(resp_obj, payload, sizeof(payload));

    String responseTopic = String(topic);
    responseTopic.replace("request", "response");
    Serial.print("[SDK] response ");
    Serial.println(payload);
    m_client.publish(responseTopic.c_str(), payload);
  }
}

void ThingsBoard::on_message(char* topic, uint8_t* payload, unsigned int length) {
  if (!ThingsBoard::m_subscribedInstance) {
    return;
  }

  ThingsBoard::m_subscribedInstance->process_message(topic, payload, length);
}

#ifndef ESP8266

bool ThingsBoardHttp::sendDataArray(const Telemetry *data, size_t data_count, bool telemetry) {
  char payload[64];
  {
    StaticJsonDocument<64> jsonBuffer;
    JsonVariant object = jsonBuffer.to<JsonVariant>();

    for (size_t i = 0; i < data_count; ++i) {
      data[i].serializeKeyval(object);
    }

    serializeJson(object, payload, sizeof(payload));
  }

  return telemetry ? sendTelemetryJson(payload) : sendAttributeJSON(payload);
}

#endif // ESP8266
