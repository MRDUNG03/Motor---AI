#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <INA226.h>                     


// ==================== CẤU HÌNH ====================
const char* ssid = "Villa_3lau";
const char* password = "23456778";
// const char* ssid = "iPhone";
// const char* password = "12345678";
const char* serverURL = "http://192.168.1.16:8000/sensor_batch";

// ADXL335
#define PIN_X 32
#define PIN_Y 33
#define PIN_Z 34
const float Vref = 3.3f;
const float zeroG = Vref / 2;
const float sensitivity = 0.33f;

// ACS712 - CHUẨN CỦA BẠN NHƯNG SIÊU NHANH
#define PIN_ACS712 35
const float V_ZERO      = 2.640f;
const float SENSITIVITY = 0.215f;   // 215 mV/A
// === THÊM VÀO ĐẦU FILE ===
const float VOLTAGE_CAL_FACTOR = 24.00f / 25.05f;   // CHỈNH GIÁ TRỊ NÀY MỘT LẦN
// I2C
INA226 ina226(0x40);   // địa chỉ mặc định
Adafruit_MLX90614 mlx = Adafruit_MLX90614();
uint8_t ina226_addr = 0;
uint8_t mlx_addr = 0;

// Buffer
#define BUFFER_SIZE 512
struct Sample {
  float ax, ay, az;
  float current;      // Ampere
  float voltage;
  float temp;
};
Sample buffer[BUFFER_SIZE];
uint16_t idx = 0;

// ==================== ACS712 NHANH + CHUẨN (chỉ 1 lần đọc + lọc mượt) ====================
float readCurrentACS712() {
  static float smooth = 0.0f;
  int raw = analogRead(PIN_ACS712);
  float v = raw * (3.3f / 4095.0f);
  float i = (v - V_ZERO) / SENSITIVITY;

  smooth = smooth * 0.94f + i * 0.06f;   // lọc nhiễu cực mạnh, vẫn theo kịp thay đổi nhanh

  if (smooth < 0.070f) smooth = 0.0f;
  if (smooth < 0.0f)   smooth = 0.0f;

  return smooth;
}

// ==================== ADXL335 ====================
float readG(int pin) {
  int raw = analogRead(pin);
  float v = raw * Vref / 4095.0f;
  float g = (v - zeroG) / sensitivity;
  if (pin == PIN_Z) g = -g;
  return roundf(g * 1000) / 1000.0f;
}

// ==================== SCAN I2C ====================
void scanI2C() {
  Serial.println("Scanning I2C...");
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("Found I2C 0x%02X\n", addr);
      if (addr == 0x40 || addr == 0x44 || addr == 0x45) ina226_addr = addr;
      if (addr >= 0x5A && addr <= 0x7F) mlx_addr = addr;
    }
  }
}

// ==================== ĐỌC TẤT CẢ ====================
void readAll() {
  buffer[idx].ax = readG(PIN_X);
  buffer[idx].ay = readG(PIN_Y);
  buffer[idx].az = readG(PIN_Z);

  buffer[idx].current = readCurrentACS712();
  // buffer[idx].voltage = ina226.getBusVoltage();
  float raw_voltage = ina226.getBusVoltage();
  buffer[idx].voltage = raw_voltage * VOLTAGE_CAL_FACTOR;   // ← ĐÃ CHUẨN 24.00V
  buffer[idx].temp    = mlx.readObjectTempC();
  if (isnan(buffer[idx].temp)) buffer[idx].temp = -999.0f;

  idx++;
}

// ==================== GỬI BATCH ====================
void sendBatch() {
  if (idx == 0) return;

  DynamicJsonDocument doc(24576);
  JsonArray arr = doc.to<JsonArray>();

  for (uint16_t i = 0; i < idx; i++) {
    JsonObject obj = arr.add<JsonObject>();
    obj["ax"] = buffer[i].ax;
    obj["ay"] = buffer[i].ay;
    obj["az"] = buffer[i].az;
    obj["current"] = roundf(buffer[i].current * 1000.0f) / 1000.0f;
    obj["voltage"] = roundf(buffer[i].voltage * 1000.0f) / 1000.0f;
    obj["temp"]    = roundf(buffer[i].temp * 100.0f) / 100.0f;
  }

  String payload;
  serializeJson(doc, payload);

  HTTPClient http;
  http.begin(serverURL);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(payload);
  Serial.printf("Sent %d samples -> %s (code %d)\n", idx, (code == 200) ? "OK" : "FAIL", code);
  http.end();

  idx = 0;
}

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_ACS712, ADC_11db);

  Wire.begin(19, 22);
  scanI2C();

  if (ina226_addr) {
    ina226 = INA226(ina226_addr);
    if (ina226.begin()) Serial.println("INA226 OK");
    else { Serial.println("INA226 FAIL"); while(1); }
  }

  if (mlx_addr) mlx.begin(mlx_addr);
  else mlx.begin();
  if (mlx.begin()) Serial.println("MLX90614 OK");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }
  Serial.println("\nWiFi OK: " + WiFi.localIP().toString());
  Serial.println("2000Hz RUNNING - ACS712 CHUẨN + SIÊU NHANH");
}

// ==================== LOOP 2000Hz CHUẨN ====================
void loop() {
  static uint32_t last = micros();
  uint32_t now = micros();

  if (now - last >= 500) {        // chính xác 2000 Hz
    last += 500;
    readAll();

    if (idx >= BUFFER_SIZE) {
      sendBatch();
    }
  }
}