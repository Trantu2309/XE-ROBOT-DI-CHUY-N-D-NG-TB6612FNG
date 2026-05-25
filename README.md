#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

// --- KHỐI 1: THÔNG TIN WIFI ---
const char* ssid     = "Happy House Q12";
const char* password = "28282828";

// --- KHỐI 3: CHÂN ĐIỀU KHIỂN TB6612FNG ---
const int PWMA = 25; const int AIN1 = 26; const int AIN2 = 27; // Motor Trái
const int PWMB = 13; const int BIN1 = 14; const int BIN2 = 12; // Motor Phải

// --- KHỐI 4: CHÂN ENCODER ---
const int ENC_L_A = 4;  const int ENC_L_B = 18; // Trái
const int ENC_R_A = 19; const int ENC_R_B = 21; // Phải

// Biến lưu số xung Encoder (Dùng volatile cho hàm ngắt)
volatile long leftPulseCount = 0;
volatile long rightPulseCount = 0;

// Hàm ngắt đọc xung Encoder Trái
void IRAM_ATTR handleLeftEncoder() {
    if (digitalRead(ENC_L_B) == LOW) leftPulseCount++; else leftPulseCount--;
}

// Hàm ngắt đọc xung Encoder Phải
void IRAM_ATTR handleRightEncoder() {
    if (digitalRead(ENC_R_B) == LOW) rightPulseCount++; else rightPulseCount--;
}

WebServer server(80);

// Cấu hình PWM cho phiên bản ESP32 cũ
void setSpeed(int speed) {
    ledcWrite(0, speed); // Kênh 0 cho motor trái
    ledcWrite(1, speed); // Kênh 1 cho motor phải
}

void stopRobot() {
    digitalWrite(AIN1, LOW);  digitalWrite(AIN2, LOW);
    digitalWrite(BIN1, LOW);  digitalWrite(BIN2, LOW);
    setSpeed(0);
}

void moveForward() {
    digitalWrite(AIN1, HIGH); digitalWrite(AIN2, LOW);
    digitalWrite(BIN1, HIGH); digitalWrite(BIN2, LOW);
    setSpeed(220); // Tốc độ chạy đường trường
}

void moveBackward() {
    digitalWrite(AIN1, LOW);  digitalWrite(AIN2, HIGH);
    digitalWrite(BIN1, LOW);  digitalWrite(BIN2, HIGH);
    setSpeed(220);
}

void turnLeft() {
    // Xoay tại chỗ: Trái lùi, Phải tiến
    digitalWrite(AIN1, LOW);  digitalWrite(AIN2, HIGH);
    digitalWrite(BIN1, HIGH); digitalWrite(BIN2, LOW);
    setSpeed(200); // Tốc độ xoay
}

void turnRight() {
    // Xoay tại chỗ: Trái tiến, Phải lùi
    digitalWrite(AIN1, HIGH); digitalWrite(AIN2, LOW);
    digitalWrite(BIN1, LOW);  digitalWrite(BIN2, HIGH);
    setSpeed(200);
}

// Hàm phản hồi lệnh nhanh cho App
void execute(void (*func)()) {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", "OK");
    func();
}

void setup() {
    Serial.begin(115200);

    // Cấu hình chân Motor
    pinMode(AIN1, OUTPUT); pinMode(AIN2, OUTPUT); pinMode(PWMA, OUTPUT);
    pinMode(BIN1, OUTPUT); pinMode(BIN2, OUTPUT); pinMode(PWMB, OUTPUT);

    // Cấu hình chân Encoder (INPUT_PULLUP giúp chống nhiễu)
    pinMode(ENC_L_A, INPUT_PULLUP); pinMode(ENC_L_B, INPUT_PULLUP);
    pinMode(ENC_R_A, INPUT_PULLUP); pinMode(ENC_R_B, INPUT_PULLUP);

    // Thiết lập Ngắt Encoder (Cực kỳ chính xác)
    attachInterrupt(digitalPinToInterrupt(ENC_L_A), handleLeftEncoder, RISING);
    attachInterrupt(digitalPinToInterrupt(ENC_R_A), handleRightEncoder, RISING);

    // Thiết lập PWM (Tần số 30kHz cho GA25 chạy êm)
    ledcSetup(0, 30000, 8); ledcAttachPin(PWMA, 0);
    ledcSetup(1, 30000, 8); ledcAttachPin(PWMB, 1);
    
    stopRobot();

    // Kết nối WiFi
    WiFi.begin(ssid, password);
    Serial.print("Connecting");
    while (WiFi.status() != WL_CONNECTED) {
        delay(200);
        Serial.print(".");
    }
    
    // --- TẮT CHẾ ĐỘ NGỦ ĐỂ PHẢN HỒI TỨC THÌ ---
    WiFi.setSleep(false);
    
    Serial.println("\nXe da san sang! IP: ");
    Serial.println(WiFi.localIP());

    // Các lệnh điều khiển từ App
    server.on("/forward", []() { execute(moveForward); });
    server.on("/backward", []() { execute(moveBackward); });
    server.on("/left", []() { execute(turnLeft); });
    server.on("/right", []() { execute(turnRight); });
    server.on("/stop", []() { execute(stopRobot); });

    // API gửi số xung Encoder về điện thoại
    server.on("/encoder", []() {
        String data = String(leftPulseCount) + "|" + String(rightPulseCount);
        server.sendHeader("Connection", "close");
        server.send(200, "text/plain", data);
    });

    // API Reset số xung về 0
    server.on("/reset_encoder", []() {
        leftPulseCount = 0; rightPulseCount = 0;
        server.sendHeader("Connection", "close");
        server.send(200, "text/plain", "OK");
    });

    server.begin();
}

void loop() {
    server.handleClient();
}
