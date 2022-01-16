#ifndef CONSTANTS_H
#define CONSTANTS_H

// Display
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CS_PIN D4
#define CLK_PIN D5
#define DIN_PIN D7

// Server
#define SERVER_PORT   80

// OTA
#define OTA_PASSWORD  "*****"
#define OTA_NAME      "Twitch"

namespace Constants {
  namespace Twitch {
    const String API{"https://api.twitch.tv"};
    const String API_REGISTER{"https://id.twitch.tv"};
    const String CLIENT_ID{"client_id"};
    const String LOGIN{"twitch_login"};
    const String LOGIN_ID{"twitch_login_id"};
    const String CLIENT_SECRET{"client_secret"};
  }

  namespace Wifi {
    const String SSID{"*****"};
    const String PASSWORD{"*****"};
  }

  namespace Display {
    const uint8_t numberOfZones{2};
    uint8_t heart[] = {0x0A, 0x1C, 0x3E, 0x7E, 0xFC, 0xFC, 0x7E, 0x3E, 0x1C};
    uint8_t twitchIcon[] = {0x0A, 0x3F, 0x21, 0xE1, 0x8D, 0x41, 0x2D, 0x21, 0x1F};
    const uint8_t DEFAULT_SCROLL_SPEED{100};
    const uint16_t DEFAULT_SCROLL_PAUSE{5000};
    const textPosition_t DEFAULT_ALIGNEMENT{PA_CENTER};
    const textEffect_t DEFAULT_SCROLL_EFFECT{PA_PRINT};
  }
}

#endif /* CONSTANTS_H */
