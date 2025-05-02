#include <WiFi.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>

#define STEP_PIN        2
#define DIR_PIN         3
#define ENABLE_PIN      4
#define SWITCH_TOP      6
#define SWITCH_BOTTOM   7

const char* ssid = "Testing Facility";
const char* password = "testingfacility";
const char* server_ip = "192.168.1.225";
const uint16_t server_port = 4840;

WiFiClient client;

int b_Homing_E = 0, w_Main_EV = 0, b_SingleStep_E = 0;
int previous_homing_e = 0;
int previous_single_step = 0;

long pos_top = 0, pos_bottom = 0, pos_bottom_adj = 0;
long current_position = 0, target_position = 0;
long fine_adjust_steps = 0;

bool is_calibrated = false;
bool is_calibrating = false;
bool in_calibration_mode = false;

const int CALIBRATION_DELAY_US = 3000; // ~20 RPM
const int MIN_DELAY_US = 500;          // fast speed
const int MAX_DELAY_US = 1200;         // slow start

const int stepsPerRev = 200;
const int microstepping = 16;

void setup() {
  Serial.begin(115200);
  delay(1500);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\n✅ WiFi connected");

  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(ENABLE_PIN, OUTPUT);
  pinMode(SWITCH_TOP, INPUT_PULLUP);
  pinMode(SWITCH_BOTTOM, INPUT_PULLUP);
  digitalWrite(ENABLE_PIN, LOW);

  if (client.connect(server_ip, server_port)) {
    Serial.println("✅ Connected to TCP server!");
  } else {
    Serial.println("❌ TCP connection failed.");
  }
}

void loop() {
  if (!client.connected()) {
    Serial.println("🔄 Reconnecting...");
    client.connect(server_ip, server_port);
    delay(1000);
    return;
  }

  if (client.available()) {
    String line = client.readStringUntil('\n');
    StaticJsonDocument<400> doc;
    if (!deserializeJson(doc, line)) {
      b_Homing_E = doc["b_Homing_E"].as<int>();
      w_Main_EV = doc["w_Main_EV"].as<int>();
      b_SingleStep_E = doc["b_SingleStep_E"].as<int>();

      // --- Calibration logic ---
      if (b_Homing_E == 1 && !in_calibration_mode) {
        runCalibration();
        in_calibration_mode = true;
        fine_adjust_steps = 0;
      }

      if (b_Homing_E == 0 && in_calibration_mode) {
        pos_bottom_adj = current_position;
        is_calibrated = true;
        in_calibration_mode = false;
        Serial.println("✅ Calibration validée !");
        Serial.print("🔧 Ajustement fin : "); Serial.print(fine_adjust_steps); Serial.println(" pas");
        sendStatus(current_positionAsPercent());
      }

      // Ajustement fin pendant calibration
      if (in_calibration_mode && b_SingleStep_E == 1 && previous_single_step == 0) {
        digitalWrite(DIR_PIN, LOW); // fermeture
        singleStep(); delayMicroseconds(CALIBRATION_DELAY_US);
        current_position--; fine_adjust_steps++;
        sendStatus(current_positionAsPercent());
      }
      previous_single_step = b_SingleStep_E;

      // Contrôle normal
      if (is_calibrated && !in_calibration_mode) {
        target_position = map(w_Main_EV, 0, 100, pos_bottom_adj, pos_top);

        // Clamp position
        if (target_position > pos_top) target_position = pos_top;
        if (target_position < pos_bottom_adj) target_position = pos_bottom_adj;

        if (target_position != current_position) {
          moveTo(target_position);
          current_position = target_position;
          sendStatus(current_positionAsPercent());
        }
      }
    }
  }
}

void runCalibration() {
  is_calibrating = true;
  in_calibration_mode = true;
  current_position = 0;

  Serial.println("⚙️ Calibration en cours...");

  digitalWrite(DIR_PIN, HIGH);
  while (digitalRead(SWITCH_TOP)) {
    singleStep(); delayMicroseconds(CALIBRATION_DELAY_US); current_position++;
  }
  pos_top = current_position; delay(300);

  digitalWrite(DIR_PIN, LOW);
  while (digitalRead(SWITCH_BOTTOM)) {
    singleStep(); delayMicroseconds(CALIBRATION_DELAY_US); current_position--;
  }
  pos_bottom = current_position; delay(300);

  Serial.println("📥 En attente d’ajustement fin...");
  is_calibrating = false;
}

void moveTo(long to) {
  long total_steps = abs(to - current_position);
  if (total_steps == 0) return;

  bool dir = (to > current_position);
  digitalWrite(DIR_PIN, dir ? HIGH : LOW);
  digitalWrite(ENABLE_PIN, LOW);

  const int boost_steps = (int)(stepsPerRev * microstepping * (2.0 / 360.0));
  long boost_phase = min(boost_steps, total_steps / 2);
  long accel_steps = total_steps - boost_phase;

  for (long i = 0; i < total_steps; i++) {
    int delay_us;
    if (i < boost_phase) {
      float progress = (float)i / boost_phase;
      delay_us = MAX_DELAY_US - (MAX_DELAY_US - MIN_DELAY_US) * sqrt(progress);
    } else {
      float linear_progress = (float)(i - boost_phase) / accel_steps;
      delay_us = MIN_DELAY_US + (MAX_DELAY_US - MIN_DELAY_US) * linear_progress;
    }
    delay_us = constrain(delay_us, MIN_DELAY_US, MAX_DELAY_US);
    singleStep();
    delayMicroseconds(delay_us);
    current_position += (dir ? 1 : -1);
  }
}

void singleStep() {
  digitalWrite(STEP_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(STEP_PIN, LOW);  delayMicroseconds(10);
}

void sendStatus(int value) {
  if (!client.connected()) return;
  StaticJsonDocument<64> doc;
  doc["status_epos_e"] = value;
  serializeJson(doc, client);
  client.print('\n');
}

int current_positionAsPercent() {
  if (pos_top == pos_bottom_adj) return 0;
  return map(current_position, pos_bottom_adj, pos_top, 0, 100);
}
