#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>  // مكتبة للتحكم في السيرفو على ESP32

// تعريف المكونات
#define SERVO_PIN_GATE 12        // سيرفو البوابة الخارجية
#define LED_GREEN 14
#define LED_RED 24
#define TCRT5000_PIN 34

// تعريف المكونات لكل موقف
#define NUM_PARKING_SPOTS 4
int servoPins[NUM_PARKING_SPOTS] = {13, 15, 16, 17};  // سيرفو لكل موقف
int ledGreenPins[NUM_PARKING_SPOTS] = {18, 19, 21, 22};  // LED أخضر لكل موقف
int ledRedPins[NUM_PARKING_SPOTS] = {23, 27, 25, 26};    // LED أحمر لكل موقف
int tcrtPins[NUM_PARKING_SPOTS] = {32, 33, 34, 35};  // حساس TCRT5000 لكل موقف

Servo gateServo;   // سيرفو البوابة الخارجية
Servo parkingServos[NUM_PARKING_SPOTS];  // مصفوفة من السيرفوهات للمواقف
int servoOpenAngle = 90;  // زاوية فتح
int servoCloseAngle = 0;  // زاوية إغلاق

bool parkingAvailable[NUM_PARKING_SPOTS] = {true, true, true, true};  // حالة المواقف
bool carInside[NUM_PARKING_SPOTS] = {false, false, false, false};  // حالة وجود السيارة في المواقف
String reservationCode[NUM_PARKING_SPOTS];  // كود الحجز لكل موقف
unsigned long reservationStartTime[NUM_PARKING_SPOTS] = {0, 0, 0, 0};  // وقت بدء الحجز لكل موقف
unsigned long reservationDuration[NUM_PARKING_SPOTS] = {0, 0, 0, 0};  // مدة الحجز بالثواني لكل موقف
unsigned long parkingStartTime[NUM_PARKING_SPOTS] = {0, 0, 0, 0}; // وقت دخول السيارة لكل موقف

// بيانات الشبكة
const char* ssid = "FiberHGW_ZTR32J";
const char* password = "TF9cNT7bNACH";

// إعداد السيرفر
AsyncWebServer server(80);

// صفحة الويب الرئيسية
String generateWebPage() {
  String page = "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'>";
  page += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  page += "<style>body {background-color: #001f3f; color: white; font-family: Arial; text-align: center;} ";
  page += ".box {background-color: #a8323e; padding: 20px; margin: 20px auto; border-radius: 10px; width: 80%;} ";
  page += ".btn {background-color: #0074d9; color: white; padding: 10px; border: none; cursor: pointer;} </style>";
  page += "<script>";
  page += "function updateParkingStatus() {";
  page += "  fetch('/get_parking_status')"; // استدعاء API
  page += "    .then(response => response.json())";
  page += "    .then(data => {";
  page += "      data.parking_spots.forEach(spot => {";
  page += "        const spotElement = document.getElementById('spot-' + spot.spot);";
  page += "        if (spotElement) {";
  page += "          spotElement.innerHTML = 'Status: ' + (spot.status === 'Available' ? '<span style=\"color:green;\">Available</span>' : '<span style=\"color:red;\">Occupied</span>');";
  page += "        }";
  page += "      });";
  page += "    });";
  page += "}";
  page += "setInterval(updateParkingStatus, 1000);"; // التحديث كل 5 ثوانٍ
  page += "</script>";
  page += "</head><body>";
  page += "<h1>Karabuk Otopark</h1>";
  for (int i = 0; i < NUM_PARKING_SPOTS; i++) {
    page += "<div class='box'><h2>Park " + String(i + 1) + "</h2>";
    page += "<p id='spot-" + String(i + 1) + "'>Status: " + (parkingAvailable[i] ? "<span style='color:green;'>Available</span>" : "<span style='color:red;'>Occupied</span>") + "</p>";
    if (parkingAvailable[i]) {
      page += "<button class='btn' onclick='window.location.href=\"/reserve?spot=" + String(i) + "\"'>Reserve</button>";
    } else {
     page += "<form method='POST' action='/enter?spot=" + String(i) + "'>";
     page += "<label for='code'>Enter Code:</label>";
     page += "<input type='text' id='code' name='code'>";
      page += "<button type='submit' class='btn'>Submit</button></form>";
    } 
    page += "</div>";
  }
  page += "</body></html>";
  return page;
}

// صفحة النجاح (الكود صحيح)
String generateSuccessPage() {
  String page = "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'>";
  page += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  page += "<style>";
  page += "body {background-color: #001f3f; color: white; font-family: Arial; display: flex; align-items: center; justify-content: center; height: 100vh; margin: 0;} ";
  page += ".circle {background-color: white; color: #001f3f; border-radius: 50%; width: 200px; height: 200px; display: flex; align-items: center; justify-content: center; font-size: 20px; font-weight: bold;} ";
  page += "</style>";
  page += "</head><body>";
  page += "<div class='circle'> dogru </div>";
  page += "</body></html>";
  return page;
}

// صفحة الحجز
String generateReservationPage(int spot) {
  reservationCode[spot] = String(random(1000, 9999)); // إنشاء كود فريد
  String page = "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'>";
  page += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  page += "<style>body {background-color: #001f3f; color: white; font-family: Arial; text-align: center;} ";
  page += ".box {background-color: #a8323e; padding: 20px; margin: 20px auto; border-radius: 10px; width: 80%;} ";
  page += ".btn {background-color: #0074d9; color: white; padding: 10px; border: none; cursor: pointer;} </style>";
  page += "</head><body>";
  page += "<h1>Karabuk Otopark</h1>";
  page += "<div class='box'><h2>Reservation Details</h2>";
  page += "<p>Your Reservation Code: <strong>" + reservationCode[spot] + "</strong></p>";
  page += "<p>Cost: 5 USD per 10 minutes</p>";
  page += "<form method='POST' action='/confirm?spot=" + String(spot) + "'>";
  page += "<label for='time'>Enter Arrival Time (seconds):</label>";
  page += "<input type='number' id='time' name='time' required>";
  page += "<button type='submit' class='btn'>Confirm</button></form></div></body></html>";
  return page;
}

// صفحة الحجز الجديدة مع العداد التنازلي
String generateCountdownPage(int spot) {
  String page = "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'>";
  page += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  page += "<style>body {background-color: #001f3f; color: white; font-family: Arial; text-align: center;} ";
  page += ".box {background-color: #a8323e; padding: 20px; margin: 20px auto; border-radius: 10px; width: 80%;} ";
  page += ".btn {background-color: #0074d9; color: white; padding: 10px; border: none; cursor: pointer;} ";
  page += "#countdown {font-size: 40px; font-weight: bold;} </style>";
  page += "<script>";
  page += "let countdownTime = " + String(reservationDuration[spot]) + " * 1000;";  // مدة الحجز بالثواني
  page += "let countdownInterval = setInterval(function() {";
  page += "  if (countdownTime <= 0) {";
  page += "    clearInterval(countdownInterval);";
  page += "    alert('Reservation has expired.');";
  page += "    window.location.href = '/';";  // إعادة توجيه الصفحة الرئيسية عند انتهاء العداد
  page += "  } else {";
  page += "    let minutes = Math.floor(countdownTime / 60000);";
  page += "    let seconds = Math.floor((countdownTime % 60000) / 1000);";
  page += "    document.getElementById('countdown').innerHTML = minutes + 'm ' + seconds + 's';";
  page += "    countdownTime -= 1000;";
  page += "  }";
  page += "}, 1000);";
  page += "</script>";
  page += "</head><body>";
  page += "<h1>Karabuk Otopark</h1>";
  page += "<div class='box'><h2>Reservation for Spot " + String(spot + 1) + "</h2>";
  page += "<p id='countdown'>Loading...</p>";
  page += "<form method='POST' action='/enter?spot=" + String(spot) + "'>";
  page += "<label for='code'>Enter Code:</label>";
  page += "<input type='text' id='code' name='code'>";
  page += "<button type='submit' class='btn'>Submit</button></form></div>";
  page += "</body></html>";
  return page;
}

// صفحة الخطأ
String generateErrorPage() {
  String page = "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'>";
  page += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  page += "<style>body {background-color: #001f3f; color: white; font-family: Arial; text-align: center;} ";
  page += ".box {background-color: #a8323e; padding: 20px; margin: 20px auto; border-radius: 10px; width: 80%;} ";
  page += ".btn {background-color: #0074d9; color: white; padding: 10px; border: none; cursor: pointer;} </style>";
  page += "</head><body>";
  page += "<h1>Karabuk Otopark</h1>";
  page += "<div class='box'><h2>Error</h2>";
  page += "<p>The code entered is invalid or time has expired.</p>";
  page += "<button class='btn' onclick='window.location.href=\"/\"'>Return</button>";
  page += "</div></body></html>";
  return page;
}

// تهيئة النظام
void setup() {
  Serial.begin(115200);

  // اتصال الشبكة
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  // طباعة عنوان الـ IP
  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());  // طباعة عنوان الـ IP

  // إعداد المكونات
  gateServo.attach(SERVO_PIN_GATE);
  gateServo.write(servoOpenAngle); // فتح البوابة
  for (int i = 0; i < NUM_PARKING_SPOTS; i++) {
    parkingServos[i].attach(servoPins[i]);
    parkingServos[i].write(servoOpenAngle);  // فتح كل المواقف
    pinMode(ledGreenPins[i], OUTPUT);
    pinMode(ledRedPins[i], OUTPUT);
    pinMode(tcrtPins[i], INPUT);
    digitalWrite(ledGreenPins[i], HIGH);
    digitalWrite(ledRedPins[i], LOW);
  }

  // إعداد السيرفر
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/html", generateWebPage());
  });

  server.on("/reserve", HTTP_GET, [](AsyncWebServerRequest* request) {
    int spot = request->getParam("spot")->value().toInt();
    if (parkingAvailable[spot]) {
      request->send(200, "text/html", generateReservationPage(spot));
    } else {
      request->send(403, "text/plain", "Parking is not available.");
    }
  });

  server.on("/confirm", HTTP_POST, [](AsyncWebServerRequest* request) {
    int spot = request->getParam("spot")->value().toInt();
    if (request->hasParam("time", true)) {
      reservationDuration[spot] = request->getParam("time", true)->value().toInt();
      reservationStartTime[spot] = millis();
      parkingAvailable[spot] = false;
      parkingServos[spot].write(servoCloseAngle);
      digitalWrite(ledGreenPins[spot], LOW);
      digitalWrite(ledRedPins[spot], HIGH);
      request->send(200, "text/html", generateCountdownPage(spot));  // عرض صفحة العداد التنازلي
    } else {
      request->send(400, "text/plain", "Invalid request.");
    }
  });

  server.on("/enter", HTTP_POST, [](AsyncWebServerRequest* request) {
    int spot = request->getParam("spot")->value().toInt();
    if (request->hasParam("code", true)) {
      String code = request->getParam("code", true)->value();
      if (code == reservationCode[spot] && (millis() - reservationStartTime[spot]) / 1000 <= reservationDuration[spot]) {
        parkingServos[spot].write(servoOpenAngle);  // فتح السيرفو للموقف بعد التأكد من الكود الصحيح
        digitalWrite(ledGreenPins[spot], HIGH);    // تشغيل LED الأخضر
        digitalWrite(ledRedPins[spot], LOW);      // إيقاف LED الأحمر
        request->send(200, "text/html", generateSuccessPage());
      } else {
        request->send(200, "text/html", generateErrorPage());
      }
    } else {
      request->send(400, "text/plain", "Invalid request.");
    }
  });

  server.on("/close_reservation", HTTP_POST, [](AsyncWebServerRequest* request) {
    int spot = request->getParam("spot")->value().toInt();
    parkingAvailable[spot] = true;  // تحديث حالة الموقف إلى متاح
    reservationCode[spot] = "";  // مسح الكود
    reservationStartTime[spot] = 0;  // مسح وقت بدء الحجز
    parkingServos[spot].write(servoOpenAngle);  // فتح الموقف
    digitalWrite(ledGreenPins[spot], HIGH);  // تشغيل LED الأخضر
    digitalWrite(ledRedPins[spot], LOW);  // إيقاف LED الأحمر
    request->redirect("/");  // العودة إلى الصفحة الرئيسية
  });

  // إضافة endpoint لتحديث حالة المواقف
  server.on("/get_parking_status", HTTP_GET, [](AsyncWebServerRequest* request) {
    String json = "{ \"parking_spots\": [";
    for (int i = 0; i < NUM_PARKING_SPOTS; i++) {
      json += "{ \"spot\": " + String(i + 1) + ", \"status\": \"" + (parkingAvailable[i] ? "Available" : "Occupied") + "\" }";
      if (i < NUM_PARKING_SPOTS - 1) json += ",";
    }
    json += "] }";

    request->send(200, "application/json", json);
  });

  // بدء السيرفر
  server.begin();
}

unsigned long previousMillis = 0;  // لتخزين الوقت السابق
const long interval = 5000;         // فاصل زمني للتحقق من انتهاء مدة الحجز (1 ثانية)

void loop() {
  unsigned long currentMillis = millis();

  // 1. التحقق من الحساسات
  for (int i = 0; i < NUM_PARKING_SPOTS; i++) {
    int sensorValue = digitalRead(tcrtPins[i]);

    if (sensorValue == LOW && !carInside[i]) {
      carInside[i] = true;
      parkingAvailable[i] = false;
      parkingStartTime[i] = currentMillis;  // استخدام currentMillis بدلاً من millis()
      parkingServos[i].write(servoCloseAngle);
      digitalWrite(ledGreenPins[i], LOW);
      digitalWrite(ledRedPins[i], HIGH);
    } else if (sensorValue == HIGH && carInside[i]) {
      carInside[i] = false;
      parkingAvailable[i] = true;
      parkingServos[i].write(servoOpenAngle);
      digitalWrite(ledGreenPins[i], HIGH);
      digitalWrite(ledRedPins[i], LOW);
    }
  }

  // 2. التحقق من انتهاء مدة الحجز (بفاصل زمني)
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;  // تحديث الوقت

    // التحقق من انتهاء مدة الحجز فقط كل ثانية
    for (int i = 0; i < NUM_PARKING_SPOTS; i++) {
      if (!parkingAvailable[i] && (currentMillis - reservationStartTime[i]) / 1000 >= reservationDuration[i]) {
        // انتهاء مدة الحجز
        parkingAvailable[i] = true;  
        reservationCode[i] = "";     
        reservationStartTime[i] = 0; 
        parkingServos[i].write(servoOpenAngle);  
        digitalWrite(ledGreenPins[i], HIGH);    
        digitalWrite(ledRedPins[i], LOW);      
      }
    }
  }
}