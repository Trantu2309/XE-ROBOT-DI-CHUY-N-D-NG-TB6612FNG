#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

// --- KHỐI 1: THÔNG TIN WIFI ---
const char* ssid     = "Happy House Q12";
const char* password = "28282828";

// --- CẤU HÌNH IP TĨNH (Để không phải tìm IP nữa) ---
IPAddress local_IP(192, 168, 1, 93);  // IP bạn muốn đặt cho xe
IPAddress gateway(192, 168, 1, 1);    // Thường là địa chỉ cục Wifi
IPAddress subnet(255, 255, 255, 0);

// --- KHỐI 3: CHÂN ĐIỀU KHIỂN TB6612FNG ---
const int PWMA = 25; const int AIN1 = 26; const int AIN2 = 27; // Trái
const int PWMB = 13; const int BIN1 = 14; const int BIN2 = 12; // Phải

// --- KHỐI 4: CHÂN ENCODER ---
const int ENC_L_A = 4;  const int ENC_L_B = 18;
const int ENC_R_A = 19; const int ENC_R_B = 21;

volatile long leftPulseCount = 0;
volatile long rightPulseCount = 0;

void IRAM_ATTR handleLeftEncoder() {
    if (digitalRead(ENC_L_B) == LOW) leftPulseCount++; else leftPulseCount--;
}
void IRAM_ATTR handleRightEncoder() {
    if (digitalRead(ENC_R_B) == LOW) rightPulseCount++; else rightPulseCount--;
}

WebServer server(80);

void setSpeed(int speed) {
    ledcWrite(0, speed);
    ledcWrite(1, speed);
}

void stopRobot() {
    digitalWrite(AIN1, LOW); digitalWrite(AIN2, LOW);
    digitalWrite(BIN1, LOW); digitalWrite(BIN2, LOW);
    setSpeed(0);
}

// Hàm thực tế làm xe chạy TIẾN
void realMoveForward() {
    digitalWrite(AIN1, HIGH); digitalWrite(AIN2, LOW);
    digitalWrite(BIN1, HIGH); digitalWrite(BIN2, LOW);
    setSpeed(220);
}

// Hàm thực tế làm xe chạy LÙI
void realMoveBackward() {
    digitalWrite(AIN1, LOW);  digitalWrite(AIN2, HIGH);
    digitalWrite(BIN1, LOW);  digitalWrite(BIN2, HIGH);
    setSpeed(220);
}

// Hàm thực tế làm xe XOAY TRÁI
void realTurnLeft() {
    digitalWrite(AIN1, LOW);  digitalWrite(AIN2, HIGH);
    digitalWrite(BIN1, HIGH); digitalWrite(BIN2, LOW);
    setSpeed(200);
}

// Hàm thực tế làm xe XOAY PHẢI
void realTurnRight() {
    digitalWrite(AIN1, HIGH); digitalWrite(AIN2, LOW);
    digitalWrite(BIN1, LOW);  digitalWrite(BIN2, HIGH);
    setSpeed(200);
}

void execute(void (*func)()) {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", "OK");
    func();
}

void setup() {
    Serial.begin(115200);

    // Cấu hình IP tĩnh trước khi kết nối WiFi
    if (!WiFi.config(local_IP, gateway, subnet)) {
        Serial.println("Static IP Failed to configure");
    }

    pinMode(AIN1, OUTPUT); pinMode(AIN2, OUTPUT); pinMode(PWMA, OUTPUT);
    pinMode(BIN1, OUTPUT); pinMode(BIN2, OUTPUT); pinMode(PWMB, OUTPUT);
    pinMode(ENC_L_A, INPUT_PULLUP); pinMode(ENC_L_B, INPUT_PULLUP);
    pinMode(ENC_R_A, INPUT_PULLUP); pinMode(ENC_R_B, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(ENC_L_A), handleLeftEncoder, RISING);
    attachInterrupt(digitalPinToInterrupt(ENC_R_A), handleRightEncoder, RISING);

    ledcSetup(0, 30000, 8); ledcAttachPin(PWMA, 0);
    ledcSetup(1, 30000, 8); ledcAttachPin(PWMB, 1);
    
    stopRobot();

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) { delay(200); }
    WiFi.setSleep(false);
    
    Serial.println(WiFi.localIP());

    // --- SỬA LẠI REMAP NÚT BẤM THEO YÊU CẦU CỦA BẠN ---
    // Nút Tiến/Lùi trên App -> Gọi hàm Xoay
    server.on("/forward", []() { execute(realTurnLeft); });
    server.on("/backward", []() { execute(realTurnRight); });

    // Nút Trái/Phải trên App -> Gọi hàm Tiến/Lùi
    server.on("/left", []() { execute(realMoveBackward); });
    server.on("/right", []() { execute(realMoveForward); });
    
    server.on("/stop", []() { execute(stopRobot); });

    server.on("/encoder", []() {
        String res = String(leftPulseCount) + "|" + String(rightPulseCount);
        server.send(200, "text/plain", res);
    });

    server.on("/reset_encoder", []() {
        leftPulseCount = 0; rightPulseCount = 0;
        server.send(200, "text/plain", "OK");
    });

    server.begin();
}

void loop() {
    server.handleClient();
}