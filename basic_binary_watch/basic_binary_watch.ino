// basic_binary_watch.ino
// version 0.1.1 | MIT License | https://github.com/shiza4za/basic_binary_watch/blob/main/LICENSE

#include <iostream>
#include <bitset>
#include <vector>
#include <esp_sntp.h>
#include <WiFi.h>
#include <M5Unified.h>





//////////////////////////// カスタマイズ ////////////////////////////

// ★NTP同期時に接続するWi-FiのSSID
constexpr const char* ssid = "your SSID";
// ★NTP同期時に接続するWi-Fiのpw
constexpr const char* ssid_pw = "your SSID password";

// ★デフォルトでデシマルを表示する/しない設定
bool BtnB_ck = false;   // デフォルト false

// ★背景色
const int color_lcd  = BLACK;   // デフォルト BLACK
// ★文字色(バッテリ以外)
const int color_text = WHITE;   // デフォルト WHITE
// ★バイナリ表示のフチの色
const int color_back = 0x31a6;  // デフォルト 0x31a6
// ★バイナリ表示オフの色
const int color_off  = 0x31a6;  // デフォルト 0x31a6
// ★バイナリ表示オンの色
const int color_on   = WHITE;   // デフォルト WHITE

// ★バッテリ表示の各文字色
const int color_bt_good   = 0x0723;   // 100-60 デフォルト緑 0x0723
const int color_bt_hmm    = 0xfd66;   //  59-20 デフォルト黄 0xfd66
const int color_bt_danger = 0xfa86;   //  19- 0 デフォルト赤 0xfa86

// ★自動終了するまでの時間(秒)
// 　※ミリ秒ではなく秒です
// 　※多分58秒未満を推奨
// 　※0にすると、勝手にオフしなくなります
#define BUTTON_TIMEOUT 30   // デフォルト 30

// ★BtnC押下時・または一定秒間操作がなかったときに、
// 　powerOffまたはdeepSleepする設定
// 　・trueでpowerOff関数実行。次回電源ボタンを押すと再起動
// 　・falseでdeepSleep関数実行。次回画面など触れると再起動
bool poweroffmode = true;   // デフォルト true

/////////////////////////////////////////////////////////////////////





// グローバル変数として宣言
auto start_time = time(nullptr);
auto start_time_local = localtime(&start_time);
int start_time_local_sec = start_time_local->tm_sec;
const int defy = 20;
const char* const week_str[7] = {"Sun", "Mon", "Tue", "Wed", "Thr", "Fri", "Sat"};





// バイナリ点滅以外の基本表示一式
void firstScreen() {
  M5.Lcd.clear(color_lcd);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.setTextColor(color_text, color_lcd);

  // バイナリ表示の背景円
  for (int n = 0; n < 12; n++) { M5.Display.fillCircle(304 - (n * 20), defy, 9, color_back); }
  for (int n = 0; n < 4 ; n++) { M5.Display.fillCircle(304 - (n * 20), defy+22, 9, color_back); }
  for (int n = 0; n < 5 ; n++) { M5.Display.fillCircle(304 - (n * 20), defy+22+22, 9, color_back); }
  for (int n = 0; n < 3 ; n++) { M5.Display.fillCircle(304 - (n * 20), defy+22+22+22, 9, color_back); }
  for (int n = 0; n < 6 ; n++) {
    M5.Display.fillCircle(300 - (n * 30), defy+22+22+22+39, 12, color_back);
    M5.Display.fillCircle(300 - (n * 30), defy+22+22+22+39+35, 12, color_back);
    M5.Display.fillCircle(300 - (n * 30), defy+22+22+22+39+35+35, 12, color_back);
  }

  // バイナリ表示のガイド値
  M5.Display.setTextSize(1);
  for (int n = 0; n < 12; n++) {
    int num = pow(2, n);
    if        (num < 10) {
      M5.Lcd.setCursor(304 - (n * 20) - 2, 0);
      M5.Display.printf("%d", num);
    } else if (num >= 10 && num < 100) {
      M5.Lcd.setCursor(304 - (n * 20) - 5, 0);
      M5.Display.printf("%d", num);
    } else if (num >= 100 && num < 1000) {
      M5.Lcd.setCursor(304 - (n * 20) - 8, 0);
      M5.Display.printf("%d", num);
    }
  }
  for (int n = 0; n < 12; n++) {
    int num = pow(2, n);
    if        (num < 10) {
      M5.Lcd.setCursor(300 - (n * 30) - 2, defy+22+22+22+16);
      M5.Display.printf("%d", num);
    } else if (num >= 10 && num < 64) {
      M5.Lcd.setCursor(300 - (n * 30) - 5, defy+22+22+22+16);
      M5.Display.printf("%d", num);
    }
  }

  // ボタン説明
  M5.Display.setTextSize(2);
  M5.Display.setCursor(55, 16*14);
  M5.Display.printf    ("NTP  BIN/DEC  Shutdown");
  //      Battery-> |100                       |
  //       Button-> |x xAA x x x BBx x x xCC x |
}





// BtnAで呼出：RTC確認 → Wi-Fi接続 → NTPサーバ接続 → 時刻同期
void connect() {
  M5.Lcd.clear(color_lcd);
  M5.Lcd.setCursor(0, 0);

  // バージョン
  M5.Lcd.setTextColor(0xad55, color_lcd);
  M5.Display.printf("\nver 0.1.0\n\n");

  // SSIDの参考表示
  M5.Lcd.setTextColor(color_text, color_lcd);
  M5.Display.printf("SSID: ");
  M5.Display.printf("%s\n\n", ssid);
  vTaskDelay(1000);

  // RTC状態表示
  M5.Display.printf("RTC...");
  if (!M5.Rtc.isEnabled()) {
    M5.Lcd.setTextColor(0xfa86, color_lcd);
    M5.Display.printf("ERR. \nPlease power off \nand try again with the \nRTC available.\n\n");
    for (;;) { vTaskDelay(10000); }
  }
  M5.Lcd.setTextColor(0x0723, color_lcd);
  M5.Display.printf("OK.\n\n");

  // Wi-Fi接続
  WiFi.disconnect();
  vTaskDelay(1000);
  M5.Lcd.setTextColor(color_text, color_lcd);
  M5.Display.printf("Wi-Fi...");
  WiFi.begin(ssid, ssid_pw);
  while (WiFi.status() != WL_CONNECTED) { M5.Display.printf("."); vTaskDelay(500); }
  M5.Lcd.setTextColor(0x0723, color_lcd);
  M5.Display.printf("OK.\n\n");
  vTaskDelay(1000);

  // NTP時刻取得
  M5.Lcd.setTextColor(color_text, color_lcd);
  M5.Display.printf("NTP...");
  configTzTime("JST-9", "ntp.nict.jp", "ntp.nict.jp", "ntp.nict.jp");
  while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) {
    M5.Display.printf("."); vTaskDelay(500);
  }
  M5.Lcd.setTextColor(0x0723, color_lcd);
  M5.Display.printf("OK.\n\n");
  vTaskDelay(1000);
  time_t get_time = time(nullptr);
  tm* local_time = localtime(&get_time);
  local_time->tm_hour += 9;
  time_t jst = mktime(local_time);
  tm* jstTime = gmtime(&jst);
  M5.Rtc.setDateTime(gmtime(&jst));
  vTaskDelay(1000);

  firstScreen();
}





// 起動オフの処理選択
void poweroffTask() {
  if        (poweroffmode == true) {
    M5.Power.powerOff();
  } else if (poweroffmode == false) {
    M5.Power.deepSleep(0);
  }
}



// ////////////////////////////////////////////////////////////////////////////////



void setup() {
  auto cfg = M5.config();

  // ★外部のRTCを読み取る場合は、コメント外します
  // 　Core2はRTC内蔵しているので不要
  // cfg.external_rtc  = true;

  M5.begin(cfg);
  M5.Displays(0).setTextSize(2);

  firstScreen();

  // 操作なし時間確認用 基準秒
  start_time = time(nullptr);
  start_time_local = localtime(&start_time);
  start_time_local_sec = start_time_local->tm_sec;
}



// ////////////////////////////////////////////////////////////////////////////////



void loop() {
  vTaskDelay(5);
  M5.Lcd.setCursor(0, 0);
  M5.update();

  auto now_time = time(nullptr);
  auto now_time_local = localtime(&now_time);


  // 操作なし時間確認、経過したら起動オフ
  int now_time_local_sec = now_time_local->tm_sec;

  if (start_time_local_sec > now_time_local_sec) {
    now_time_local_sec += 60;
  }
  if (BUTTON_TIMEOUT != 0) {
    if (now_time_local_sec - start_time_local_sec > BUTTON_TIMEOUT) {
      poweroffTask();
    }
  }




  // ボタン操作
    auto get_detail = M5.Touch.getDetail(1);
    // m5::touch_state_t get_state;
    // get_state = get_detail.state;

    // BtnA ちょっと長押しでWi-Fi・NTPサーバ接続開始
    if (M5.BtnA.pressedFor(500)) {
      connect();
      start_time_local_sec = now_time_local_sec;
    }

    // BtnB or タッチでデシマル表示/非表示
    if (M5.BtnB.wasClicked() || get_detail.wasClicked()) {
      if (BtnB_ck == false) {
        BtnB_ck = true;
        start_time_local_sec = now_time_local_sec;
      } else if (BtnB_ck == true) {
        BtnB_ck = false;
        start_time_local_sec = now_time_local_sec;
        // BtnBで呼び出したデシマル表示を塗り潰して無理やり非表示
        M5.Display.fillRect(0, 0, 60, 222, color_lcd);
      }
    }

    // BtnC ちょっと長押しで起動オフ
    if (M5.BtnC.pressedFor(500)) {
      poweroffTask();
    }
  //





  int year = now_time_local->tm_year+1900;
  int month = now_time_local->tm_mon+1;
  int day = now_time_local->tm_mday;
  int week = now_time_local->tm_wday;
  int hour = now_time_local->tm_hour;
  int min = now_time_local->tm_min;
  int sec = now_time_local->tm_sec;


  // デシマル表示出力
    if (BtnB_ck == true) {
      M5.Lcd.setCursor(7, defy-6);
      M5.Display.printf("%04d", year);
      M5.Lcd.setCursor(7, defy-6+22);
      M5.Display.printf("  %02d", month);
      M5.Lcd.setCursor(7, defy-6+22+22);
      M5.Display.printf("  %02d", day);
      M5.Lcd.setCursor(7, defy-6+22+22+22);
      M5.Display.printf(" %s",   week_str[week]);
      M5.Display.setTextSize(3);
      M5.Lcd.setCursor(20, defy-7+22+22+22+36);
      M5.Display.printf("%02d", hour);
      M5.Lcd.setCursor(20, defy-7+22+22+22+36+35);
      M5.Display.printf("%02d", min);
      M5.Lcd.setCursor(20, defy-7+22+22+22+36+35+35);
      M5.Display.printf("%02d", sec);
      M5.Display.setTextSize(1);
    }
  //


  // 10進数時間を2進数にしてベクタへ
    M5.Lcd.setCursor(0, 0);

    std::bitset<12> binary_year(year);
    std::vector<int> binary_vec_year;
    for (int i = 0; i < 12; i++) {
      binary_vec_year.push_back(binary_year[i]);
    }
    std::bitset<6> binary_month(month);
    std::vector<int> binary_vec_month;
    for (int i = 0; i < 4; i++) {
      binary_vec_month.push_back(binary_month[i]);
    }
    std::bitset<6> binary_day(day);
    std::vector<int> binary_vec_day;
    for (int i = 0; i < 5; i++) {
      binary_vec_day.push_back(binary_day[i]);
    }
    std::bitset<6> binary_week(week);
    std::vector<int> binary_vec_week;
    for (int i = 0; i < 3; i++) {
      binary_vec_week.push_back(binary_week[i]);
    }
    std::bitset<6> binary_hour(hour);
    std::vector<int> binary_vec_hour;
    for (int i = 0; i < 6; i++) {
      binary_vec_hour.push_back(binary_hour[i]);
    }
    std::bitset<6> binary_min(min);
    std::vector<int> binary_vec_min;
    for (int i = 0; i < 6; i++) {
      binary_vec_min.push_back(binary_min[i]);
    }
    std::bitset<6> binary_sec(sec);
    std::vector<int> binary_vec_sec;
    for (int i = 0; i < 6; i++) {
      binary_vec_sec.push_back(binary_sec[i]);
    }
  //



  // バイナリ表示出力
    for (int n = 0; n < 12; n++) {
      if (binary_vec_year[n] == 0) {
        M5.Display.fillCircle(304 - (n * 20), defy, 7, color_off);
      } else if (binary_vec_year[n] == 1) {
        M5.Display.fillCircle(304 - (n * 20), defy, 7, color_on);
      }
    }
    for (int n = 0; n < 4; n++) {
      if (binary_vec_month[n] == 0) {
        M5.Display.fillCircle(304 - (n * 20), defy+22, 7, color_off);
      } else if (binary_vec_month[n] == 1) {
        M5.Display.fillCircle(304 - (n * 20), defy+22, 7, color_on);
      }
    }
    for (int n = 0; n < 5; n++) {
      if (binary_vec_day[n] == 0) {
        M5.Display.fillCircle(304 - (n * 20), defy+22+22, 7, color_off);
      } else if (binary_vec_day[n] == 1) {
        M5.Display.fillCircle(304 - (n * 20), defy+22+22, 7, color_on);
      }
    }
    for (int n = 0; n < 3; n++) {
      if (binary_vec_week[n] == 0) {
        M5.Display.fillCircle(304 - (n * 20), defy+22+22+22, 7, color_off);
      } else if (binary_vec_week[n] == 1) {
        M5.Display.fillCircle(304 - (n * 20), defy+22+22+22, 7, color_on);
      }
    }

    for (int n = 0; n < 6; n++) {
      if (binary_vec_hour[n] == 0) {
        M5.Display.fillCircle(300 - (n * 30), defy+22+22+22+39, 10, color_off);
      } else if (binary_vec_hour[n] == 1) {
        M5.Display.fillCircle(300 - (n * 30), defy+22+22+22+39, 10, color_on);
      }
      if (binary_vec_min[n] == 0) {
        M5.Display.fillCircle(300 - (n * 30), defy+22+22+22+39+35, 10, color_off);
      } else if (binary_vec_min[n] == 1) {
        M5.Display.fillCircle(300 - (n * 30), defy+22+22+22+39+35, 10, color_on);
      }
      if (binary_vec_sec[n] == 0) {
        M5.Display.fillCircle(300 - (n * 30), defy+22+22+22+39+35+35, 10, color_off);
      } else if (binary_vec_sec[n] == 1) {
        M5.Display.fillCircle(300 - (n * 30), defy+22+22+22+39+35+35, 10, color_on);
      }
    }

  M5.Display.setTextSize(2);
  //



  // バッテリ残量表示
  // 100-60 緑
  //  59-20 黄
  //  19- 0 赤
  int battery_level = M5.Power.getBatteryLevel();
  M5.Lcd.setCursor(0, 16*14);
  if      (battery_level <= 100 && battery_level > 60) { M5.Lcd.setTextColor(color_bt_good, color_lcd); }
  else if (battery_level <=  60 && battery_level > 20) { M5.Lcd.setTextColor(color_bt_hmm, color_lcd); }
  else if (battery_level <=  20 && battery_level >  0) { M5.Lcd.setTextColor(color_bt_danger, color_lcd); }
  M5.Display.printf("%03d", battery_level);
  M5.Lcd.setTextColor(color_text, color_lcd);
}
