#ifndef CLOUD_CLIENT_H
#define CLOUD_CLIENT_H

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "config.h"

class CloudClient {
public:
  CloudClient() {
    _client.setCACert(ROOT_CA);
  }

  String getAIResponse(String userInput) {
    if (WiFi.status() != WL_CONNECTED) return "WiFi Disconnected";

    HTTPClient http;
    http.begin(_client, "https://api.openai.com/v1/chat/completions");
    http.addHeader("Content-Type", "application/json");
    String authHeader = "Bearer " + String(OPENAI_API_KEY);
    http.addHeader("Authorization", authHeader);

    StaticJsonDocument<512> doc;
    doc["model"] = "gpt-4o-mini";
    JsonArray messages = doc.createNestedArray("messages");
    JsonObject systemMsg = messages.createNestedObject();
    systemMsg["role"] = "system";
    systemMsg["content"] = "You are Mochi, a friendly table robot assistant. Keep responses short and concise.";
    
    JsonObject userMsg = messages.createNestedObject();
    userMsg["role"] = "user";
    userMsg["content"] = userInput;

    String requestBody;
    serializeJson(doc, requestBody);

    int httpResponseCode = http.POST(requestBody);
    String response = "Error";

    if (httpResponseCode > 0) {
      String payload = http.getString();
      StaticJsonDocument<1024> responseDoc;
      deserializeJson(responseDoc, payload);
      response = responseDoc["choices"][0]["message"]["content"].as<String>();
    } else {
      Serial.print("Error on HTTP request: ");
      Serial.println(httpResponseCode);
    }

    http.end();
    return response;
  }

  String sendAudioToSTT(uint8_t* audioData, size_t size) {
    if (WiFi.status() != WL_CONNECTED) return "";

    HTTPClient http;
    // Using Deepgram's pre-recorded API for simplicity in this step, 
    // though streaming is better for latency.
    http.begin(_client, "https://api.deepgram.com/v1/listen?model=nova-2&smart_format=true");
    http.addHeader("Authorization", "Token " + String(DEEPGRAM_API_KEY));
    http.addHeader("Content-Type", "audio/wav"); // Assuming wav for now

    int httpResponseCode = http.POST(audioData, size);
    String transcript = "";

    if (httpResponseCode > 0) {
      String payload = http.getString();
      StaticJsonDocument<1024> doc;
      deserializeJson(doc, payload);
      transcript = doc["results"]["channels"][0]["alternatives"][0]["transcript"].as<String>();
    } else {
      Serial.print("STT Error: ");
      Serial.println(httpResponseCode);
    }

    http.end();
    return transcript;
  }

private:
  WiFiClientSecure _client;
};

#endif
