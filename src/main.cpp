#include <ESP8266WiFi.h>
#include <espnow.h>

// Пины для кнопки и светодиода на ESP-01
#define BUTTON_PIN 0 // GPIO0
#define LED_PIN 2    // GPIO2

// Состояние узла
bool registered = false;
String myColor = "";
bool gameStarted = false;
bool buttonPressed = false;
bool ledBlinking = false;
unsigned long ledBlinkTime = 0;
const unsigned long BLINK_DURATION = 3000;

// MAC адрес управляющего узла (будет установлен при получении discovery)
uint8_t controllerMac[6];
bool controllerMacSet = false;

unsigned long log_index = 0;

// Структура для сообщений
typedef struct struct_message
{
    char type[20];
    char color[10];
    uint8_t mac[6];
} struct_message;

struct_message incomingMessage;
struct_message outgoingMessage;

// Объявления функций
void setupESP_NOW();
void sendMessage(const char *type);
void processIncomingMessage(uint8_t *mac, uint8_t *incomingData, uint8_t len);
void handleButtonPress();
void blinkLED();

// Callback при получении данных
void OnDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len)
{
    processIncomingMessage(mac, incomingData, len);
}

void setup()
{
    Serial.begin(115200);

    // Инициализация пинов
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // Настройка WiFi и ESP-NOW
    WiFi.mode(WIFI_STA);
    setupESP_NOW();

    Serial.println("Узел-кнопка запущен. Ожидание регистрации...");
}

void setupESP_NOW()
{
    if (esp_now_init() != 0)
    {
        Serial.println("Ошибка инициализации ESP-NOW");
        return;
    }

    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    esp_now_register_recv_cb(OnDataRecv);
}

void loop()
{
    // Обработка нажатия кнопки
    if (registered && gameStarted && !buttonPressed)
    {
        handleButtonPress();
    }

    // Обработка мигания светодиода
    if (ledBlinking)
    {
        blinkLED();
    }

    delay(50);
}

void processIncomingMessage(uint8_t *mac, uint8_t *incomingData, uint8_t len)
{
    memcpy(&incomingMessage, incomingData, sizeof(incomingMessage));

    Serial.printf("%6ld Получено: ", ++log_index);
    Serial.println(incomingMessage.type);

    if (strcmp(incomingMessage.type, "discovery") == 0)
    {
        if (!registered)
        {
            // Сохранить MAC управляющего узла
            memcpy(controllerMac, incomingMessage.mac, 6);

            // Добавить управляющий узел как peer
            esp_now_add_peer(controllerMac, ESP_NOW_ROLE_COMBO, 1, NULL, 0);
            controllerMacSet = true;

            // Отправить запрос регистрации
            sendMessage("register_request");
            Serial.println("Отправлен запрос регистрации");
        }
    }
    else if (strcmp(incomingMessage.type, "register_confirm") == 0)
    {
        if (!registered)
        {
            registered = true;
            myColor = String(incomingMessage.color);

            // Включить светодиод для подтверждения регистрации
            digitalWrite(LED_PIN, HIGH);

            Serial.print("Зарегистрирован с цветом: ");
            Serial.println(myColor);
        }
    }
    else if (strcmp(incomingMessage.type, "start_game") == 0)
    {
        gameStarted = true;
        buttonPressed = false;

        // Выключить светодиод при начале игры
        digitalWrite(LED_PIN, LOW);

        Serial.println("Игра начата");
    }
    else if (strcmp(incomingMessage.type, "you_win") == 0)
    {
        // Включить светодиод при победе
        digitalWrite(LED_PIN, HIGH);
        buttonPressed = true;

        Serial.println("ПОБЕДА!");
    }
    else if (strcmp(incomingMessage.type, "next_round") == 0)
    {
        buttonPressed = false;

        // Запустить мигание светодиода на 3 секунды
        ledBlinking = true;
        ledBlinkTime = millis();
        digitalWrite(LED_PIN, HIGH);

        Serial.println("Новый раунд");
    }
}

void sendMessage(const char *type)
{
    if (!controllerMacSet)
        return;

    strcpy(outgoingMessage.type, type);

    uint8_t self_mac[6];
    WiFi.macAddress(self_mac);
    memcpy(outgoingMessage.mac, self_mac, 6);
    // memcpy(outgoingMessage.mac, WiFi.macAddress(), 6);

    esp_now_send(controllerMac, (uint8_t *)&outgoingMessage, sizeof(outgoingMessage));
}

void handleButtonPress()
{
    static bool lastButtonState = HIGH;
    bool currentButtonState = digitalRead(BUTTON_PIN);

    if (currentButtonState == LOW && lastButtonState == HIGH)
    {
        delay(50); // Антидребезг
        if (digitalRead(BUTTON_PIN) == LOW)
        {
            sendMessage("button_press");
            buttonPressed = true;
            Serial.println("Кнопка нажата");
        }
    }

    lastButtonState = currentButtonState;
}

void blinkLED()
{
    if (millis() - ledBlinkTime >= BLINK_DURATION)
    {
        digitalWrite(LED_PIN, LOW);
        ledBlinking = false;
    }
}