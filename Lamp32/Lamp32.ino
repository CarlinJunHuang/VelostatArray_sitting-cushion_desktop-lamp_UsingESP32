#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <FastLED.h>

#define NUM_LEDS 20
#define DATA_PIN 18  // 使用合适的GPIO引脚
#define BRIGHTNESS 128  // 设置亮度，范围是0-255
#define SSID "SDIM_DQ"
#define WIFI_PASSWORD "*****"
#define mqttServer "*****.mqtt.iothub.aliyuncs.com"
#define mqttPort 1883
#define ClientId "*****.ESP8266_lamp|securemode=2,signmethod=hmacsha256,timestamp=*****|"
#define User "ESP8266_lamp&*****"
#define Pass "**********"
#define TOPIC "/*****/ESP8266_lamp/user/get"

CRGB leds[NUM_LEDS];

WiFiClient espClient;
PubSubClient client(espClient);

// 全局变量
unsigned long lastStateChangeTime = 0;
bool lastStatus = false; // false 表示站起，true 表示坐下
bool breathingMode = false;
bool lightsOffDueToStanding = false;
unsigned long lastBreathUpdate = 0;
int breathingBrightness = 0;
bool increasing = true;
bool stopBreathingAfterStanding = false;
bool inCenter = true;
float cop_x = 4.5;
float cop_y = 4.5;

// 连接WiFi
void setup_wifi() {
    delay(10);
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(SSID);

    WiFi.begin(SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
}

// 计算方位角并映射到LED灯珠索引
void mapToLED(float cop_x, float cop_y) {
    float offset = sqrt(pow(cop_x - 4.5, 2) + pow(cop_y - 4.5, 2));
    Serial.print("Offset: ");
    Serial.println(offset);
    FastLED.clear();

    if (offset > 1) {
        float angle = atan2(cop_y - 4.5, cop_x - 4.5) * (180 / PI);
        int angleInt = static_cast<int>(angle + 360) % 360;
        int ledIndex = round(angleInt / (360 / NUM_LEDS));
        Serial.print("Angle: ");
        Serial.println(angle);
        Serial.print("LED Index: ");
        Serial.println(ledIndex);
        leds[ledIndex] = CRGB::Red; // 设置超出位置的灯为红色
    }
}

void callback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (int i = 0; i < length; i++) {
        Serial.print((char)payload[i]);
    }
    Serial.println();

    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload, length);

    if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return;
    }

    // Be careful to make sure if you need to unpack your parms first, or just postureData (for sure, this one is also defined by you in ur cloud JSON format.)
    
    // JsonObject params = doc["params"];
    // JsonObject postureData = params["postureData"];

    JsonObject postureData = doc["postureData"];

    int posture = postureData["posture"];
    cop_x = postureData["COP_X"];
    cop_y = postureData["COP_Y"];
    bool sittingstatus = postureData["Status"];
    int duration = postureData["duration"];

    Serial.print("Posture: ");
    Serial.println(posture);
    Serial.print("COP_X: ");
    Serial.println(cop_x);
    Serial.print("COP_Y: ");
    Serial.println(cop_y);
    Serial.print("Status: ");
    Serial.println(sittingstatus ? "Sitting" : "Standing");
    Serial.print("Duration: ");
    Serial.println(duration);

    if (sittingstatus != lastStatus) {
        lastStateChangeTime = millis();
        lastStatus = sittingstatus;
        lightsOffDueToStanding = false;
        stopBreathingAfterStanding = false;  // 重置标志
    }

    inCenter = pow(cop_x - 4.5, 2) + pow(cop_y - 4.5, 2) <= 0.5;

    if (sittingstatus) {  // Sitting 状态
        if (!inCenter) {  // 如果不在中心区域，映射红光
            breathingMode = false;
            mapToLED(cop_x, cop_y);
            FastLED.show();
        }
        if (duration > 15) {
            breathingMode = true;
        }
    } else {  // Standing 状态
        if (duration > 10) {
            FastLED.clear();
            FastLED.show();
            lightsOffDueToStanding = true;
        } else if (duration > 5) {
            stopBreathingAfterStanding = true;  // 设置标志
            Serial.println("yeah");
        }
    }

    if (!lightsOffDueToStanding) {
        if (!breathingMode && !sittingstatus) {  // 如果不处于呼吸灯模式且是站姿状态
            FastLED.setBrightness(BRIGHTNESS);
            fill_solid(leds, NUM_LEDS, CRGB(255, 223, 186)); // 护眼颜色
            FastLED.show();
        } else if (sittingstatus) {  // 坐姿状态
            if (inCenter) {
                FastLED.setBrightness(BRIGHTNESS);
                fill_solid(leds, NUM_LEDS, CRGB(255, 223, 186)); // 护眼颜色
                FastLED.show();
            }
        }
    }
}


// 呼吸灯效果函数
void updateBreathingEffect() {
    if (breathingMode && !stopBreathingAfterStanding && inCenter) {
        unsigned long currentMillis = millis();
        if (currentMillis - lastBreathUpdate >= 20) {
            lastBreathUpdate = currentMillis;
            if (increasing) {
                breathingBrightness += 1;
                if (breathingBrightness >= 128) {
                    increasing = false;
                }
            } else {
                breathingBrightness -= 1;
                if (breathingBrightness <= 0) {
                    increasing = true;
                }
            }
            FastLED.setBrightness(breathingBrightness);
            fill_solid(leds, NUM_LEDS, CRGB(255, 223, 186)); // 护眼颜色
            FastLED.show();
        }
    }
}


// 连接MQTT服务器
void setupMQTT() {
    client.setServer(mqttServer, mqttPort);
    client.setCallback(callback); 
    while (!client.connected()) {
        Serial.println("Connecting MQTT");
        
        if (client.connect(ClientId, User, Pass)) {
            Serial.println("MQTT connected successfully!");
            client.subscribe(TOPIC);
        } else {
            Serial.print("Failed with state ");
            Serial.println(client.state());
            delay(2000);
        }
    }
}

void setup() {
    Serial.begin(115200);
    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
    FastLED.setBrightness(BRIGHTNESS);
    setup_wifi();
    setupMQTT();
}

void loop() {
    if (!client.connected()) {
        setupMQTT();
    }
    client.loop();

    updateBreathingEffect();
}
