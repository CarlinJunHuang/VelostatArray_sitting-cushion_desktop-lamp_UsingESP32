#include <tflm_esp32.h>
#include <EloquentTinyML.h>
#include <eloquent_tinyml/tensorflow.h>
#include "posture_model.h"  // 包含生成的模型头文件
#include <exception.h>
#include <utility.h>
#include <SoftwareSerial.h>
#include <Arduino.h>

// 模型相关参数
#define NUMBER_OF_INPUTS 81
#define NUMBER_OF_OUTPUTS 7
#define TENSOR_ARENA_SIZE 2 * 1024

Eloquent::TinyML::TfLite<NUMBER_OF_INPUTS, NUMBER_OF_OUTPUTS, TENSOR_ARENA_SIZE> tf_model;

// 滑动窗口参数
const int window_size = 5; // 窗口大小
float window_data[window_size][NUMBER_OF_INPUTS];
int window_index = 0;
bool window_filled = false;

// 串行通信
SoftwareSerial mySerial(2, 3); // RX, TX

// 计算均值和标准差
void calculate_mean_std(float *mean, float *std) {
    for (int j = 0; j < NUMBER_OF_INPUTS; j++) {
        mean[j] = 0;
        std[j] = 0;
    }

    int count = window_filled ? window_size : window_index;

    for (int i = 0; i < count; i++) {
        for (int j = 0; j < NUMBER_OF_INPUTS; j++) {
            mean[j] += window_data[i][j];
        }
    }
    for (int j = 0; j < NUMBER_OF_INPUTS; j++) {
        mean[j] /= count;
    }

    for (int i = 0; i < count; i++) {
        for (int j = 0; j < NUMBER_OF_INPUTS; j++) {
            std[j] += (window_data[i][j] - mean[j]) * (window_data[i][j] - mean[j]);
        }
    }
    for (int j = 0; j < NUMBER_OF_INPUTS; j++) {
        std[j] = sqrt(std[j] / count);
    }
}

// 标准化数据
void standardize(float *input, float *mean, float *std) {
    for (int i = 0; i < NUMBER_OF_INPUTS; i++) {
        input[i] = (input[i] - mean[i]) / std[i];
    }
}

void setup() {
    Serial.begin(115200);
    mySerial.begin(9600);

    // 初始化EloquentTinyML
    tf_model.begin(posture_model_tflite);

    // 检查模型是否正确加载
    if (!tf_model.begin(posture_model_tflite)) {
        Serial.println("Failed to load the model");
        while (1);
    }
}

void loop() {
    if (mySerial.available()) {
        String data = mySerial.readStringUntil('\n');
        float input_data[NUMBER_OF_INPUTS];
        int index = 0;
        for (int i = 0; i < data.length(); i++) {
            if (data[i] == ',') {
                input_data[index] = data.substring(0, i).toFloat();
                data = data.substring(i + 1);
                index++;
            }
        }
        input_data[index] = data.toFloat();

        // 将数据添加到滑动窗口
        for (int i = 0; i < NUMBER_OF_INPUTS; i++) {
            window_data[window_index][i] = input_data[i];
        }
        window_index = (window_index + 1) % window_size;
        if (window_index == 0) {
            window_filled = true;
        }

        // 计算均值和标准差
        float mean[NUMBER_OF_INPUTS];
        float std[NUMBER_OF_INPUTS];
        calculate_mean_std(mean, std);

        // 标准化数据
        standardize(input_data, mean, std);

        // 运行模型
        tf_model.predict(input_data);

        // 获取预测结果
        int predicted_posture = tf_model.predictedClass();

        // 打印结果
        Serial.print("Predicted Posture: ");
        Serial.println(predicted_posture);
    }
}
