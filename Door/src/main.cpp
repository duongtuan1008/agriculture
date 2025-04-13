#include <WiFi.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Keypad.h>
#include <Wire.h>
#include <SPIFFS.h>
#include <Keypad.h>
#include <EEPROM.h>
#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal_I2C.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>

LiquidCrystal_I2C lcd(0x27, 20, 4); // Địa chỉ I2C thường là 0x27 hoặc 0x3F

const char *ssid = "mypc";
const char *password_wifi = "11111111";

const char *server_url = "http://192.168.137.74/api/control.php?esp=1";
const char *server_rfid_url = "http://192.168.137.74/api/rfid.php";
const char *server_pass_url = "http://192.168.137.74/api/password.php";
String apiKeyValue = "tPmAT5Ab3j7F9";

bool screenUpdated = false;
unsigned char id = 0;
unsigned char id_rf = 0;
unsigned char index_t = 0;
unsigned char error_in = 0;
unsigned char in_num = 0, error_pass = 0, isMode = 0;
unsigned long lastFetch = 0;
unsigned long lastScheduleFetch = 0;
String lastUIState = "";    // để tránh ghi đè nhiều lần
byte firstScanTag[4] = {0}; // Biến lưu dữ liệu UID của thẻ quét lần 1

// 🛠 Keypad 4x4
const byte ROWS = 4, COLS = 4;
char keys[ROWS][COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}};
byte rowPins[4] = {13, 12, 14, 27}; // ✅ Hàng - Sử dụng GPIO hợp lệ
byte colPins[4] = {16, 17, 4, 15};  // ✅ Cột - Đảm bảo GPIO hợp lệ

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

const byte RFID_SIZE = 4;
int addr = 0;
char password[6] = "11111";
char pass_def[6] = "12395";
char mode_changePass[6] = "*#01#";
char mode_resetPass[6] = "*#02#";
char mode_hardReset[6] = "*#03#";
char mode_addRFID[6] = "*101#";
char mode_delRFID[6] = "*102#";
char mode_delAllRFID[6] = "*103#";
char data_input[6];
char new_pass1[6];
char new_pass2[6];

// 🛠 SPI cho RFID (HSPI)
// ⚡ Chuyển Module RFID RC522 sang HSPI
#define PIN_SG90 26
#define SS_PIN 5  // ESP32 pin GIOP5
#define RST_PIN 2 // ESP32 pin GIOP27
Servo sg90;
MFRC522 rfid(SS_PIN, RST_PIN); // Chân SS và RST của RFID

MFRC522::MIFARE_Key key;
byte nuidPICC[4];
typedef enum
{
  MODE_ID_RFID_ADD,
  MODE_ID_RFID_FIRST,
  MODE_ID_RFID_SECOND,
} MODE_ID_RFID_E;

// unsigned char MODE = MODE_ID_FINGER_ADD; // Mode = 3
unsigned char MODE_RFID = MODE_ID_RFID_ADD;
void writeEpprom(char data[])
{
  for (unsigned char i = 0; i < 5; i++)
  {
    EEPROM.write(i, data[i]);
  }
  EEPROM.commit();
}

// 📌 Đọc mật khẩu từ EEPROM
void readEpprom()
{
  for (unsigned char i = 0; i < 5; i++)
  {
    password[i] = EEPROM.read(i);
  }
}

// 📌 Xóa dữ liệu nhập
void clear_data_input() // xoa gia tri nhap vao hien tai
{
  int i = 0;
  for (i = 0; i < 6; i++)
  {
    data_input[i] = '\0';
  }
}
unsigned char isBufferdata(char data[]) // Kiem tra buffer da co gia tri chua
{
  unsigned char i = 0;
  for (i = 0; i < 5; i++)
  {
    if (data[i] == '\0')
    {
      return 0;
    }
  }
  return 1;
}

bool compareData(char data1[], char data2[]) // Kiem tra 2 cai buffer co giong nhau hay khong
{
  unsigned char i = 0;
  for (i = 0; i < 5; i++)
  {
    if (data1[i] != data2[i])
    {
      return false;
    }
  }
  return true;
}

void insertData(char data1[], char data2[]) // Gan buffer 2 cho buffer 1
{
  unsigned char i = 0;
  for (i = 0; i < 5; i++)
  {
    data1[i] = data2[i];
  }
}

void getData()
{
  char key = keypad.getKey();
  if (key)
  {
    if (in_num < 5)
    {
      data_input[in_num] = key;
      lcd.setCursor(5 + in_num, 1);
      lcd.print("*");
      in_num++;
    }

    if (in_num == 5)
    {
      Serial.println(data_input);
      in_num = 0;
    }
  }
}

void checkPass() // kiem tra password
{
  getData();
  lcd.setCursor(0, 0);
  lcd.print("Enter Password: "); // ← ghi đè dòng 0
  if (isBufferdata(data_input))
  {
    if (compareData(data_input, password)) // Dung pass
    {
      lcd.clear();
      clear_data_input();
      index_t = 3;
    }
    else if (compareData(data_input, mode_changePass))
    {
      // Serial.print("mode_changePass");
      lcd.clear();
      clear_data_input();
      index_t = 1;
    }
    else if (compareData(data_input, mode_resetPass))
    {
      // Serial.print("mode_resetPass");
      lcd.clear();
      clear_data_input();
      index_t = 2;
    }
    else if (compareData(data_input, mode_hardReset))
    {
      lcd.setCursor(0, 0);
      lcd.print("---HardReset---");
      writeEpprom(pass_def);
      insertData(password, pass_def);
      clear_data_input();
      delay(2000);
      lcd.clear();
      index_t = 0;
    }
    else if (compareData(data_input, mode_addRFID))
    {
      lcd.clear();
      clear_data_input();
      index_t = 8;
    }
    else if (compareData(data_input, mode_delRFID))
    {
      lcd.clear();
      clear_data_input();
      index_t = 9;
    }
    else if (compareData(data_input, mode_delAllRFID))
    {
      lcd.clear();
      clear_data_input();
      index_t = 10;
    }
    else
    {
      if (error_pass == 2)
      {
        clear_data_input();
        lcd.clear();
        index_t = 4;
      }
      Serial.print("Error");
      lcd.clear();
      lcd.setCursor(1, 1);
      lcd.print("WRONG PASSWORD");
      clear_data_input();
      error_pass++;
      delay(1000);
      lcd.clear();
    }
  }
}

void openDoor()
{
  // Serial.println("Open The Door");
  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print("---OPENDOOR---");
  unsigned char pos;
  delay(1000);
  sg90.write(180);
  delay(5000);
  sg90.write(0);
  lcd.clear();
  index_t = 0;
}

// 🔒 Xử lý nhập sai 3 lần - khóa hệ thống 30 giây
void error()
{
  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print("WRONG 3 TIME");
  delay(2000);
  lcd.setCursor(1, 1);
  lcd.print("Wait 1 minutes");
  unsigned char minute = 0;
  unsigned char i = 30;
  char buff[3];
  while (i > 0)
  {
    if (i == 1 && minute > 0)
    {
      minute--;
      i = 59;
    }
    if (i == 1 && minute == 0)
    {
      break;
    }
    sprintf(buff, "%.2d", i);
    i--;
    delay(200);
  }
  lcd.clear();
  index_t = 0;
}

void changePass() // Thay đổi mật khẩu
{
  lcd.setCursor(0, 0);
  lcd.print("-- Change Pass --");
  delay(3000);
  lcd.setCursor(0, 0);
  lcd.print("--- New Pass ---");

  while (1)
  {
    getData();
    if (isBufferdata(data_input))
    {
      insertData(new_pass1, data_input);
      clear_data_input();
      break;
    }
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("---- AGAIN ----");

  while (1)
  {
    getData();
    if (isBufferdata(data_input))
    {
      insertData(new_pass2, data_input);
      clear_data_input();
      break;
    }
  }

  delay(1000);

  if (compareData(new_pass1, new_pass2)) // Nếu mật khẩu trùng khớp
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("--- Success ---");

    delay(1000);
    writeEpprom(new_pass2);
    insertData(password, new_pass2);

    lcd.clear();
    index_t = 0;
  }
  else // Nếu mật khẩu không trùng
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("-- Mismatched --");
    delay(1000);
    lcd.clear();
    index_t = 0;
  }
}

void resetPass()
{
  unsigned char choise = 0;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("RESET PASSWORD");

  getData();
  if (isBufferdata(data_input))
  {
    if (compareData(data_input, password))
    {
      lcd.clear();
      clear_data_input();

      while (1)
      {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("RESET PASSWORD");

        if (choise == 0)
        {
          lcd.setCursor(0, 1);
          lcd.print(">");
          lcd.setCursor(2, 1);
          lcd.print("YES");
          lcd.setCursor(9, 1);
          lcd.print(" ");
          lcd.setCursor(11, 1);
          lcd.print("NO");
        }
        else
        {
          lcd.setCursor(0, 1);
          lcd.print(" ");
          lcd.setCursor(2, 1);
          lcd.print("YES");
          lcd.setCursor(9, 1);
          lcd.print(">");
          lcd.setCursor(11, 1);
          lcd.print("NO");
        }

        char key = keypad.getKey();
        if (key == '*')
        {
          choise = (choise == 1) ? 0 : 1;
        }

        if (key == '#' && choise == 0) // YES - Reset mật khẩu
        {
          lcd.clear();
          delay(1000);
          writeEpprom(pass_def);
          insertData(password, pass_def);
          lcd.setCursor(0, 0);
          lcd.print("---Reset ok---");
          delay(1000);
          lcd.clear();
          break;
        }

        if (key == '#' && choise == 1) // NO - Thoát
        {
          lcd.clear();
          break;
        }
      }
      index_t = 0;
    }
    else
    {
      index_t = 0;
      lcd.clear();
    }
  }
}

unsigned char numberInput()
{
  char number[5];
  char count_i = 0;
  while (count_i < 2)
  {
    char key = keypad.getKey();
    if (key && key != 'A' && key != 'B' && key != 'C' && key != 'D' && key != '*' && key != '#')
    {
      delay(100);
      lcd.setCursor(10 + count_i, 1);
      lcd.print(key);
      number[count_i] = key;
      count_i++;
    }
  }
  return (number[0] - '0') * 10 + (number[1] - '0');
}

void checkEEPROM()
{
  Serial.println("🔍 Kiểm tra EEPROM lưu thẻ RFID...");

  for (int i = 10; i < 50; i += 4) // Duyệt qua từng thẻ RFID trong EEPROM
  {
    Serial.print("📌 ID ");
    Serial.print((i - 10) / 4 + 1); // Tính số thứ tự thẻ RFID
    Serial.print(": ");

    bool empty = true; // Biến kiểm tra nếu tất cả giá trị = 0

    for (int j = 0; j < 4; j++)
    {
      byte data = EEPROM.read(i + j);
      Serial.print(data, HEX);
      Serial.print(" ");

      if (data != 0xFF && data != 0x00) // Nếu có dữ liệu hợp lệ
      {
        empty = false;
      }
    }

    if (empty)
    {
      Serial.print(" -> (Trống)");
    }

    Serial.println();
  }
}

bool isAllowedRFIDTag(byte tag[])
{
  int count = 0;
  for (int i = 10; i < 512; i += 4)
  {
    Serial.print("EEPROM: ");
    for (int j = 0; j < 4; j++)
    {
      Serial.print(EEPROM.read(i + j), HEX);
      if (tag[j] == EEPROM.read(i + j))
      {
        count++;
      }
    }
    Serial.println();
    if (count == 4)
    {
      return true; // Thẻ đã tồn tại
    }
    count = 0;
  }
  return false;
}

void rfidCheck()
{
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial())
  {
    byte rfidTag[4];
    Serial.print("RFID TAG: ");
    for (byte i = 0; i < rfid.uid.size; i++)
    {
      rfidTag[i] = rfid.uid.uidByte[i];
      Serial.print(rfidTag[i], HEX);
    }
    Serial.println();

    if (isAllowedRFIDTag(rfidTag))
    {
      lcd.clear();
      index_t = 3;
    }
    else
    {
      if (error_pass == 2)
      {
        lcd.clear();
        index_t = 4;
      }
      Serial.print("Error\n");
      lcd.clear();
      lcd.setCursor(3, 1);
      lcd.print("WRONG RFID");
      error_pass++;
      delay(1000);
      lcd.clear();
    }
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }
}

void handleWrongRFID()
{
  lcd.clear();

  if (error_pass == 2)
  {
    lcd.setCursor(0, 0);
    lcd.print("SYSTEM");
    lcd.setCursor(0, 1);
    lcd.print("LOCKED");
    index_t = 4;
  }
  else
  {
    Serial.println("Error: Wrong RFID");
    lcd.setCursor(0, 0);
    lcd.print("WRONG RFID");
    error_pass++;
  }

  delay(1000);
}

// Khai báo biến toàn cục để lưu thẻ quét lần 1

void addRFID()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ADD NEW RFID");

  Serial.println("📌 ADD_RFID MODE");

  switch (MODE_RFID)
  {
  case MODE_ID_RFID_ADD:
  {
    Serial.println("📌 Nhập ID thẻ...");

    lcd.setCursor(0, 1);
    lcd.print("Input ID:");

    id_rf = numberInput();
    Serial.println(id_rf);

    if (id_rf == 0)
    {
      lcd.clear();
      lcd.setCursor(3, 1);
      lcd.print("ID ERROR");
      delay(2000);
    }
    else
    {
      MODE_RFID = MODE_ID_RFID_FIRST;
    }
  }
  break;

  case MODE_ID_RFID_FIRST:
  {
    Serial.println("🔄 Chờ quét thẻ lần 1...");
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print("Put RFID");

    unsigned long timeout = millis() + 10000;
    while (!rfid.PICC_IsNewCardPresent() && millis() < timeout)
    {
      Serial.println("⏳ Đang quét... chưa thấy thẻ.");
      delay(500);
    }

    if (millis() >= timeout)
    {
      Serial.println("⚠ Không phát hiện thẻ!");
      lcd.clear();
      lcd.setCursor(0, 1);
      lcd.print("NO CARD DETECTED");
      delay(2000);
      MODE_RFID = MODE_ID_RFID_ADD;
      return;
    }

    if (rfid.PICC_ReadCardSerial())
    {
      Serial.println("✅ Đã phát hiện thẻ! Đọc UID...");
      Serial.print("🆔 RFID TAG: ");

      for (byte i = 0; i < 4; i++)
      {
        firstScanTag[i] = rfid.uid.uidByte[i];
        Serial.print(firstScanTag[i], HEX);
        Serial.print(" ");
      }
      Serial.println();

      if (isAllowedRFIDTag(firstScanTag))
      {
        lcd.clear();
        lcd.setCursor(1, 1);
        lcd.print("RFID EXISTS");
        delay(2000);
        MODE_RFID = MODE_ID_RFID_ADD;
      }
      else
      {
        MODE_RFID = MODE_ID_RFID_SECOND;
      }
    }

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }
  break;

  case MODE_ID_RFID_SECOND:
  {
    Serial.println("🔄 Chờ quét thẻ lần 2...");
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print("Put Again");
    delay(1000);

    unsigned long timeout = millis() + 10000;
    while (!rfid.PICC_IsNewCardPresent() && millis() < timeout)
    {
      Serial.println("⏳ Đang quét lần 2...");
      delay(500);
    }

    if (millis() >= timeout)
    {
      Serial.println("⚠ Không phát hiện thẻ lần 2!");
      lcd.clear();
      lcd.setCursor(0, 1);
      lcd.print("NO CARD DETECTED");
      delay(2000);
      MODE_RFID = MODE_ID_RFID_ADD;
      return;
    }

    if (rfid.PICC_ReadCardSerial())
    {
      byte secondScanTag[4];
      Serial.print("🔍 RFID TAG (Lần 2): ");
      for (byte i = 0; i < 4; i++)
      {
        secondScanTag[i] = rfid.uid.uidByte[i];
        Serial.print(secondScanTag[i], HEX);
        Serial.print(" ");
      }
      Serial.println();

      if (memcmp(firstScanTag, secondScanTag, 4) != 0)
      {
        Serial.println("⚠️ Thẻ không khớp!");
        lcd.clear();
        lcd.setCursor(0, 1);
        lcd.print("MISMATCHED RFID");
        delay(2000);
        MODE_RFID = MODE_ID_RFID_ADD;
        return;
      }

      // ✅ Lưu vào EEPROM
      Serial.println("💾 Ghi EEPROM...");
      for (int i = 0; i < 4; i++)
      {
        EEPROM.write(10 + (id_rf - 1) * 4 + i, secondScanTag[i]);
      }
      EEPROM.commit();

      // ✅ Gửi dữ liệu lên server bằng GET
      if (WiFi.status() == WL_CONNECTED)
      {
        HTTPClient http;

        // Chuẩn bị các UID viết hoa
        String uid1 = String(secondScanTag[0], HEX);
        uid1.toUpperCase();
        String uid2 = String(secondScanTag[1], HEX);
        uid2.toUpperCase();
        String uid3 = String(secondScanTag[2], HEX);
        uid3.toUpperCase();
        String uid4 = String(secondScanTag[3], HEX);
        uid4.toUpperCase();

        String url = String(server_rfid_url) + "?action=add";
        url += "&id=" + String(id_rf);
        url += "&uid1=" + uid1;
        url += "&uid2=" + uid2;
        url += "&uid3=" + uid3;
        url += "&uid4=" + uid4;

        Serial.println("🔗 Gửi URL: " + url);
        http.begin(url);
        int responseCode = http.GET();
        String response = http.getString();

        Serial.print("📡 Mã phản hồi: ");
        Serial.println(responseCode);
        Serial.println("📤 Server Response: " + response);
        http.end();
      }
      else
      {
        Serial.println("⚠ Không có WiFi, bỏ qua gửi dữ liệu.");
      }

      // ✅ Hiển thị thông báo thành công
      lcd.clear();
      lcd.setCursor(0, 1);
      lcd.print("Add RFID Done");
      delay(2000);

      // Reset trạng thái
      MODE_RFID = MODE_ID_RFID_ADD;
      id_rf = 0;
      Serial.println("✅ ADD_OUT");
      index_t = 0; // ✅ THOÁT KHỎI chế độ addRFID sau khi hoàn tất
    }

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }
  break;
  }
}

void delRFID()
{
  char buffDisp[20];

  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("DELETE RFID");

  Serial.println("📌 DEL_IN");

  lcd.setCursor(0, 1);
  lcd.print("Input ID:");

  id_rf = numberInput(); // Nhận ID từ bàn phím

  if (id_rf == 0) // ID #0 không hợp lệ
  {
    lcd.clear();
    lcd.setCursor(3, 1);
    lcd.print("ID ERROR");
    delay(2000);
  }
  else
  {
    for (int i = 0; i < 4; i++)
    {
      EEPROM.write(10 + (id_rf - 1) * 4 + i, '\0'); // Xóa dữ liệu trong EEPROM
    }
    EEPROM.commit();

    if (WiFi.status() == WL_CONNECTED)
    {
      HTTPClient http;
      http.begin(server_rfid_url);
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");

      String postData = "action=delete&id=" + String(id_rf);
      int httpCode = http.POST(postData);
      String response = http.getString();

      Serial.println("🗑️ Delete RFID SQL: " + response);
      http.end();
    }

    sprintf(buffDisp, "Clear ID: %d Done", id_rf);

    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print(buffDisp);

    Serial.println("✅ DEL_OUT");
    delay(2000);
    lcd.clear();
    index_t = 0;
  }
}

void delAllRFID()
{
  char key = keypad.getKey();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("CLEAR ALL RFID?");

  if (key == '*')
  {
    isMode = 0; // Chọn YES
  }
  if (key == '#')
  {
    isMode = 1; // Chọn NO
  }

  if (isMode == 0) // Hiển thị lựa chọn YES
  {
    lcd.setCursor(0, 1);
    lcd.print("> Yes      No  ");
  }
  else if (isMode == 1) // Hiển thị lựa chọn NO
  {
    lcd.setCursor(0, 1);
    lcd.print("  Yes    > No  ");
  }

  if (key == '0' && isMode == 0) // Xác nhận xóa toàn bộ RFID
  {
    for (int i = 10; i < 512; i++)
    {
      EEPROM.write(i, '\0');
    }
    EEPROM.commit();

    if (WiFi.status() == WL_CONNECTED)
    {
      HTTPClient http;
      http.begin(server_rfid_url);
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");

      String postData = "action=delete_all";
      int httpCode = http.POST(postData);
      String response = http.getString();

      Serial.println("🧹 Delete ALL RFID SQL: " + response);
      http.end();
    }

    Serial.println("✅ Tất cả RFID đã bị xóa!");
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print("CLEAR DONE");
    delay(2000);
    lcd.clear();
    index_t = 0;
  }

  if (key == '0' && isMode == 1) // Thoát mà không xóa
  {
    lcd.clear();
    index_t = 0;
  }
}

void syncFromServer()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("⚠ Không có WiFi, bỏ qua sync.");
    return;
  }

  HTTPClient http;
  http.begin(server_rfid_url + String("?action=sync")); // Gửi GET request
  int httpCode = http.GET();

  if (httpCode == 200)
  {
    String payload = http.getString();
    Serial.println("📥 JSON từ server:");
    Serial.println(payload);

    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
      Serial.println("❌ Lỗi phân tích JSON");
      http.end();
      return;
    }

    // ✅ 1. Đồng bộ mật khẩu
    const char *serverPass = doc["password"];
    if (serverPass && strlen(serverPass) == 5)
    {
      bool pass_diff = false;
      for (int i = 0; i < 5; i++)
      {
        if (EEPROM.read(i) != serverPass[i])
        {
          pass_diff = true;
          break;
        }
      }

      if (pass_diff)
      {
        Serial.println("🔁 Mật khẩu khác → cập nhật EEPROM");
        for (int i = 0; i < 5; i++)
        {
          EEPROM.write(i, serverPass[i]);
          password[i] = serverPass[i];
        }
        EEPROM.commit();
      }
      else
      {
        Serial.println("✅ Mật khẩu giống → không thay đổi");
      }
    }
    else
    {
      Serial.println("⚠️ Không tìm thấy mật khẩu hợp lệ trong JSON");
    }

    // ✅ 2. Đồng bộ RFID
    JsonArray list = doc["rfid_list"];
    for (JsonObject rfid : list)
    {
      int id = atoi(rfid["id"]);
      int addr = 10 + (id - 1) * 4;
      byte uid_server[4];

      for (int i = 0; i < 4; i++)
      {
        String key = "uid" + String(i + 1);
        const char *hexStr = rfid[key];
        uid_server[i] = (byte)strtol(hexStr, NULL, 16);
      }

      bool rfid_diff = false;
      for (int i = 0; i < 4; i++)
      {
        if (EEPROM.read(addr + i) != uid_server[i])
        {
          rfid_diff = true;
          break;
        }
      }

      if (rfid_diff)
      {
        Serial.print("🔁 Ghi lại RFID ID ");
        Serial.println(id);
        for (int i = 0; i < 4; i++)
        {
          EEPROM.write(addr + i, uid_server[i]);
        }
      }
      else
      {
        Serial.print("✅ RFID ID ");
        Serial.print(id);
        Serial.println(" trùng khớp");
      }
    }

    EEPROM.commit();
    Serial.println("✅ Đồng bộ RFID & mật khẩu hoàn tất!");
    checkEEPROM();
  }
  else
  {
    Serial.print("❌ Lỗi kết nối server, mã lỗi: ");
    Serial.println(httpCode);
  }

  http.end();
}

void fetchSchedule()
{
  HTTPClient http;
  http.begin("http://192.168.137.74/api/control.php?esp=1");
  int httpCode = http.GET();

  if (httpCode == 200)
  {
    String payload = http.getString();
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, payload);

    if (!error)
    {
      String hour = String(doc[0]["pump_start_hour"].as<int>());
      String minute = String(doc[0]["pump_start_minute"].as<int>());
      String days = String(doc[0]["repeat_days"].as<String>());

      String timeStr = "Time: " + hour + ":" + (minute.length() == 1 ? "0" + minute : minute);
      String dayStr = "Days: " + days;

      lcd.setCursor(0, 2);
      lcd.print(timeStr + "      "); // padding xoá dòng cũ

      lcd.setCursor(0, 3);
      lcd.print(dayStr + "       ");
    }
    else
    {
      lcd.setCursor(0, 2);
      lcd.print("JSON Error         ");
      lcd.setCursor(0, 3);
      lcd.print("                   ");
    }
  }
  else
  {
    lcd.setCursor(0, 2);
    lcd.print("Fetch fail: " + String(httpCode));
    lcd.setCursor(0, 3);
    lcd.print("                   ");
  }

  http.end();
}

void setup()
{
  Serial.begin(115200);

  // 🔄 Kết nối WiFi
  WiFi.begin(ssid, password_wifi);
  Serial.print("🔄 Đang kết nối WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi đã kết nối!");
  Serial.print("📡 Địa chỉ IP: ");
  Serial.println(WiFi.localIP());
  sg90.attach(PIN_SG90); // 🛠️ Gắn servo vào chân GPIO 26

  EEPROM.begin(512);
  EEPROM.commit();
  checkEEPROM();

  // Khởi tạo SPI và RFID
  SPI.begin();
  rfid.PCD_Init();

  // ✅ Khởi tạo màn hình LCD 20x4
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("SYSTEM INIT...");
  delay(2000);
  lcd.clear();

  readEpprom();     // Đọc mật khẩu từ EEPROM
  syncFromServer(); // <=== GỌI ĐỒNG BỘ DỮ LIỆU TỪ SQL

  Serial.print("🔑 PASSWORD: ");
  Serial.println(password);

  // Nếu mật khẩu EEPROM chưa được thiết lập, ghi lại mật khẩu mặc định
  if (password[0] == 0xFF)
  {
    writeEpprom(pass_def);
    insertData(password, pass_def);
    Serial.print("🔑 PASSWORD (Mới): ");
    Serial.println(password);
  }
}

void loop()
{
  // ✅ Hiển thị giao diện nhập mật khẩu nếu đang ở trạng thái chờ
  if (index_t == 0 && lastUIState != "enter_password")
  {
    lcd.setCursor(0, 0);
    lcd.print("Enter Password:     "); // xóa dòng bằng padding
    lcd.setCursor(0, 1);
    lcd.print("                    "); // dòng trống
    lastUIState = "enter_password";
  }

  // ✅ Gọi check password & rfid
  checkPass();
  rfidCheck();

  // ✅ Cập nhật lịch mỗi 30 giây nếu đang ở trạng thái chờ
  if (index_t == 0 && millis() - lastScheduleFetch > 1000)
  {
    fetchSchedule();
    lastScheduleFetch = millis();
  }

  // ✅ Xử lý các trạng thái hệ thống
  switch (index_t)
  {
  case 1:
    changePass();
    index_t = 0;
    break;

  case 2:
    resetPass();
    index_t = 0;
    break;

  case 3:
    openDoor();
    error_pass = 0;
    index_t = 0;
    break;

  case 4:
    error();
    error_pass = 0;
    index_t = 0;
    break;

  case 8:
    addRFID();
    index_t = 0;
    break;

  case 9:
    delRFID();
    index_t = 0;
    break;

  case 10:
    delAllRFID();
    index_t = 0;
    break;
  }
}
