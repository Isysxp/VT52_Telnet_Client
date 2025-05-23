This app provides a VT52 compatible Telnet client using a BLE keyboard and a Waveshare 4.3" LCD display: https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-4.3

The application is built using the Arduino IDE and requires the following board and libraries:

Esp32 by Espressif V 3.1.1

Arduino GFX librarty V 1.2

NimBLE-Arduino V 2.3.0

U8g2 V 2.35.30

You may get some errors that indicate other missing libraries.

Arduino configuration:<br>
Device: Waveshare-ESP32-S3-Touch-LCD-4.3<br>
Config: Upload mode UART0 / Hardware CDC + USB mode Hardware CDC and JTAG + PSRAM enabled.

Change line 43 in VT_Telnet_Client.ino to match the name of your BLE keyboard.<br>
This is not an ideal way to connect to BLE devices as they commonly do not have a name.<br>
To check the system out I suggest you connect a terminal app to the USB port on the Waveshare board.<br>
You should see a list of local advertising devices from which you can find the name of your keyboard if you are not too sure.<br>
Also, you will need to set the SSID and password for you local router in Wave_Display.ino.<br>
With a bit of luck, the app will find your keyboard, attach to your router and connect to your host.<br>
At this point, the login prompt will appear of the display.<br>
CapsLock is an issue with my keyboard as I cannot control the LED. You will not that a blue square appears in the display to indicate this state.<br>
For this build, the cursor keys are setup as for a VT52 keyboard but the keypad is fixed in numeric mode.<br>
To change this, look at src/key_codes.h and alter the text for keys on the last 3 lines of this file.<br>
This app was originally built to connect to a UNIX and use vi. So, these keys are not required.<br>
These are required for TECO/KED etc. but not for EMACS and (I think) EDT.

Best of luck as this is a slightly involved app using BLE/WiFi/DMA all at once!!

Ian Schofield March 20205

