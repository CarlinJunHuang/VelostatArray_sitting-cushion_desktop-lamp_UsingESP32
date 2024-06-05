#include <Adafruit_NeoPixel.h>

#define PIN 5
#define NUM_LEDS 20
#define BRIGHTNESS 50 // 亮度可以根据需要调整

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.show(); // 初始化所有LED为关闭状态
}

void loop() {
  // 逐颗点亮跑马灯效果
  for(int i=0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(255, 0, 0)); // 设置当前LED为红色
    strip.show();
    delay(1000 / NUM_LEDS); // 每秒钟切换一次
    strip.setPixelColor(i, strip.Color(0, 0, 0)); // 关闭当前LED
  }
}
