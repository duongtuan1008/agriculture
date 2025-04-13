#include <lvgl.h>

#include <TFT_eSPI.h>

#include <XPT2046_Touchscreen.h>

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <BH1750.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_AHTX0.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <vector>

// Replace with your network credentials
const char *ssid = "mypc";
const char *password = "11111111";
const char *serverName = "http://192.168.137.74/api/get-data.php";
String apiKeyValue = "tPmAT5Ab3j7F9";
WebServer server(80);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7 * 3600, 60000); // GMT+7

// Install Adafruit Unified Sensor and Adafruit BME280 Library

Adafruit_AHTX0 aht10;
BH1750 lightMeter;

// SET VARIABLE TO 0 FOR TEMPERATURE IN FAHRENHEIT DEGREES
#define TEMP_CELSIUS 1

#define LDR_PIN 34

// Touchscreen pins
#define XPT2046_IRQ 36  // T_IRQ
#define XPT2046_MOSI 32 // T_DIN
#define XPT2046_MISO 39 // T_OUT
#define XPT2046_CLK 25  // T_CLK
#define XPT2046_CS 33   // T_CS

SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 160

// Touchscreen coordinates: (x, y) and pressure (z)
int x, y, z;

#define FLOW_K 15.0 // hoặc 20.0, 30.0 nếu cần

#define DRAW_BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 10 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

bool autoMode = false;
bool manualOverride = false;
bool pumpCommand = false;
bool ledRunning = false;

int soilThreshold = 1000;
int pumpDuration = 180;
int pumpStartHour = 6;
int pumpStartMinute = 30;

bool pumpRunning = false;
bool curtainRunning = false;
unsigned long pumpStartTime = 0;

float temperature = 0.0, humidity = 0.0, lightLevel = 0.0;
int flameStatus = 0;
float flowRate = 0.0;
int soilMoisture = 0, rainStatus = 0;
volatile int flow_pulses = 0;
unsigned long lastFlowTime = 0;
float flowRate_Lmin = 0.0;
int rotating_set = 0;
const int num_sets = 2; // 6 dòng chia thành 2 nhóm 3 dòng
unsigned long lastUpdate = 0;
const unsigned long updateInterval = 3000; // Cập nhật mỗi 3 giây
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 1000; // 15 giây
unsigned long lastPumpLog = 0;           // 👈 log bơm mỗi X mili giây
float waterTargetML = 1000.0;            // Ngưỡng cần bơm (từ lịch)
float waterDeliveredML = 0.0;            // Tổng lượng nước đã bơm (mL)
unsigned long lastFlowCalc = 0;          // Thời điểm cập nhật gần nhất
bool lastTouchState = LOW;
bool ledState = false;
bool lastTouchLedState = LOW;

int lastScheduleHour = -1;
int lastScheduleMinute = -1;
int lastScheduleDay = -1; // thêm biến ngày trong tuần

#define TOUCH_PUMP_PIN 12 // GPIO12
#define PUMP_RELAY 13
#define TOUCH_LED_PIN 16 // Cảm biến chạm bật/tắt đèn
#define LED_PIN 17       // Chân điều khiển đèn (thường dùng GPIO2)
#define CURTAIN_PIN 19

#define SOIL_SENSOR_PIN 34
#define LDR_PIN 34
#define SOIL_PIN 35
#define FLOW_SENSOR_PIN 26
#define FLAME_SENSOR_D0 27 // đổi từ 32 -> 27
#define RAIN_SENSOR_PIN 14 // đổi từ 33 -> 14
#define BUZZER_PIN 15
#define FAN_PIN 19
volatile int flowPulseCount = 0;
unsigned long lastFlowCheck = 0;

void controlPumpLogic();

// Icon thay thế khả dụng:       // WiFi

const char *icon_humi = "\xF0\x9F\x92\xA7"; // 💧
#define TEMP_CELSIUS 1

#if TEMP_CELSIUS
const char degree_symbol[] = "\u00B0C";
#else
const char degree_symbol[] = "\u00B0F";
#endif

void IRAM_ATTR flowISR()
{
  flowPulseCount++;
}

// If logging is enabled, it will inform the user about what is happening in the library
void log_print(lv_log_level_t level, const char *buf)
{
  LV_UNUSED(level);
  Serial.println(buf);
  Serial.flush();
}

// Get the Touchscreen data
void touchscreen_read(lv_indev_t *indev, lv_indev_data_t *data)
{
  if (!touchscreen.tirqTouched() || !touchscreen.touched())
  {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }

  TS_Point p = touchscreen.getPoint();

  // Ngăn dữ liệu ảo với áp lực quá cao
  if (p.z > 1000 || p.z < 10)
  {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }

  // Bạn có thể dùng map() tạm để thử
  x = map(p.x, 200, 3700, 0, SCREEN_WIDTH - 1);
  y = map(p.y, 240, 3800, 0, SCREEN_HEIGHT - 1);

  x = constrain(x, 0, SCREEN_WIDTH - 1);
  y = constrain(y, 0, SCREEN_HEIGHT - 1);

  z = p.z;

  data->state = LV_INDEV_STATE_PRESSED;
  data->point.x = x;
  data->point.y = y;

  // DEBUG – in ra nếu thực sự có chạm
  Serial.printf("X = %d | Y = %d | Pressure = %d\n", x, y, z);
}

static lv_obj_t *table;

static void update_rotating_rows(lv_timer_t *timer)
{
  float lux = lightMeter.readLightLevel();
  int soil = analogRead(SOIL_PIN);
  sensors_event_t humi_event, temp_event;
  aht10.getEvent(&humi_event, &temp_event);
  float temperature = temp_event.temperature;
  float humidity = humi_event.relative_humidity;
  String flow_str = String(flowRate_Lmin, 2) + " L/min";
  bool fireDetected = digitalRead(FLAME_SENSOR_D0) == LOW; // LOW = có lửa
  String fire_str = fireDetected ? "warning" : "good";

  // Header
  lv_table_set_cell_value(table, 0, 0, "Data");
  lv_table_set_cell_value(table, 0, 1, "Value");

  // Xóa toàn bộ 3 dòng dưới
  for (int i = 1; i <= 3; i++)
  {
    lv_table_set_cell_value(table, i, 0, "");
    lv_table_set_cell_value(table, i, 1, "");
  }

  if (rotating_set == 0)
  {
    lv_table_set_cell_value(table, 1, 0, "Temp");
    lv_table_set_cell_value(table, 1, 1, (String(temperature, 1) + " °C").c_str());

    lv_table_set_cell_value(table, 2, 0, "Humi");
    lv_table_set_cell_value(table, 2, 1, (String(humidity, 1) + " %").c_str());

    lv_table_set_cell_value(table, 3, 0, "Light");
    lv_table_set_cell_value(table, 3, 1, (String(lux, 1) + " lx").c_str());
  }
  else if (rotating_set == 1)
  {
    lv_table_set_cell_value(table, 1, 0, "Soil");
    lv_table_set_cell_value(table, 1, 1, String(soil).c_str());

    lv_table_set_cell_value(table, 2, 0, "Flow");
    lv_table_set_cell_value(table, 2, 1, flow_str.c_str());

    lv_table_set_cell_value(table, 3, 0, "Fire");
    lv_table_set_cell_value(table, 3, 1, fire_str.c_str());
  }

  rotating_set = (rotating_set + 1) % num_sets;
}

static void float_button_event_cb(lv_event_t *e)
{
  update_rotating_rows(nullptr);
}

static void draw_event_cb(lv_event_t *e)
{
  lv_draw_task_t *draw_task = lv_event_get_draw_task(e);
  lv_draw_dsc_base_t *base_dsc = (lv_draw_dsc_base_t *)draw_task->draw_dsc;
  // If the cells are drawn
  if (base_dsc->part == LV_PART_ITEMS)
  {
    uint32_t row = base_dsc->id1;
    uint32_t col = base_dsc->id2;

    // Make the texts in the first cell center aligned
    if (row == 0)
    {
      lv_draw_label_dsc_t *label_draw_dsc = lv_draw_task_get_label_dsc(draw_task);
      if (label_draw_dsc)
      {
        label_draw_dsc->align = LV_TEXT_ALIGN_CENTER;
      }
      lv_draw_fill_dsc_t *fill_draw_dsc = lv_draw_task_get_fill_dsc(draw_task);
      if (fill_draw_dsc)
      {
        fill_draw_dsc->color = lv_color_mix(lv_palette_main(LV_PALETTE_BLUE), fill_draw_dsc->color, LV_OPA_20);
        fill_draw_dsc->opa = LV_OPA_COVER;
      }
    }
    // In the first column align the texts to the right
    else if (col == 0)
    {
      lv_draw_label_dsc_t *label_draw_dsc = lv_draw_task_get_label_dsc(draw_task);
      if (label_draw_dsc)
      {
        label_draw_dsc->align = LV_TEXT_ALIGN_RIGHT;
      }
    }

    // Make every 2nd row gray color
    if ((row != 0 && row % 2) == 0)
    {
      lv_draw_fill_dsc_t *fill_draw_dsc = lv_draw_task_get_fill_dsc(draw_task);
      if (fill_draw_dsc)
      {
        fill_draw_dsc->color = lv_color_mix(lv_palette_main(LV_PALETTE_GREY), fill_draw_dsc->color, LV_OPA_10);
        fill_draw_dsc->opa = LV_OPA_COVER;
      }
    }
  }
}

void lv_create_main_gui(void)
{
  table = lv_table_create(lv_scr_act());
  lv_table_set_row_cnt(table, 4); // 5 dòng dữ liệu + tiêu đề
  lv_table_set_col_cnt(table, 2);
  // 🧩 Giảm padding để thu nhỏ từng ô
  lv_obj_set_style_pad_all(table, 0, 0); // Không padding tổng thể
  lv_obj_set_style_pad_row(table, 0, 0); // Không padding dòng
  lv_obj_set_style_pad_top(table, 0, 0); // Không padding trong ô
  lv_obj_set_style_pad_bottom(table, 0, 0);
  lv_obj_set_style_pad_left(table, 0, 2);
  lv_obj_set_style_pad_right(table, 0, 2);
  // Gán font nhỏ gọn
  lv_obj_set_style_text_font(table, &lv_font_montserrat_10, 0);

  // Cột icon và value nhỏ gọn
  lv_table_set_col_width(table, 0, 58); // icon
  lv_table_set_col_width(table, 1, 70); // value

  // Gọn bảng
  lv_obj_set_size(table, 128, 160);
  lv_obj_center(table);

  // Cho phép tùy chỉnh vẽ bảng
  lv_obj_add_event_cb(table, draw_event_cb, LV_EVENT_DRAW_TASK_ADDED, NULL);
  lv_obj_add_flag(table, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);

  // Nút làm mới nhỏ
  lv_obj_t *float_button = lv_btn_create(lv_scr_act());
  lv_obj_set_size(float_button, 20, 20);
  lv_obj_add_flag(float_button, LV_OBJ_FLAG_FLOATING);
  lv_obj_align(float_button, LV_ALIGN_BOTTOM_RIGHT, -2, -2);
  lv_obj_add_event_cb(float_button, float_button_event_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_set_style_radius(float_button, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_image_src(float_button, LV_SYMBOL_REFRESH, 0);
  lv_obj_set_style_text_font(float_button, &lv_font_montserrat_10, 0);
  lv_obj_set_style_bg_color(float_button, lv_palette_main(LV_PALETTE_GREEN), LV_PART_MAIN);

  update_rotating_rows(nullptr);

  // Tạo timer để tự động đổi nhóm dữ liệu mỗi 3s
  lv_timer_create(update_rotating_rows, 3000, NULL);

  // update_table_values();
}
int calculateSoilMoisturePercent(int rawValue)
{
  const int DRY_VALUE = 3500; // Giá trị khi đất khô
  const int WET_VALUE = 1200; // Giá trị khi đất ướt

  int percent = map(rawValue, DRY_VALUE, WET_VALUE, 0, 100);
  percent = constrain(percent, 0, 100);
  return percent;
}

void readAllSensors()
{
  // Đọc nhiệt độ & độ ẩm từ AHT10
  sensors_event_t humi_event, temp_event;
  aht10.getEvent(&humi_event, &temp_event);
  temperature = temp_event.temperature;
  humidity = humi_event.relative_humidity;

  // Đọc ánh sáng từ BH1750
  lightLevel = lightMeter.readLightLevel();

  // Đọc độ ẩm đất
  soilMoisture = analogRead(SOIL_PIN);
  int soilPercent = calculateSoilMoisturePercent(soilMoisture);
  Serial.printf("🌱 Soil Moisture Raw: %d | Percent: %d%%\n", soilMoisture, soilPercent);

  // Đọc trạng thái cảm biến lửa
  flameStatus = digitalRead(FLAME_SENSOR_D0) == LOW ? 1 : 0;

  // Giả định bạn đã cập nhật flowRate_Lmin từ ISR
  flowRate = flowRate_Lmin;

  // Nếu có cảm biến mưa:
  rainStatus = analogRead(RAIN_SENSOR_PIN) > 2000 ? 0 : 1;
}
// xử lý máy bơm và đèn
void setPump(bool on)
{
  static bool lastState = false;
  if (on == lastState)
    return; // ⛔ Không làm gì nếu không thay đổi

  digitalWrite(PUMP_RELAY, on ? HIGH : LOW);
  pumpRunning = on;

  Serial.printf("%s | GPIO %d trạng thái: %d\n",
                on ? "🚿 Pump ON" : "🛑 Pump OFF", PUMP_RELAY, digitalRead(PUMP_RELAY));
  lastState = on;
}
void setCurtain(bool on)
{
  static bool lastState = false;
  if (on == lastState)
    return; // ⛔ Không làm gì nếu trạng thái không thay đổi

  // Điều khiển GPIO 19
  digitalWrite(CURTAIN_PIN, on ? HIGH : LOW); // Kiểm tra lại HIGH/LOW
  curtainRunning = on;

  // Kiểm tra và in ra thông báo để xác nhận trạng thái của rèm
  Serial.printf("%s | GPIO %d trạng thái: %d\n",
                on ? "🪟 Curtain OPEN" : "🛑 Curtain CLOSE",
                CURTAIN_PIN,
                digitalRead(CURTAIN_PIN)); // In trạng thái GPIO 19

  lastState = on;
}

void handleSchedulePost()
{
  if (server.method() == HTTP_POST)
  {
    String json = server.arg("plain");
    Serial.println("📥 Đã nhận JSON từ app:");
    Serial.println(json);

    fs::File file = LittleFS.open("/schedule.json", "w");
    if (!file)
    {
      server.send(500, "text/plain", "❌ Không mở được file để lưu");
      Serial.println("❌ Không thể mở file để ghi lịch!");
      return;
    }

    file.print(json);
    file.close();

    server.send(200, "text/plain", "✅ Lưu lịch tưới thành công");
    Serial.println("✅ Đã lưu lịch tưới vào /schedule.json");
  }
  else
  {
    server.send(405, "text/plain", "❌ Chỉ chấp nhận phương thức POST");
  }
}

struct Schedule
{
  int hour, minute;
  float threshold; // hoặc: float flowTargetML;
  std::vector<String> days;
};

std::vector<Schedule> schedules;

bool isTodayScheduled(const std::vector<String> &days, const String &today)
{
  for (auto &d : days)
  {
    if (d == today)
      return true;
  }
  return false;
}

void loadSchedules()
{
  if (!LittleFS.exists("/schedule.json"))
  {
    Serial.println("⚠️ Không tìm thấy file lịch tưới!");
    return;
  }

  fs::File file = LittleFS.open("/schedule.json", "r");
  if (!file)
  {
    Serial.println("❌ Không thể mở file /schedule.json để đọc!");
    return;
  }

  DynamicJsonDocument doc(8192);
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error)
  {
    Serial.print("❌ Lỗi khi đọc JSON: ");
    Serial.println(error.c_str());
    return;
  }

  schedules.clear();
  int index = 0;

  for (JsonObject obj : doc.as<JsonArray>())
  {
    if (!obj["hour"].is<int>() || !obj["minute"].is<int>() ||
        !obj["threshold"].is<float>() || !obj["days"].is<JsonArray>())
    {
      Serial.printf("⚠️ Lịch #%d thiếu dữ liệu, bỏ qua\n", index);
      continue;
    }

    Schedule s;
    s.hour = obj["hour"];
    s.minute = obj["minute"];
    s.threshold = obj["threshold"];

    for (const auto &d : obj["days"].as<JsonArray>())
    {
      s.days.push_back(String(d.as<const char *>()));
    }

    schedules.push_back(s);
    Serial.printf("📝 Lịch #%d - %02d:%02d| Thresh: %d | Days: ",
                  index, s.hour, s.minute, s.threshold);
    for (auto &d : s.days)
      Serial.print(d + " ");
    Serial.println();

    index++;
  }

  Serial.printf("✅ Đã nạp %d lịch tưới\n", schedules.size());
}

void checkPumpSchedule()
{
  timeClient.update(); // Cập nhật thời gian từ NTP

  int hour = timeClient.getHours();
  int minute = timeClient.getMinutes();
  int nowMinutes = hour * 60 + minute;

  time_t now = time(nullptr);
  struct tm *timeinfo = localtime(&now);
  int weekday = timeinfo->tm_wday; // 0 = CN
  String today = (weekday == 0) ? "CN" : String(weekday);

  Serial.printf("🕒 CheckSchedule: %02d:%02d | Today: %s\n", hour, minute, today.c_str());

  for (auto s : schedules)
  {
    int schedMinutes = s.hour * 60 + s.minute;

    Serial.printf("📝 Lịch %02d:%02d | Thresh: %.0f | Days: ", s.hour, s.minute, s.threshold);
    for (auto &d : s.days)
      Serial.print(d + " ");
    Serial.println();

    // ✅ Chỉ chạy nếu:
    // - Khớp thời gian (±1 phút)
    // - Hôm nay nằm trong danh sách
    // - Chưa từng chạy lịch này trong hôm nay
    // Ngăn lịch chạy lại liên tục
    if (abs(nowMinutes - schedMinutes) <= 1 &&
        isTodayScheduled(s.days, today) &&
        !(lastScheduleDay == weekday && lastScheduleHour == s.hour && lastScheduleMinute == s.minute))
    {
      Serial.printf("✅ Khớp lịch, bắt đầu tưới theo lưu lượng! [%02d:%02d]\n", s.hour, s.minute);

      setPump(true);
      pumpRunning = true;
      pumpStartTime = millis();
      waterDeliveredML = 0;
      waterTargetML = s.threshold;
      lastFlowCalc = millis();

      // Ghi lại lịch này đã tưới
      lastScheduleDay = weekday;
      lastScheduleHour = s.hour;
      lastScheduleMinute = s.minute;

      break;
    }
  }
}
void logPumpCompletion(float volume)
{
  if (WiFi.status() == WL_CONNECTED)
  {
    HTTPClient http;
    http.begin("http://192.168.137.74/api/pump_log.php"); // 🔁 Thay bằng đường dẫn PHP của bạn
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    // Lấy thời gian hiện tại
    timeClient.update();
    String currentTime = timeClient.getFormattedTime(); // dạng HH:MM:SS
    String postData = "api_key=" + apiKeyValue +
                      "&device_id=esp32" +
                      "&volume=" + String(volume, 2) +
                      "&status=done" +
                      "&time=" + currentTime;

    Serial.println("📤 Gửi log hoàn thành tưới: " + postData);

    int responseCode = http.POST(postData);
    Serial.print("📩 Phản hồi server: ");
    Serial.println(responseCode);
    http.end();
  }
}
void getControlFromServer()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    HTTPClient http;
    String url = "http://192.168.137.74/api/pump-command.php?rand=" + String(random(1000, 9999));
    http.begin(url); // Chống cache
    int code = http.GET();

    if (code == 200)
    {
      String result = http.getString();
      result.trim();

      Serial.println("📥 JSON từ server: " + result);

      DynamicJsonDocument doc(512);
      DeserializationError error = deserializeJson(doc, result);

      if (error)
      {
        Serial.print("❌ Lỗi JSON: ");
        Serial.println(error.c_str());
        return;
      }

      // ✅ Xử lý trạng thái máy bơm
      String pumpState = doc["pump"] | "OFF"; // Trạng thái máy bơm
      if (pumpState == "ON" && !pumpRunning)
      {
        setPump(true);
        Serial.println("🚿 Bơm được bật từ server");
      }
      else if (pumpState == "OFF" && pumpRunning)
      {
        setPump(false);
        Serial.println("🛑 Bơm được tắt từ server");
      }

      // ✅ Xử lý trạng thái rèm
      String curtainState = doc["curtain"] | "OFF"; // Trạng thái rèm
      if (curtainState == "ON" && !curtainRunning)
      {
        setCurtain(true);
        Serial.println("🪟 Rèm được mở từ server");
      }
      else if (curtainState == "OFF" && curtainRunning)
      {
        setCurtain(false);
        Serial.println("🪟 Rèm được đóng từ server");
      }

      // ✅ Xử lý đèn LED từ server
      String ledServerState = doc["led"] | "OFF"; // Trạng thái đèn LED
      bool shouldLedBeOn = (ledServerState == "ON");

      if (shouldLedBeOn != ledState)
      {
        ledState = shouldLedBeOn;
        digitalWrite(LED_PIN, ledState);
        Serial.printf("💡 Đèn được %s từ server\n", ledState ? "BẬT" : "TẮT");
      }
    }
    else
    {
      Serial.printf("❌ Lỗi HTTP (%d) khi GET\n", code);
    }

    http.end();
  }
  else
  {
    Serial.println("🚫 ESP32 chưa kết nối WiFi");
  }
}

// ✅ Gửi dữ liệu lên server
void sendSensorData()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    HTTPClient http;
    Serial.println("🌐 Đang gửi dữ liệu...");

    http.begin(serverName); // ✅ Dùng đường dẫn mới đã sửa
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String postData = "api_key=" + apiKeyValue +
                      "&temperature=" + String(temperature) +
                      "&humidity=" + String(humidity) +
                      "&flame=" + String(flameStatus) +
                      "&light=" + String(lightLevel) +
                      "&flow=" + String(flowRate) +
                      "&soil=" + String(soilMoisture) +
                      "&rain=" + String(rainStatus);

    Serial.println("📤 POST data: " + postData);

    int httpResponseCode = http.POST(postData);

    Serial.print("📩 Mã phản hồi HTTP: ");
    Serial.println(httpResponseCode);

    String response = http.getString();
    Serial.println("📥 Server phản hồi: " + response);

    http.end();
  }
  else
  {
    Serial.println("❌ Không kết nối WiFi!");
  }
}
void downloadScheduleFromServer()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    HTTPClient http;
    http.begin("http://192.168.137.74/api/control.php?esp=1");

    int code = http.GET();
    if (code == 200)
    {
      String rawJson = http.getString();
      Serial.println("📥 JSON từ server:");
      Serial.println(rawJson);

      DynamicJsonDocument rawDoc(8192);
      DeserializationError error = deserializeJson(rawDoc, rawJson);
      if (error)
      {
        Serial.print("❌ Lỗi phân tích JSON từ server: ");
        Serial.println(error.c_str());
        return;
      }

      DynamicJsonDocument finalDoc(8192);
      JsonArray converted = finalDoc.to<JsonArray>();

      for (JsonObject item : rawDoc.as<JsonArray>())
      {
        if (!item["is_enabled"].is<int>() || item["is_enabled"].as<int>() != 1)
          continue;

        JsonObject s = converted.createNestedObject();
        s["hour"] = item["pump_start_hour"];
        s["minute"] = item["pump_start_minute"];
        s["threshold"] = item["flow_threshold"]; // đặt tên hợp lý hơn

        JsonArray dayArr = s.createNestedArray("days");
        String daysStr = item["repeat_days"];
        int start = 0;
        while (true)
        {
          int idx = daysStr.indexOf(',', start);
          String day = daysStr.substring(start, idx == -1 ? daysStr.length() : idx);
          day.trim();
          if (day.length())
            dayArr.add(day);
          if (idx == -1)
            break;
          start = idx + 1;
        }
      }

      fs::File file = LittleFS.open("/schedule.json", "w");
      if (!file)
      {
        Serial.println("❌ Không thể mở file để ghi!");
        return;
      }

      serializeJson(converted, file);
      file.close();
      Serial.println("✅ Đã lưu lịch tưới vào /schedule.json");
    }
    else
    {
      Serial.printf("❌ Lỗi HTTP khi lấy lịch (Mã: %d)\n", code);
    }

    http.end();
  }
  else
  {
    Serial.println("🚫 Không kết nối được WiFi!");
  }
  loadSchedules(); // 🔄 Nạp lại lịch mới sau khi đã ghi file
}

void updateFlowRate()
{
  unsigned long now = millis();
  if (now - lastFlowCheck >= 1000)
  {
    float freq = (flowPulseCount * 1000.0) / (now - lastFlowCheck);
    flowRate_Lmin = freq / FLOW_K;
    flowPulseCount = 0;
    lastFlowCheck = now;

    // ✅ Gán giá trị sang flowRate dùng để tính toán
    flowRate = flowRate_Lmin;

    Serial.printf("💦 Lưu lượng hiện tại: %.2f L/min\n", flowRate_Lmin);
  }
}
void controlPumpLogic()
{
  time_t now = time(nullptr);
  struct tm *timeinfo = localtime(&now);

  int hour = timeinfo->tm_hour;
  int minute = timeinfo->tm_min;

  // Nếu đang ở chế độ manual -> làm theo app
  if (manualOverride)
  {
    setPump(pumpCommand);
    return;
  }

  // Nếu auto mode -> tưới theo giờ và ngưỡng
  if (autoMode)
  {
    if (!pumpRunning && hour == pumpStartHour && minute == pumpStartMinute && soilMoisture < soilThreshold)
    {
      setPump(true);
      pumpRunning = true;
      pumpStartTime = millis();
      waterDeliveredML = 0; // reset lượng nước
      lastFlowCalc = millis();
      Serial.println("🚿 Bắt đầu tưới tự động");
    }

    if (pumpRunning)
    {
      if (soilMoisture > soilThreshold || (millis() - pumpStartTime > pumpDuration * 1000))
      {
        setPump(false);
        pumpRunning = false;
        Serial.println("🛑 Tắt bơm vì đạt điều kiện ngưỡng hoặc hết thời gian");
      }
    }
  }

  // Log trạng thái định kỳ
  if (millis() - lastPumpLog > 5000)
  {
    Serial.printf("🧠 AutoMode: %d | Manual: %d | Soil: %d | Threshold: %d | Pump: %d\n",
                  autoMode, manualOverride, soilMoisture, soilThreshold, pumpRunning);
    lastPumpLog = millis();
  }

  // Nếu đang bơm → tính lượng nước đã bơm
  if (pumpRunning)
  {
    unsigned long nowMs = millis();

    // ✅ Chờ ít nhất 3 giây để tránh lưu lượng sai lệch ban đầu
    if (nowMs - pumpStartTime >= 3000 && nowMs - lastFlowCalc >= 1000)
    {
      float safeFlowRate = flowRate;

      // ✅ Chặn lưu lượng bất thường
      if (safeFlowRate < 0.1 || safeFlowRate > 200.0)
      {
        Serial.printf("⚠️ Lưu lượng bất thường (%.2f L/min) → bỏ qua\n", safeFlowRate);
        safeFlowRate = 0;
      }

      float flowMLperSec = (safeFlowRate / 60.0) * 1000.0;
      waterDeliveredML += flowMLperSec;
      lastFlowCalc = nowMs;

      Serial.printf("💧 Đã bơm: %.2f mL / %.0f mL\n", waterDeliveredML, waterTargetML);

      // ✅ Dừng nếu đủ lượng nước
      if (waterDeliveredML >= waterTargetML)
      {
        setPump(false);
        pumpRunning = false;
        Serial.println("✅ Đủ lượng nước, dừng bơm");

        // 👉 Gửi log nếu có hàm logPumpCompletion
        logPumpCompletion(waterDeliveredML);
      }
    }
  }
}
void handlePumpTouchSensor()
{
  bool currentState = digitalRead(TOUCH_PUMP_PIN);

  if (currentState == HIGH && lastTouchState == LOW)
  {
    // Phát hiện 1 lần chạm (rising edge)
    setPump(!pumpRunning); // Đảo trạng thái bơm
    Serial.println("👆 Cảm biến chạm: Đổi trạng thái máy bơm");

    delay(300); // Chống rung nhẹ
  }

  lastTouchState = currentState;
}
void handleLedTouchSensor()
{
  bool current = digitalRead(TOUCH_LED_PIN);
  if (current == HIGH && lastTouchLedState == LOW)
  {
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
    Serial.printf("💡 Đèn %s\n", ledState ? "BẬT" : "TẮT");
    delay(300);
  }
  lastTouchLedState = current;
}

void setup()
{
  // 🔌 Bắt đầu Serial trước
  Serial.begin(115200);
  delay(100); // đợi ổn định cổng Serial
  Serial.println();
  Serial.println("🔧 ESP32 Đang khởi động...");

  // 🖥️ In version của LVGL
  String LVGL_Arduino = String("LVGL Library Version: ") + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
  Serial.println(LVGL_Arduino);

  // 🚿 Khởi tạo relay & tắt ban đầu
  pinMode(PUMP_RELAY, OUTPUT);
  setPump(false);

  // 🧪 TEST relay thủ công
  Serial.println("🧪 TEST: Bật relay 3 giây...");
  setPump(true);
  delay(3000);
  setPump(false);
  Serial.println("✅ TEST hoàn tất");

  // 🎚️ Cấu hình ADC
  analogReadResolution(12);

  // 📡 Kết nối Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("📶 Connecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\n✅ Đã kết nối Wi-Fi: ");
  Serial.println(WiFi.localIP());

  // 🌡️ AHT10 - Nhiệt độ & độ ẩm
  aht10.begin();

  // 🧠 I2C & BH1750 - ánh sáng
  Wire.begin();
  if (!lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE))
  {
    Serial.println("⚠️ Không tìm thấy cảm biến BH1750!");
  }
  else
  {
    Serial.println("✅ BH1750 OK");
  }

  // 💾 File hệ thống
  LittleFS.begin(true);

  // 🌐 Server nội bộ (nhận lịch từ app)
  server.on("/schedule", HTTP_POST, handleSchedulePost);
  server.begin();

  // 🕒 Thời gian thực
  timeClient.begin();

  // 🌱 Cảm biến
  pinMode(SOIL_PIN, INPUT);
  pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), flowISR, RISING);

  // 🧠 LVGL giao diện
  lv_init();
  lv_log_register_print_cb(log_print);

  // 🖲️ Touchscreen
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin();
  touchscreen.setRotation(2);

  // 📺 Màn hình
  lv_display_t *disp = lv_tft_espi_create(SCREEN_WIDTH, SCREEN_HEIGHT, draw_buf, sizeof(draw_buf));
  lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_0);

  // ⬆️ Đọc cảm ứng
  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touchscreen_read);

  // 🖼️ GUI
  lv_create_main_gui();

  // 🔁 Gửi dữ liệu mỗi 5s
  lv_timer_create([](lv_timer_t *)
                  {
    readAllSensors();
    sendSensorData(); }, 5000, NULL);

  pinMode(TOUCH_PUMP_PIN, INPUT_PULLDOWN);
  pinMode(TOUCH_LED_PIN, INPUT_PULLDOWN); // 👈 Bắt buộc
  pinMode(LED_PIN, OUTPUT);               // 👈 để điều khiển LED
  digitalWrite(LED_PIN, LOW);             // 👈 tắt LED ban đầu
  pinMode(CURTAIN_PIN, OUTPUT);           // 👈 để điều khiển LED
  digitalWrite(CURTAIN_PIN, LOW);
  // ⏬ Tải lịch từ server và nạp vào RAM
  downloadScheduleFromServer(); // gọi luôn cả loadSchedules()
}

void loop()
{
  lv_task_handler();
  lv_tick_inc(5);

  unsigned long now = millis();

  // Gửi cảm biến
  if (now - lastSendTime > 5000)
  {
    sendSensorData();
    lastSendTime = now;
  }

  if (now - lastUpdate > 1000) // 1000ms = 1 giây
  {
    getControlFromServer();
    lastUpdate = now;
  }

  controlPumpLogic();      // Auto/manual logic
  server.handleClient();   // Xử lý webserver
  checkPumpSchedule();     // Lịch tưới
  updateFlowRate();        // Lưu lượng nước
  handlePumpTouchSensor(); // Cảm biến chạm bơm
  handleLedTouchSensor();  // Cảm biến chạm đèn

  delay(5); // Mượt cho LVGL
}
