/**************************************************************************
 * @file multicast_commPC.ino
 * @brief multicast master using ESP-NOW for M5Stack
 * @author 13Mzawa2
 * @details
 * PCとBluetoothでシリアル接続し，PCからの命令を他のM5デバイスにキャストする．
 * ESP-NOWはWiFi通信のIEEE802.11規格を拡張した通信方式だが，
 * WiFiアクセスポイントに接続せずに機器同士(peer-to-peer)で直接通信ができる．
 * @note 参考：https://lang-ship.com/blog/work/m5stickc-esp-now-1/
 * @note ESP-NOW Documentation : https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_now.html
 **************************************************************************/

#include <M5Stack.h>
#include <esp_now.h>
#include <WiFi.h>
#include <BluetoothSerial.h>
#include "MyUtility.h"

#define SLAVE_NUM 3
#define SLAVE_ID_UNKNOWN 0xff
#define MSG_THIS_IS_MASTER ((uint8_t)'M')
#define MSG_THIS_IS_SLAVE  ((uint8_t)'S')

BluetoothSerial SerialBT;
MyUtility util("undefined");
uint8_t addr_master[ESP_NOW_ETH_ALEN];
uint8_t addr_slaves[SLAVE_NUM][ESP_NOW_ETH_ALEN];
const uint8_t addr_broadcast[ESP_NOW_ETH_ALEN] = {0xff,0xff,0xff,0xff,0xff,0xff};
bool this_is_master = false;
bool this_is_slave = false;
uint8_t slave_id = SLAVE_ID_UNKNOWN;
uint8_t slave_id_counter = 0;

/**
 * @brief copy MAC address
 * @param[in]  src source (copy from)
 * @param[out] dst destination (copy to)
 * @details
 * please allocate memory of dst[ESP_NOW_ETH_ALEN] before using
 */
void addressCopy(const uint8_t* src, uint8_t* dst){
  for(int i=0; i<ESP_NOW_ETH_ALEN; i++){ dst[i] = src[i]; }
}

/**
 * @brief draw or update indicator bar
 */
void drawIndicatorBar(){
  auto str = String(util.name) + String(" ") + String(WiFi.macAddress());
  drawIndicatorBar(str);
}
/**
 * @brief draw or update indicator bar with string
 * @param[in] str message to print in indicator bar
 */
void drawIndicatorBar(String str){
  util.drawBar();
  util.printInBar(str);
  util.drawBatteryState();
}

/**
 * @brief 低頻度でインジケータの更新
 */
void updateIndicatorBar(uint16_t countup){
  static uint16_t indicatorUpdateCounter = 0;
  if(indicatorUpdateCounter > countup){
    drawIndicatorBar();
    indicatorUpdateCounter = 0;
  }
  indicatorUpdateCounter ++;
}

/**
 * @brief print the list of slave ID and its MAC address
 */ 
void printSlaveList(){
  for(int i=0; i<=slave_id_counter; i++){
    String str = String("Slave ") + String(i) + String(" : ") + util.getMACAddressString(addr_slaves[i]);
    if(i == slave_id) { str += String(" (ME)"); }
    M5.Lcd.println(str);
  }
}

/**
 * @brief MACアドレスからPeer情報を生成して登録する
 * @param[in] mac_addr 登録する機器のMACアドレス
 */
esp_err_t registerPeerInfo(uint8_t *mac_addr){
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  addressCopy(mac_addr, peerInfo.peer_addr);
  peerInfo.channel = 0;      // 通信に用いるWiFiチャンネル．0の場合，現在のチャンネルを使用する．
  peerInfo.encrypt = false;  // 通信の暗号化をする場合はtrueにして，暗号化キーをpeerInfo.lmkに入れる
  return esp_now_add_peer(&peerInfo);
}

/**
 * @brief 送信コールバック
 * @param[in] mac_addr 送信先のMACアドレス
 * @param[in] status   送信結果ステータス
 * @details
 * 送信完了後に送信結果をstatusに受信し，このコールバックが呼ばれる．
 * ブロードキャスト宛(FF:FF:FF:FF:FF:FF)に送信した場合は結果が当てにならないが，
 * 個別MACアドレス宛の場合は送信できたか否かを判定することができる．
 */
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  auto macStr = util.getMACAddressString(mac_addr);
  // 画面にも描画
  M5.Lcd.fillScreen(BLACK);
  drawIndicatorBar();
  M5.Lcd.print("Last Packet Sent to: \n  ");
  M5.Lcd.println(macStr);
  M5.Lcd.print("Last Packet Send Status: \n  ");
  M5.Lcd.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

/**
 * @brief 受信コールバック
 * @param[in] mac_addr 受信元のMACアドレス
 * @param[in] data     受信データのバイト列
 * @param[in] data_len 受信データ長
 * @details
 * 受信時に呼ばれる．ESP-NOWのライブラリではポーリング受信は不可能で，
 * 受信コールバック関数に受信時の挙動を記述する必要がある．
 */
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
  auto macStr = util.getMACAddressString(mac_addr);
          
  // 画面にも描画
  M5.Lcd.fillScreen(BLACK);
  drawIndicatorBar();
  M5.Lcd.print("Last Packet Recv from: \n  ");
  M5.Lcd.println(macStr);
  M5.Lcd.printf("Last Packet Recv Data(%d): \n  ", data_len);
  M5.Lcd.printf("%s", data);
}

/**
 * @brief 初期設定用の送信コールバック
 * @param[in] mac_addr 送信先のMACアドレス
 * @param[in] status   送信結果ステータス
 * @details
 * 送信完了後に送信結果をstatusに受信し，このコールバックが呼ばれる．
 * ブロードキャスト宛(FF:FF:FF:FF:FF:FF)に送信した場合は結果が当てにならないが，
 * 個別MACアドレス宛の場合は送信できたか否かを判定することができる．
 */
void OnDataSentForRegistration(const uint8_t *mac_addr, esp_now_send_status_t status) {
    return;
}

/**
 * @brief 初期設定用の受信コールバック
 * @param[in] mac_addr 受信元のMACアドレス
 * @param[in] data     受信データのバイト列
 * @param[in] data_len 受信データ長
 * @details
 * Master信号がブロードキャストされたとき，そのMACアドレスをマスターとして記録し，自分はスレーブとなる．
 * Slave信号がブロードキャストされたとき，そのMACアドレスをスレーブとして記録し，カウンターを進める．
 */
void OnDataRecvForRegistration(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
  // MACアドレスをString化
  auto macAddrString = util.getMACAddressString(mac_addr);
  if(data_len > 0){
    // Master宣言がブロードキャストされた場合
    if(data[0] == MSG_THIS_IS_MASTER) {
      addressCopy(mac_addr, addr_master);
      this_is_master = false;
      this_is_slave = true;
      slave_id_counter = 0;
      util.name = "Slave X";
      // 画面にマスターのMACアドレスを表示
      M5.Lcd.fillScreen(BLACK);
      drawIndicatorBar();
      M5.Lcd.print("Arrived from master: \n  ");
      M5.Lcd.println(macAddrString);
    }
    // Slave宣言がブロードキャストされた場合
    else if(data[0] == MSG_THIS_IS_SLAVE){
      addressCopy(mac_addr, addr_slaves[slave_id_counter]);
      // 画面にこれまで得られたスレーブのリストを表示
      M5.Lcd.fillScreen(BLACK);
      drawIndicatorBar();
      M5.Lcd.print("Arrived from slaves: \n");
      printSlaveList();
      slave_id_counter ++;
    }
  }
}

/**
 * @brief initial settings
 * @details
 * 電源投入直後の初期設定．
 * この時点ではマスターモードかスレーブモードかが決まっていない．
 */
void initESPNOW(){
  // ESP-NOW初期化
  WiFi.begin();
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() == ESP_OK) {
    M5.Lcd.print("ESPNow Init Success\n");
  } else {
    M5.Lcd.print("ESPNow Init Failed\n");
    ESP.restart();
  }
  // ブロードキャスト用Peer情報を登録
  if(registerPeerInfo((uint8_t*)addr_broadcast) != ESP_OK){
    M5.Lcd.println("Faild to add peer");
  }
  
  // ESP-NOWコールバック登録
  esp_now_register_send_cb(OnDataSentForRegistration);
  esp_now_register_recv_cb(OnDataRecvForRegistration);

  M5.Lcd.print("Press button A (leftside) and register this as a master.\n");

  // すべてのスレーブが確定するまで初期設定モード
  while(slave_id_counter < SLAVE_NUM){
    loopForInitialSettings();
  }

  delay(1000);

  // ESP-NOWコールバック登録解除
  esp_now_unregister_send_cb();
  esp_now_unregister_recv_cb();

  // 設定完了後のコールバック関数登録
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);

  // 自身がマスターの場合，認識したSlaveを登録する
  if(this_is_master){
    for(int i=0; i<SLAVE_NUM; i++){
      registerPeerInfo(addr_slaves[i]);
    }
  }
  // 自身がスレーブの場合，認識したMasterを登録する
  else{
    registerPeerInfo(addr_master);
  }
}

void loopForInitialSettings(){
  M5.update();

  if(M5.BtnA.wasPressed()){
    // ボタンを押した瞬間に，まだマスターが決まっていなければ自分がマスターと宣言する．
    if(!this_is_master && !this_is_slave){
      this_is_master = true;
      this_is_slave = false;
      uint8_t data[] = {MSG_THIS_IS_MASTER};
      esp_err_t result = esp_now_send(addr_broadcast, data, sizeof(data));
      if (result == ESP_OK) {
        // 自身のMACアドレスをMasterとして登録
        uint8_t macEN[ESP_NOW_ETH_ALEN];
        esp_read_mac(macEN, ESP_MAC_WIFI_STA);
        addressCopy(macEN, addr_master);
        util.name = "Master";
        M5.Lcd.fillScreen(BLACK);
        drawIndicatorBar();
        M5.Lcd.print("This is MASTER! \n");
      }
    }
    // 自分がスレーブと確定している場合，自分のスレーブIDを確定させて知らせる．
    else if(this_is_slave && slave_id == SLAVE_ID_UNKNOWN){
      slave_id = slave_id_counter;
      // slave宣言をブロードキャスト
      uint8_t data[] = {MSG_THIS_IS_SLAVE};
      esp_err_t result = esp_now_send(addr_broadcast, data, sizeof(data));
      if (result == ESP_OK) {
        // 自身のMACアドレスを登録して表示
        uint8_t macEN[ESP_NOW_ETH_ALEN];
        esp_read_mac(macEN, ESP_MAC_WIFI_STA);
        addressCopy(macEN, addr_slaves[slave_id]);
        util.name = String("Slave ") + String(slave_id);
        M5.Lcd.fillScreen(BLACK);
        drawIndicatorBar();
        M5.Lcd.printf("This is SLAVE No.%d ! \n", slave_id);
        printSlaveList();
        slave_id_counter ++;
      }
    }
  }
  delay(1);
  updateIndicatorBar(10000);
}

/**
 * @brief initialize bluetooth serial (master only)
 */
void initBluetooth(){
  // MACアドレス下2桁を使用して固有のデバイスID名を作成
  char bluetoothName[13];
  snprintf(bluetoothName, sizeof(bluetoothName), "M5Stack-%02X%02X", addr_master[4], addr_master[5]);
  // Bluetooth開始
  if(SerialBT.begin(bluetoothName)) {
    M5.Lcd.println("Bluetooth serial is now open.");
  }
}

void setup() {
  M5.begin();
  M5.Lcd.fillScreen(BLACK);
  // インジケータ表示
  M5.Power.begin();
  drawIndicatorBar();
  // 初期化
  M5.Lcd.print("ESP-NOW Test\n");
  initESPNOW();
  if(this_is_master){
    initBluetooth();
  }
  M5.Lcd.println("READY");
}

void loop() {
  M5.update();

  // ボタンを押したら送信
  uint8_t data[] = "hello!";
  if ( M5.BtnA.wasPressed() ) {
      esp_now_send(addr_slaves[0], data, sizeof(data));
  }
  if ( M5.BtnB.wasPressed() ) {
      esp_now_send(addr_slaves[1], data, sizeof(data));
  }
  if ( M5.BtnC.wasPressed() ) {
      esp_now_send(addr_slaves[2], data, sizeof(data));
  }
  // Bluetooth通信
  if(SerialBT.available()) {
    char c = SerialBT.read();
    uint8_t data[] = "got a message";
    switch(c){
      case '0':
        M5.Lcd.fillScreen(BLACK);
        drawIndicatorBar();
        M5.Lcd.println("got a message");
        break;
      case '1':
        esp_now_send(addr_slaves[0], data, sizeof(data));
        break;
      case '2':
        esp_now_send(addr_slaves[1], data, sizeof(data));
        break;
      case '3':
        esp_now_send(addr_slaves[2], data, sizeof(data));
        break;
    }
  }
  delay(1);
  updateIndicatorBar(10000);
}