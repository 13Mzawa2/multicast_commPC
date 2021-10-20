#include <M5Stack.h>

class MyUtility
{
    public:
    
    String name = String("default");
    const uint16_t M5Lcd_width = 320;
    const uint16_t M5Lcd_height = 240;
    const uint16_t statusbar_height = 24;

    MyUtility() = default;
    MyUtility(const char* new_name)
    {
        name = String(new_name);
    }

    void drawBar()
    {
        M5.Lcd.fillRect(0, 0, M5Lcd_width, statusbar_height, NAVY);
    }

    void drawBatteryState()
    {
        const uint16_t bat_width = 40;
        const uint16_t bat_height = 20;
        const uint16_t bat_offset = 10;
        const uint16_t bat_posterm_w = 4;
        const uint16_t bat_posterm_h = 10;
        const uint16_t bat_area_x = M5Lcd_width - bat_offset - bat_width;
        const uint16_t bat_area_y = 2;
        const uint16_t bat_posterm_x = bat_area_x - bat_posterm_w;
        const uint16_t bat_posterm_y = bat_area_y + (bat_height - bat_posterm_h)/2;

        // バッテリー領域のLCD表示クリア
        M5.Lcd.fillRect(bat_area_x, bat_area_y, bat_width, bat_height, NAVY);
        // バッテリー量
        float batLevel = M5.Power.getBatteryLevel();
        uint16_t bat_width_now = (uint16_t)(bat_width * batLevel / 100.f);
        uint16_t batColor = (M5.Power.isChargeFull()) ? ORANGE :
                            (batLevel > 26) ? GREEN :
                            YELLOW;
        M5.Lcd.fillRect(bat_area_x + bat_width - bat_width_now, bat_area_y, bat_width_now, bat_height, batColor);
        M5.Lcd.setCursor(bat_posterm_x - 60, 4, 2);
        M5.Lcd.setTextColor(WHITE, NAVY);
        M5.Lcd.printf("%d %%", (uint16_t)batLevel);
        if(M5.Power.isCharging()){
            M5.Lcd.printf(" C");
        }
        // 電池形状
        M5.Lcd.drawRect(bat_area_x, bat_area_y, bat_width, bat_height, WHITE);
        M5.Lcd.fillRect(bat_posterm_x, bat_posterm_y, bat_posterm_w, bat_posterm_h, WHITE);

        M5.Lcd.setCursor(0, statusbar_height);
        M5.Lcd.setTextColor(WHITE, BLACK);
    }

    String getMACAddressString(const uint8_t *mac_addr, bool hide = false)
    {
        char macStr[ESP_NOW_ETH_ALEN * 3];
        if(hide){
            snprintf(macStr, sizeof(macStr), "XX:XX:XX:XX:XX:%02X", mac_addr[5]);
        }
        else{
            snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        }
        return String(macStr);
    }

    void printInBar(String str)
    {
        M5.Lcd.setCursor(0, 4, 2);
        M5.Lcd.setTextColor(WHITE, NAVY);
        M5.Lcd.print(str);
        
        M5.Lcd.setCursor(0, statusbar_height);
        M5.Lcd.setTextColor(WHITE, BLACK);
    }
};