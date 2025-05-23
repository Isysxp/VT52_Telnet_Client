//#define U8G2_USE_LARGE_FONTS
#include <U8g2lib.h>
#include <Arduino_GFX_Library.h>
#include "src/FreeMono9pt7b_New.h"
#include <WiFi.h>
#include <string.h>

void ParseTelnet(WiFiClient TCPclient);
void RawTelnet(WiFiClient TCPclient);
#define GFX_DEV_DEVICE WAVESHARE_ESP32_S3_TFT_4_3
#define GFX_BL 2
const char* ssid = "RaspiAP";       // CHANGE TO YOUR WIFI SSID
const char* password = "c531a3d358";  // CHANGE TO YOUR WIFI PASSWORD
const int serverPort = 23;
const char* nhost = "192.168.1.227";
int keepAlive=1000;  // Milliseconds


Arduino_ESP32RGBPanel* rgbpanel = new Arduino_ESP32RGBPanel(
  5 /* DE */,
  3 /* VSYNC */,
  46 /* HSYNC */,
  7 /* PCLK */,

  1 /* R0 */,
  2 /* R1 */,
  42 /* R2 */,
  41 /* R3 */,
  40 /* R4 */,

  39 /* G0 */,
  0 /* G1 */,
  45 /* G2 */,
  48 /* G3 */,
  47 /* G4 */,
  21 /* G5 */,

  14 /* B0 */,
  38 /* B1 */,
  18 /* B2 */,
  17 /* B3 */,
  10 /* B4 */,

  0 /* hsync_polarity */, 40 /* hsync_front_porch */, 48 /* hsync_pulse_width */, 88 /* hsync_back_porch */,
  0 /* vsync_polarity */, 13 /* vsync_front_porch */, 3 /* vsync_pulse_width */, 32 /* vsync_back_porch */,
  1 /* pclk_active_neg */, 16000000 /* prefer_speed */
);

//void GFXdraw16bitBeRGBBitmap(int16_t x1, int16_t y1, uint16_t* full, int16_t w, int16_t h);

Arduino_RGB_Display* gfx = new Arduino_RGB_Display(
  800 /* width */,
  480 /* height */,
  rgbpanel,
  0 /* rotation */,
  true /* auto_flush */
);
uint8_t* fb;
int lnum, altchar = 0, tflg = 0, cflg = 0, cenb = 1, escz = 0;
WiFiClient TCPclient;
int sz = 800 * 480 * 2;
char cbuf=0;
char lstc,*inchar=&cbuf;
extern uint8_t cplock;

hw_timer_t* Timer0_Cfg = NULL;

void IRAM_ATTR Timer0_ISR() {
  tflg++;
}

void start_display(bool *online ) {
char bfr[128];
  // Init Display
#ifdef GFX_EXTRA_PRE_INIT
  GFX_EXTRA_PRE_INIT();
#endif
  if (!gfx->begin()) {
    Serial.println("gfx->begin() failed!");
  }
  gfx->fillScreen(BLACK);
  gfx->setFont(&FreeMono9pt7b);
  gfx->setTextSize(1, 1);
#ifdef GFX_BL
  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH);
#endif
  gfx->fillScreen(BLACK);
  fb = (uint8_t*)gfx->getFramebuffer();
  gfx->setTextColor(DARKGREEN, DARKGREEN);
  lnum = 1;
  gfx->setCursor(0, (lnum)*20);

  // Connect to Wi-Fi
  WiFi.disconnect(true);
  //Serial.flush();
  delay(3000);
  WiFi.useStaticBuffers(true);
  WiFi.setMinSecurity(WIFI_AUTH_WPA2_PSK);
  WiFi.setSleep(WIFI_PS_NONE);
  WiFi.setHostname("ESP-Terminal");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  WiFi.dnsIP();
  Serial.println("Connected to WiFi");
  WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), IPAddress(192,168,1,254)); 
  Serial.println("\e[?2lMode changed to VT52");
  Serial.println("IP address:" + WiFi.localIP().toString());
  IPAddress hip(192,168,1,191);;
  WiFi.hostByName(nhost, hip);
  sprintf(bfr,"Connected to:%s\r\n",nhost);
  Display_String(bfr);
  Serial.println("Host:" + hip.toString());
  TCPclient.setNoDelay(false);
  TCPclient.connect(hip, serverPort);
  while (!TCPclient.connected())
    delay(10);


  Timer0_Cfg = timerBegin(1000000);
  timerAttachInterrupt(Timer0_Cfg, &Timer0_ISR);
  timerAlarm(Timer0_Cfg, 300000, true, 0);
  while(*online)
    disp_loop();
  timerEnd(Timer0_Cfg);
  TCPclient.stop();
  Serial.println("Keyboard disconnect ... resetting ESP32");
  ESP.restart();
}

void scrollUp(int ofs) {
  memcpy(fb + ofs, fb + ofs + 40 * 800, sz - (40 * 800 + ofs));
  memset(fb + (40 * 23) * 800, 0, 800 * 40);
  Cache_WriteBack_Addr((uint32_t)fb, sz);
}

void scrollDown(int ofs) {
  memmove(fb + ofs + 40 * 800, fb + ofs, sz - (40 * 800 + ofs));
  memset(fb + ofs, 0, 800 * 40);
  Cache_WriteBack_Addr((uint32_t)fb, sz);
}

char recv_char() {
  char ch;

  while (!TCPclient.available())
    yield();
  ch = TCPclient.read();
  Serial.print(ch);
  return (ch);
}

void doesc(char ec) {
  int cx = gfx->getCursorX();
  int cy = lnum, bgn, cnt, ch;
  int cr[2],*crp;

  switch (ec) {
    case 'A':
      if (cy > 1)
        cy--;
      break;
    case 'B':
      cy++;
      break;
    case 'C':
      cx += 10;
      break;
    case 'D':
      cx -= 10;
      break;
    case 'H':
      cx = 0;
      cy = 1;
      break;
    case '=':
    case 'I':
      break;
    case 'J':
      if (cx > 0)
        cx -= 10;
      gfx->fillRect(cx, lnum * 20, 800 - cx, -20, BLACK);  // Clear to end of line
      gfx->fillRect(0, lnum * 20, 800, (24 - lnum) * 20, BLACK);
      break;
    case 'K':
      gfx->fillRect(cx, lnum * 20, 800 - cx, -20, BLACK);
      break;
    case 'L':
      scrollDown((cy - 1) * 40 * 800);  //Insert a line and scroll down
      break;
    case 'M':
      scrollUp((cy - 1) * 40 * 800);  //Delete a line and scroll up
      break;
    case 'Y':
      cy = 1 + recv_char() - 32;  //set cursor position
      cx = (recv_char() - 32) * 10;
      break;
    case 'Z':
      escz++;
      break;
    case '(':
      ch=recv_char();
      if (ch != '0')
      break;
        ch=recv_char();
        gfx->setTextColor(WHITE, WHITE);
        gfx->print('$');
        gfx->setTextColor(DARKGREEN);
      break;
    case '[':
      memset(cr,0,sizeof(cr));
      crp=cr;
      while (1) {
        ch=recv_char();
        if (ch == 'K') {
          gfx->fillRect(cx, lnum * 20, 800 - cx, -20, BLACK);  // Clear to end of line
          break;          
        }
        if (ch == 'J') {
          gfx->fillRect(cx, lnum * 20, 800 - cx, -20, BLACK);  // Clear to end of line
          gfx->fillRect(0, lnum * 20, 800, (24 - lnum) * 20, BLACK);
          break;
        }
        if (ch == 'm')
          break;
        if (ch == 'H') {
          cy = cr[0];
          cx = cr[1]*10-10;
          break;
        }
        if (ch == ';') {
          crp++;
          continue;
        }
        if (ch < '0' || ch > '9')
          break;
        *crp=*crp*10+(ch-'0');
      }
      break;
  }
  lnum = cy;
  gfx->setCursor(cx, (lnum)*20);
}

void SetCursor()
{
    if (tflg && cenb) {
      int px = gfx->getCursorX() - 10;
      tflg = 0;
      if (cflg) {
        gfx->setTextColor(DARKGREEN);
        gfx->print('_');
        cflg = 0;
      } else {
        cflg++;
        gfx->setTextColor(WHITE);
        gfx->print('_');
      }
      gfx->setCursor(px + 10, (lnum)*20);
      gfx->fillRect(790, 0, 10, 10, cplock==3?NAVY:BLACK);
    }
}

void Display_String(char *str){
  while (*str)
    Display_Char(*str++);
}


void Display_Char(char ch) {
  int cx;

  cx = gfx->getCursorX();
  cenb=0;
  gfx->setTextColor(BLACK);
  gfx->print('_');
  gfx->setCursor(cx, (lnum)*20);
  gfx->fillRect(cx, lnum * 20, 10, -4, BLACK);
  gfx->setTextColor(DARKGREEN);

  switch (ch) {
    case (27):
      ch = recv_char();
      doesc(ch);
      break;
    case ('\n'):
      if (++lnum == 25) {
        lnum--;
        scrollUp(0);
      }
      gfx->setCursor(cx, (lnum)*20);
      break;
    case ('\r'):
      gfx->setCursor(0, (lnum)*20);
      break;
    case (7):  // ^g (BEL)
      break;
    case ('\b'):
      cx = gfx->getCursorX();
      if (cx >= 0) {
        gfx->setCursor(cx - 10, (lnum)*20);
      }
      break;
    case ('\t'):
      cx = (gfx->getCursorX() + 80) / 80;
      gfx->setCursor(cx * 80, (lnum)*20);
      break;
    default:
      cx = gfx->getCursorX();
      if (cx >= 800) {
        gfx->setCursor(cx = 0, (lnum)*20);
        Display_Char('\n');
      }
      if (ch) {
        if (ch != 0x0d)
          gfx->fillRect(cx, lnum * 20, 10, -20, BLACK);
        if (ch != ' ')
          gfx->print(ch);
        lstc = ch;
        gfx->setCursor(cx + 10, (lnum)*20);
      }
      break;
  }
  cenb=1;
}

void SetChar(char *kchar){
  inchar=kchar;
}

void disp_loop() {

  char ch;
  while (online) {
    if (!TCPclient.connected()) {
      Serial.println("Connection is disconnected");
      TCPclient.stop();

      // reconnect to TCP server (Arduino #2)
      if (TCPclient.connect(nhost, serverPort)) {
        Serial.println("Reconnected to TCP server");
        Serial.println("\e[?2lMode changed to VT52");
      } else {
        Serial.println("Failed to reconnect to TCP server");
      }
    }

    ParseTelnet(TCPclient);

    if (Serial.available()) {
      ch = Serial.read();
      TCPclient.write(ch);
    }
    if (*inchar) {
      TCPclient.print(inchar);
      TCPclient.flush();
      inchar=&cbuf;
    }
    SetCursor();
    yield();
  }
}
