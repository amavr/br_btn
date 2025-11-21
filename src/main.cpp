#include <ESP8266WiFi.h>
#include <WebSocketsClient.h>

const char *ssid = "BrainRing";
const char *password = "12345678";

WebSocketsClient webSocket;
const int buttonPin = 0; // GPIO0
const int ledPin = 2;    // GPIO2

bool pressed = false;
bool isLeader = false;
String clientId = "";

void webSocketEvent(WStype_t type, uint8_t *payload, size_t length)
{
    switch (type)
    {
    case WStype_DISCONNECTED:
        Serial.println("Disconnected");
        break;

    case WStype_CONNECTED:
    {
        Serial.println("Connected to server");
        // Отправляем ID при подключении
        String connectMsg = "connect:" + clientId;
        webSocket.sendTXT(connectMsg);
        break;
    }

    case WStype_TEXT:
    {
        String message = String((char *)payload);
        Serial.println("Received: " + message);

        if (message == "reset")
        {
            digitalWrite(ledPin, HIGH);
            isLeader = false;
            pressed = false;
        }
        else if (message.startsWith("leader:"))
        {
            String leaderId = message.substring(7);
            if (leaderId == clientId)
            {
                digitalWrite(ledPin, LOW);
                isLeader = true;
                Serial.println("I am the leader!");
            }
        }
        break;
    }
    }
}

void setup()
{
    Serial.begin(115200);
    pinMode(buttonPin, INPUT_PULLUP);
    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, HIGH);

    // Генерируем уникальный ID
    clientId = "esp_" + String(ESP.getChipId());
    Serial.println("My ID: " + clientId);

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected to WiFi");

    webSocket.begin("192.168.4.1", 81, "/");
    webSocket.onEvent(webSocketEvent);
    webSocket.setReconnectInterval(3000);
}

void loop()
{
    webSocket.loop();

    if (!pressed && digitalRead(buttonPin) == LOW)
    // if (!pressed && (digitalRead(buttonPin) == LOW || random(10) > 7))
    {
        pressed = true;
        delay(50); // Антидребезг

        if (webSocket.isConnected() && !isLeader)
        {
            // Создаем строку отдельно и затем отправляем
            String pressMessage = "press:" + clientId;
            webSocket.sendTXT(pressMessage);
            Serial.println("Sent: " + pressMessage);
        }

        // Ждем отпускания кнопки
        while (digitalRead(buttonPin) == LOW)
        {
            delay(10);
        }
        pressed = false;
    }

    delay(10);
}