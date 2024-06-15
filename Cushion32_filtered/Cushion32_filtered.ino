#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#define WIFI_SSID "SDIM_DQ"
#define WIFI_PASSWORD "*****"
#define MQTT_SERVER "*****.mqtt.iothub.aliyuncs.com"
#define MQTT_PORT 1883
#define MQTT_CLIENT_ID "*****.ESP32_sittingcushion|securemode=2,signmethod=hmacsha256,timestamp=*****|"
#define MQTT_USERNAME "ESP32_sittingcushion&*****"
#define MQTT_PASSWORD "********"
#define MQTT_TOPIC "/*****/ESP32_sittingcushion/user/update"

WiFiClient espClient;
PubSubClient client(espClient);

String inputString = "";  // A string to hold incoming data
bool stringComplete = false;  // Whether the string is complete

float cop_x = 0;
float cop_y = 0;
bool status = false;
bool prevStatus = false; // 用于记录前一帧的状态
unsigned long lastStateChangeTime = 0;
unsigned long duration = 0;

// 分界点
const float threshold = 850.0;
const int frameThreshold = 2;  // 连续帧阈值
int frameCount = 0;  // 连续帧计数

// 卡尔曼滤波器的参数
float Q = 0.01;  // 过程噪声协方差
float R = 1.0;   // 测量噪声协方差
float P = 1.0;   // 估计误差协方差
float K = 0.0;   // 卡尔曼增益
float filteredValue = 0.0;  // 滤波后的值

void setup_wifi() {
    Serial.begin(115200);
    Serial2.begin(9600); // 初始化与Arduino的串口通信，使用Serial2
    delay(10);
    Serial.print("Connecting to ");
    Serial.println(WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("WiFi connected");
}

void setup_mqtt() {
    client.setServer(MQTT_SERVER, MQTT_PORT);
    while (!client.connected()) {
        Serial.println("Connecting to MQTT...");
        if (client.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) {
            Serial.println("MQTT connected");
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            delay(2000);
        }
    }
}

// 卡尔曼滤波函数
float kalmanFilter(float value) {
    P = P + Q;  // 更新估计误差协方差
    K = P / (P + R);  // 计算卡尔曼增益
    filteredValue = filteredValue + K * (value - filteredValue);  // 更新估计值
    P = (1 - K) * P;  // 更新估计误差协方差
    return filteredValue;
}

void calculateCOP(float matrix[9][9], float &cop_x, float &cop_y) {
    float sum = 0, sum_x = 0, sum_y = 0;
    for (int i = 0; i < 9; i++) {
        for (int j = 0; j < 9; j++) {
            float filteredValue = kalmanFilter(matrix[i][j]/1000.0);  // 使用卡尔曼滤波处理数据
            sum += matrix[i][j]/1000;
            sum_x += i * matrix[i][j]/1000;
            sum_y += j * matrix[i][j]/1000;
        }
    }
    if (sum == 0) {
        cop_x = 0;
        cop_y = 0;
    } else {
        cop_x = sum_x / sum + 0.5;
        cop_y = sum_y / sum + 0.5;
    }
}

void sendDataToAliyun(float cop_x, float cop_y, bool status, unsigned long duration) {
    StaticJsonDocument<200> doc;
    JsonObject postureData = doc.createNestedObject("postureData");
    postureData["posture"] = 1;
    postureData["COP_X"] = cop_x;
    postureData["COP_Y"] = cop_y;
    postureData["Status"] = status ? 1 : 0;
    postureData["duration"] = duration;

    char buffer[256];
    serializeJson(doc, buffer);
    client.publish(MQTT_TOPIC, buffer);
}

void serialEvent2() {
    while (Serial2.available()) {
        char inChar = (char)Serial2.read();
        inputString += inChar;
        if (inChar == '\n') {
            stringComplete = true;
        }
    }
}

void setup() {
    Serial.begin(115200);
    setup_wifi();
    setup_mqtt();
    inputString.reserve(200); // reserve 200 bytes for the inputString
}

void loop() {
    if (!client.connected()) {
        setup_mqtt();
    }
    client.loop();

    // 发送触发信号给Arduino
    Serial2.println("TRIGGER"); // 发送触发信号

    if (stringComplete) {
        Serial.println("Received data: " + inputString);  // 打印接收到的数据以进行调试
        float matrix[9][9];
        int index = 0;
        char inputArray[inputString.length() + 1];
        inputString.toCharArray(inputArray, inputString.length() + 1);
        char *token = strtok(inputArray, "\t");
        while (token != nullptr) {
            matrix[index / 9][index % 9] = atof(token);
            index++;
            token = strtok(nullptr, "\t");
        }
        calculateCOP(matrix, cop_x, cop_y);

        if (isnan(cop_x) || isnan(cop_y) || isinf(cop_x) || isinf(cop_y)) {
            // 如果COP值无效，跳过这一帧
            Serial.println("Invalid COP values, skipping this frame.");
            inputString = "";
            stringComplete = false;
            return;
        }

        Serial.print("COP_X: ");
        Serial.println(cop_x);
        Serial.print("COP_Y: ");
        Serial.println(cop_y);

        // 计算平均值
        float average = 0;
        for (int i = 0; i < 9; i++) {
            for (int j = 0; j < 9; j++) {
                average += matrix[i][j];
            }
        }
        average /= 81;

        // 更新持续时间
        unsigned long currentTime = millis();
        duration = (currentTime - lastStateChangeTime) / 1000;

        // 使用平均值判断状态
        prevStatus = status;
        if (status) {
            if (average > threshold) {
                frameCount++;
            } else {
                frameCount = 0;
            }
        } else {
            if (average <= threshold) {
                frameCount++;
            } else {
                frameCount = 0;
            }
        }

        if (frameCount >= frameThreshold) {
            // 如果状态切换达到帧阈值，更新状态和时间
            status = !status;
            lastStateChangeTime = currentTime;
            frameCount = 0; // 重置帧计数
        }

        Serial.print("Status: ");
        Serial.println(status ? "Sitting" : "Standing");
        Serial.print("Duration: ");
        Serial.println(duration);

        // 发送数据到阿里云
        sendDataToAliyun(cop_x, cop_y, status, duration);

        // 清除inputString并重置stringComplete
        inputString = "";
        stringComplete = false;
    }

    delay(600); // 添加延迟以防止紧密循环导致WDT重置
}
