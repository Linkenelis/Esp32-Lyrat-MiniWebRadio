/***********************************************************************************************************************
    MiniWebRadio -- Webradio receiver for ESP32

    first release on 03/2017
    Version 2.e, Feb 26/2022

    2.8" color display (320x240px) with controller ILI9341 or HX8347D (SPI) or
    3.5" color display (480x320px) wihr controller ILI9486 (SPI)

    HW decoder VS1053 or
    SW decoder with external DAC over I2S

    SD is mandatory
    IR remote is optional

***********************************************************************************************************************/

// THE SOFTWARE IS PROVIDED "AS IS" FOR PRIVATE USE ONLY, IT IS NOT FOR COMMERCIAL USE IN WHOLE OR PART OR CONCEPT.
// FOR PERSONAL USE IT IS SUPPLIED WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
// WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHOR
// OR COPYRIGHT HOLDER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE

#include "common.h"


//global variables

const uint8_t  _max_volume     = 21;
const uint16_t _max_stations   = 1000;
uint8_t        _alarmdays      = 0;
uint16_t       _cur_station    = 0;      // current station(nr), will be set later
uint16_t       _sleeptime      = 0;      // time in min until MiniWebRadio goes to sleep
uint16_t       _sum_stations   = 0;
uint8_t        _cur_volume     = 0;      // will be set from stored preferences
uint8_t        _state          = 0;      // statemaschine
uint8_t        _touchCnt       = TouchCnt;
uint8_t        _commercial_dur = 0;      // duration of advertising
uint16_t       _alarmtime      = 0;      // in minutes (23:59 = 23 *60 + 59)
int8_t         _releaseNr      = -1;
uint32_t       _resumeFilePos  = 0;
uint16_t x, y;                  // touch points
uint8_t         rtc_tries=0;
uint8_t        input=1;        //1=radio; 2=Player_SD; 4= Player_network?; 8=BT_in
uint8_t        output=1;       //1=speaker(I2S); 2=BT-out; 4=? 
char           _chbuf[512];
char           _myIP[25];
char           _afn[256];                // audioFileName
char           _path[128];
char           _prefix[5]      = "/s";
char*          _lastconnectedfile = nullptr;
char*          _lastconnectedhost = nullptr;
char*          _stationURL = nullptr;
const char*    _pressBtn[7];
const char*    _releaseBtn[7];
const uint8_t* _flashRelBtn[7];
const uint8_t* _flashPressBtn[7];
boolean        _f_rtc  = false;             // true if time from ntp is received
boolean        _f_1sec = false;
boolean        _f_1min = false;
boolean        _f_mute = false;
boolean        _f_sleeping = false;
boolean        _f_isWebConnected = false;
boolean        _f_isFSConnected = false;
boolean        _f_eof = false;
boolean        _f_eof_alarm = false;
boolean        _f_semaphore = false;
boolean        _f_alarm = false;
boolean        _f_irNumberSeen = false;
boolean        _f_newIcyDescription = false;
boolean        _f_loop=true;
boolean        _BT_In=false;
boolean         audioTask_runs=false;
boolean         BTTask_runs=   false;

String         _station = "";
String         _stationName_nvs = "";
String         _stationName_air = "";
String         _homepage = "";
String         _streamTitle = "";
String         _filename = "";
String         _icydescription = "";
boolean         psRAMavail=false;
String          _mp3Name[1000];
char timc[20]; //for digital time
int clocktype=1;
int previous_sec=100;
int previous_day=100;

uint8_t nbroftracks=0;
int previousMillis=0;
bool mp3playall = true;        //play next mp3, after end of file when true
bool shuffle=false;
String _audiotrack="";             //track from SD/mp3files/
String artsong="";
String connectto="";
String Title="";

char _hl_item[11][25]{                          // Title in headline
                "Internet Radio ",         // "* интернет-радио *"  "ραδιόφωνο Internet"
                "Internet Radio ",
                "Internet Radio ",
                "Clock ",                    // Clock "** часы́ **"  "** ρολόι **"
                "Clock ",
                "Brightness ",             // Brightness яркость λάμψη
                "Audioplayer ",            // "** цифрово́й плеер **"
                "Audioplayer ",
                "" ,                            // Alarm should be empty
                "Sleeptimer ",       // "Sleeptimer" "Χρονομετρητής" "Таймер сна"
                "Settings",
};

enum status{RADIO = 0, RADIOico = 1, RADIOmenue = 2,
            CLOCK = 3, CLOCKico = 4, BRIGHTNESS = 5,
            PLAYER= 6, PLAYERico= 7,
            ALARM = 8, SLEEP    = 9, SETTINGS = 10};

/** variables for time, set in platformio.ini*/
char NTP_pool_name[] = NTP_pool;
const char* ntpServer = NTP_pool_name;


WebSrv webSrv;
Preferences pref;
Preferences stations;
RTIME rtc;
struct tm timeinfo;
time_t now;
hw_timer_t * timer = NULL;
Ticker ticker;
//IR ir(IR_PIN);                  // do not change the objectname, it must be "ir"
//TP tp(TP_CS, TP_IRQ);
//WiFiMulti wifiMulti;
File audioFile;
FtpServer ftpSrv;
/** Task handle of the taskhandler */
TaskHandle_t audioTaskHandler;
TaskHandle_t BTTaskHandler;



SemaphoreHandle_t  mutex_rtc;
SemaphoreHandle_t  mutex_display;

portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
volatile bool screen_touched = false;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

/** hardware timer to count secs */
volatile int interruptCounter;  //int(max) = 2.147.483.647; a day = 86400 secs, so over 248.547 days or 680 years
volatile int totalcounter;	//multisecs (long eg. hours)
volatile int mediumcounter;	//multisecs (medium long eg. minutes)
void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&timerMux);    //portenter and exit are needed as to block it for other processes
  interruptCounter++;
  portEXIT_CRITICAL_ISR(&timerMux); 
}
void IRAM_ATTR touched() {
    portENTER_CRITICAL(&mux);
    screen_touched=true;
    portEXIT_CRITICAL(&mux);
}

#if TFT_CONTROLLER == 0 || TFT_CONTROLLER == 1
    //
    //  Display 320x240
    //  +-------------------------------------------+ _yHeader=0
    //  | Header                                    |       _hHeader=20px
    //  +-------------------------------------------+ _yName=20
    //  |                                           |
    //  | Logo                   StationName        |       _hName=100px
    //  |                                           |
    //  +-------------------------------------------+ _yTitle=120
    //  |                                           |
    //  |              StreamTitle                  |       _hTitle=100px
    //  |                                           |
    //  +-------------------------------------------+ _yFooter=220
    //  | Footer                                    |       _hFooter=20px
    //  +-------------------------------------------+ 240
    //                                             320
    const unsigned short* _fonts[6] = {
        Times_New_Roman15x14,
        Times_New_Roman21x17,
        Times_New_Roman27x21,
        Times_New_Roman34x27,
        Times_New_Roman38x31,
        Times_New_Roman43x35,
    };

    struct w_h {uint16_t x = 0;   uint16_t y = 0;   uint16_t w = 320; uint16_t h = 20; } const _winHeader;
    struct w_l {uint16_t x = 0;   uint16_t y = 20;  uint16_t w = 100; uint16_t h = 100;} const _winLogo;
    struct w_n {uint16_t x = 100; uint16_t y = 20;  uint16_t w = 220; uint16_t h = 100;} const _winName;
    struct w_e {uint16_t x = 0;   uint16_t y = 20;  uint16_t w = 320; uint16_t h = 100;} const _winFName;
    struct w_t {uint16_t x = 0;   uint16_t y = 120; uint16_t w = 320; uint16_t h = 100;} const _winTitle;
    struct w_f {uint16_t x = 0;   uint16_t y = 220; uint16_t w = 320; uint16_t h = 20; } const _winFooter;
    struct w_i {uint16_t x = 0;   uint16_t y = 0;   uint16_t w = 180; uint16_t h = 20; } const _winItem;
    struct w_v {uint16_t x = 180; uint16_t y = 0;   uint16_t w =  50; uint16_t h = 20; } const _winVolume;
    struct w_m {uint16_t x = 260; uint16_t y = 0;   uint16_t w =  60; uint16_t h = 20; } const _winTime;
    struct w_s {uint16_t x = 0;   uint16_t y = 220; uint16_t w =  60; uint16_t h = 20; } const _winStaNr;
    struct w_p {uint16_t x = 60;  uint16_t y = 220; uint16_t w = 120; uint16_t h = 20; } const _winSleep;
    struct w_a {uint16_t x = 180; uint16_t y = 220; uint16_t w = 160; uint16_t h = 20; } const _winIPaddr;
    struct w_b {uint16_t x = 0;   uint16_t y = 120; uint16_t w = 320; uint16_t h = 14; } const _winVolBar;
    struct w_o {uint16_t x = 0;   uint16_t y = 154; uint16_t w =  64; uint16_t h = 64; } const _winButton;
    uint16_t _alarmdaysXPos[7] = {3, 48, 93, 138, 183, 228, 273};
    uint16_t _alarmtimeXPos[5] = {2, 75, 173, 246, 148}; // last is colon
    uint16_t _sleeptimeXPos[5] = {5, 77, 129, 57}; // last is colon
    uint8_t  _alarmdays_w = 44 + 4;
    uint8_t  _alarmdays_h = 40;
    uint16_t _dispHeight  = 240;
    uint8_t  _tftSize     = 0;
    //
    TFT tft(TFT_CONTROLLER);
    //
#endif //TFT_CONTROLLER == 0 || TFT_CONTROLLER == 1


#if TFT_CONTROLLER == 2 || TFT_CONTROLLER == 3
    //
    //  Display 480x320
    //  +-------------------------------------------+ _yHeader=0
    //  | Header                                    |       _winHeader=30px
    //  +-------------------------------------------+ _yName=30
    //  |                                           |
    //  | Logo                   StationName        |       _winFName=130px
    //  |                                           |
    //  +-------------------------------------------+ _yVolumebar=160
    //  +-------------------------------------------+ _yTitle=165
    //  |                                           |
    //  |              StreamTitle                  |       _winTitle=120px
    //  |                                           |
    //  +-------------------------------------------+ _yFooter=290
    //  | Footer                                    |       _winFooter=30px
    //  +-------------------------------------------+ 320
    //                                             480
/*
    const unsigned short* _fonts[6] = {
        Times_New_Roman27x21,
        Times_New_Roman34x27,
        Times_New_Roman38x31,
        Times_New_Roman43x35,
        Times_New_Roman56x46,
        Times_New_Roman66x53,
    };
*/
// Window sizes
    struct w_h {uint16_t x = 0;   uint16_t y = 0;   uint16_t w = 480; uint16_t h = 30; } const _winHeader;
    struct w_l {uint16_t x = 0;   uint16_t y = 30;  uint16_t w = 130; uint16_t h = 130;} const _winLogo;
    struct w_n {uint16_t x = 135; uint16_t y = 30;  uint16_t w = 345; uint16_t h = 130;} const _winName;
    struct w_e {uint16_t x = 0;   uint16_t y = 30;  uint16_t w = 480; uint16_t h = 130;} const _winFName;
    struct w_b {uint16_t x = 0;   uint16_t y = 160; uint16_t w = 480; uint16_t h = 5;} const _winVolBar;
    struct w_t {uint16_t x = 0;   uint16_t y = 165; uint16_t w = 480; uint16_t h = 120;} const _winTitle;
    struct w_f {uint16_t x = 0;   uint16_t y = 290; uint16_t w = 480; uint16_t h = 30; } const _winFooter;
    struct w_m {uint16_t x = 390; uint16_t y = 0;   uint16_t w =  90; uint16_t h = 30; } const _winTime;
    struct w_i {uint16_t x = 0;   uint16_t y = 0;   uint16_t w = 280; uint16_t h = 30; } const _winItem;
    struct w_v {uint16_t x = 210; uint16_t y = 0;   uint16_t w = 90; uint16_t h = 30; } const _winVolume;
    struct w_a {uint16_t x = 210; uint16_t y = 290; uint16_t w = 270; uint16_t h = 30; } const _winIPaddr;
    struct w_s {uint16_t x = 0;   uint16_t y = 290; uint16_t w = 100; uint16_t h = 30; } const _winStaNr;
    struct w_p {uint16_t x = 100; uint16_t y = 290; uint16_t w = 80; uint16_t h = 30; } const _winSleep;
    //struct w_b {uint16_t x = 0;   uint16_t y = 160; uint16_t w = 480; uint16_t h = 30; } const _winVolBar;
    struct w_o {uint16_t x = 0;   uint16_t y = 210; uint16_t w =  67; uint16_t h = 96; } const _winButton;  //w was 96, y was 190 but I use small buttons to use up to 7 buttons
    uint16_t _alarmdaysXPos[7] = {2, 70, 138, 206, 274, 342, 410};
    uint16_t _alarmtimeXPos[5] = {12, 118, 266, 372, 224}; // last is colon
    uint16_t _sleeptimeXPos[5] = {5, 107, 175, 73 };
    uint8_t  _alarmdays_w = 64 + 4;
    uint8_t  _alarmdays_h = 56;
    uint16_t _dispHeight  = 320;
    uint8_t  _tftSize     = 1;
    //
    //TFT tft(TFT_CONTROLLER);
    //
#endif  // #if TFT_CONTROLLER == 2 || TFT_CONTROLLER == 3


//TIME  *********************************************************************************************************
void get_time(void){
    if(!getLocalTime(&timeinfo)){
        Serial.println("Failed to obtain time");
        rtc_tries++;
        if(rtc_tries>24){_f_rtc=false;}  //if _f_rtc=false every minute if _f_rtc=true every hour; max 24 hours without sync
        return;
    }

    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    if (timeinfo.tm_year+1900 > 2020) 
    {
        _f_rtc=true; 
        rtc_tries=0;
    }else{
        _f_rtc=false;
    }
    Serial.print("_f_rtc= "); Serial.println(_f_rtc);
}
const char* gettime_s(){  // hh:mm:ss
	time(&now);
	localtime_r(&now, &timeinfo);
	sprintf(timc,"%02d:%02d:%02d ",  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
	return timc;
}
const char* gettime_xs(){  // hh:mm
    time(&now);
    localtime_r(&now, &timeinfo);
    sprintf(timc,"%02d:%02d",  timeinfo.tm_hour, timeinfo.tm_min);
    return timc;
}
uint8_t getweekday(){ //So=0, Mo=1 ... Sa=6
    time(&now);
    localtime_r(&now, &timeinfo);
    return timeinfo.tm_wday;
}
uint16_t getMinuteOfTheDay(){ // counts at 00:00, from 0...23*60+59
    time(&now);
    localtime_r(&now, &timeinfo);
    return timeinfo.tm_hour * 60 + timeinfo.tm_min;
}
//WiFi and Reset ************************************************************************************************
#ifndef ip_fixed    //This happens when ip_fixed is commented out (or not present) in platformio.ini ->DHCP
    IPAddress local_IP;    //unit ip, gateway and subnet are set in platformio.ini
    IPAddress gateway;
    IPAddress subnet;
#endif
#ifdef ip_fixed     //This happens when ip_fixed has a value in platformio.ini ->Fixed ip
    IPAddress local_IP(unit_ip);    //unit ip, gateway and subnet are set in platformio.ini
    IPAddress gateway(unit_gateway);    //editor might complain, as it is not found in the code
    IPAddress subnet(unit_subnet);
#endif
const char* ssid = WIFI_SSID;
const char* password =  WIFI_PASS;

void BTInit() {
    xTaskCreatePinnedToCore(
        BTTask,              // Function to implement the task 
        "BT-In",            // Name of the task 
        5000,                   // Stack size in words 
        NULL,                   // Task input parameter 
        2 | portPRIVILEGE_BIT,  // Priority of the task 
        &BTTaskHandler,                   // Task handle. 
        0                       // Core where the task should run 
    );
}
//BT-In
void avrc_metadata_callback(uint8_t id, const uint8_t *text) {
  Serial.printf("==> AVRC metadata rsp: attribute id 0x%x, %s\n", id, text);
}

void wifi_conn(void)  // Connect the WiFi
{
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid,password);
	if (ip_fixed)   //If local_IP has a value, then set WiFi config
	{
	WiFi.config(local_IP, gateway, subnet);
	}

	while (WiFi.status() != WL_CONNECTED)
	{
		delay(500);
		Serial.print(".");
	}

	// Print information how to connect
	IPAddress ip = WiFi.localIP();
	Serial.print("\nWiFi connected with IP ");
	Serial.println(ip);
    sprintf(_myIP, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

/** Handle resetDevice */
void resetDevice(void)
{
	delay(100);
	WiFi.disconnect();
	esp_restart();
}

/***********************************************************************************************************************
*                                        D E F A U L T S E T T I N G S                                                 *
***********************************************************************************************************************/
boolean defaultsettings(){
    if(pref.getUInt("default", 0) != 1000){
        log_i("first init, set defaults");
		if(!saveStationsToNVS()) return false;
        pref.clear();
        //
        pref.putUShort("alarm_weekday",0); // for alarmclock
        pref.putUInt("alarm_time", 0);
        pref.putUShort("ringvolume",21);
        //
        pref.putUShort("volume",12); // 0...21
        pref.putUShort("mute",   0); // no mute

        pref.putUShort("brightness", 100); // 0...100
        pref.putUInt("sleeptime", 0);

        pref.putUShort("toneha", 0); // BassFreq 0...15        VS1053
        pref.putUShort("tonehf", 0); // TrebleGain 0...14      VS1053
        pref.putUShort("tonela", 0); // BassGain 0...15        VS1053
        pref.putUShort("tonelf", 0); // BassFreq 0...13        VS1053

        pref.putShort("toneLP", 0); // -40 ... +6 (dB)     I2S DAC
        pref.putShort("toneBP", 0); // -40 ... +6 (dB)     I2S DAC
        pref.putShort("toneHP", 0); // -40 ... +6 (dB)     I2S DAC
        //
        pref.putUInt("station", 1);
        //
        pref.putUInt("default", 1000);
    }
	return true;
}

boolean saveStationsToNVS(){
    String X="", Cy="", StationName="", StreamURL="", currentLine="", tmp="";
    uint16_t cnt = 0;
    // StationList
	if(!SD.exists("/stations.csv")){
		log_e("SD/stations.csv not found");
		return false;
	}

    File file = SD.open("/stations.csv");
    if(file){  // try to read from SD
        stations.clear();
        currentLine = file.readStringUntil('\n');             // read the headline
        while(file.available()){
            currentLine = file.readStringUntil('\n');         // read the line
            uint p = 0, q = 0;
            X=""; Cy=""; StationName=""; StreamURL="";
            for(int i = 0; i < currentLine.length() + 1; i++){
                if(currentLine[i] == '\t' || i == currentLine.length()){
                    if(p == 0) X            = currentLine.substring(q, i);
                    if(p == 1) Cy           = currentLine.substring(q, i);
                    if(p == 2) StationName  = currentLine.substring(q, i);
                    if(p == 3) StreamURL    = currentLine.substring(q, i);
                    p++;
                    i++;
                    q = i;
                }
            }
            if(X == "*") continue;
            if(StationName == "") continue; // is empty
            if(StreamURL   == "") continue; // is empty
            //log_i("Cy=%s, StationName=%s, StreamURL=%s",Cy.c_str(), StationName.c_str(), StreamURL.c_str());
            cnt++;
            if(cnt ==_max_stations){
                SerialPrintfln("No more than %d entries in stationlist allowed!", _max_stations);
                cnt--; // maxstations 999
                break;
            }
            tmp = StationName + "#" + StreamURL;
            sprintf(_chbuf, "station_%03d", cnt);
            stations.putString(_chbuf, tmp);
        }
        _sum_stations = cnt;
        stations.putLong("stations.size", file.size());
        file.close();
        stations.putUInt("sumstations", cnt);
        SerialPrintfln("stationlist internally loaded");
        SerialPrintfln("number of stations: %i", cnt);
        return true;
    }
    else return false;
}

/***********************************************************************************************************************
*                                                     A S C I I                                                        *
***********************************************************************************************************************/
const char* UTF8toASCII(const char* str){
    uint16_t i=0, j=0;
    char tab[96]={
          96,173,155,156, 32,157, 32, 32, 32, 32,166,174,170, 32, 32, 32,248,241,253, 32,
          32,230, 32,250, 32, 32,167,175,172,171, 32,168, 32, 32, 32, 32,142,143,146,128,
          32,144, 32, 32, 32, 32, 32, 32, 32,165, 32, 32, 32, 32,153, 32, 32, 32, 32, 32,
         154, 32, 32,225,133,160,131, 32,132,134,145,135,138,130,136,137,141,161,140,139,
          32,164,149,162,147, 32,148,246, 32,151,163,150,129, 32, 32,152
     };
    while((str[i]!=0)&&(j<1020)){
        _chbuf[j]=str[i];
        if(str[i]==0xC2){ // compute unicode from utf8
            i++;
            if((str[i]>159)&&(str[i]<192)) _chbuf[j]=tab[str[i]-160];
            else _chbuf[j]=32;
        }
        else if(str[i]==0xC3){
            i++;
            if((str[i]>127)&&(str[i]<192)) _chbuf[j]=tab[str[i]-96];
            else _chbuf[j]=32;
        }
        i++; j++;
    }
    _chbuf[j]=0;
    return (_chbuf);
}
const char* ASCIItoUTF8(const char* str){
    uint16_t i=0, j=0, uni=0;
    uint16_t tab[128]={
         199, 252, 233, 226, 228, 224, 229, 231, 234, 235, 232, 239, 238, 236, 196, 197,
         201, 230, 198, 244, 246, 242, 251, 249, 255, 214, 220, 162, 163, 165,8359, 402,
         225, 237, 243, 250, 241, 209, 170, 186, 191,8976, 172, 189, 188, 161, 171, 187,
        9617,9618,9619,9474,9508,9569,9570,9558,9557,9571,9553,9559,9565,9564,9563,9488,
        9492,9524,9516,9500,9472,9532,9566,9567,9562,9556,9577,9574,9568,9552,9580,9575,
        9576,9572,9573,9561,9560,9554,9555,9579,9578,9496,9484,9608,9604,9612,9616,9600,
         945, 223, 915, 960, 931, 963, 181, 964, 934, 920, 937, 948,8734, 966, 949,8745,
        8801, 177,8805,8804,8992,8993, 247,8776, 176,8729, 183,8730,8319, 178,9632, 160
    };
    while((str[i]!=0)&&(j<1020)){
        uni=str[i];
        if(uni>=128){uni-=128; uni=tab[uni];}
//            uni=UTF8fromASCII(str[i]);
            switch(uni){
                case   0 ... 127:{_chbuf[j]=str[i]; i++; j++; break;}
                case 160 ... 191:{_chbuf[j]=0xC2; _chbuf[j+1]=uni; j+=2; i++; break;}
                case 192 ... 255:{_chbuf[j]=0xC3; _chbuf[j+1]=uni-64; j+=2; i++; break;}
                default:{_chbuf[j]=' '; i++; j++; break;} // ignore all other
            }
    }
    _chbuf[j]=0;
    return _chbuf;
}
/***********************************************************************************************************************
*                                                     T I M E R                                                        *
***********************************************************************************************************************/
void timer1sec() {
    static volatile uint8_t sec=0;
    _f_1sec = true;
    sec++;
    //log_i("sec=%i", sec);
    if(sec==60){sec=0; _f_1min = true;}
}
/***********************************************************************************************************************
*                                                   D I S P L A Y                                                      *
***********************************************************************************************************************/

/***********************************************************************************************************************
*                                         L I S T A U D I O F I L E                                                    *
***********************************************************************************************************************/
bool setAudioFolder(const char* audioDir){
    Serial.println("setaudiofolder");
    if(audioFile) audioFile.close();  // same as rewind()
    if(!SD.exists(audioDir)){log_e("%s not exist", audioDir); return false;}
    audioFile = SD.open(audioDir);
    if(!audioFile.isDirectory()){log_e("%s is not a directory", audioDir); return false;}
    return true;
}
const char* listAudioFile(){
    Serial.println("listAudioFile");
    File file = audioFile.openNextFile();
    if(!file) {
        Serial.println("no more files found");
        audioFile.close();
        return NULL;
    }
    while(file){
        const char* name = file.name();
        if(endsWith(name, ".mp3") || endsWith(name, ".aac") ||
           endsWith(name, ".m4a") || endsWith(name, ".wav") ||
           endsWith(name, ".flac")  ) Serial.println(name); return name;
    }
    return NULL;
}

bool sendAudioList2Web(const char* audioDir){       //cannot do 1000 tracks
Serial.println("sendAudioList2Web");
    if(!setAudioFolder(audioDir)) return false;
    const char* FileName = NULL;
    String str = "AudioFileList=";
    uint8_t i = 0;
    while(true){
        FileName = listAudioFile();
        if(FileName){
            if(i) str += ",";
            str += (String)FileName;
            i++;    //max25 for now
            if(i==25) break;
        }
        else break;
    }
     log_e("%s", str.c_str());
    webSrv.send((const char*)str.c_str());
    return true;
}
/***********************************************************************************************************************
*                                         C O N N E C T   TO   W I F I                                                 *
***********************************************************************************************************************/
/*
bool connectToWiFi(){
    String s_ssid = "", s_password = "", s_info = "";
    wifiMulti.addAP(_SSID, _PW);                // SSID and PW in code
    WiFi.setHostname("MiniWebRadio");
    File file = SD.open("/networks.csv"); // try credentials given in "/networks.txt"
    if(file){                                         // try to read from SD
        String str = "";
        while(file.available()){
            str = file.readStringUntil('\n');         // read the line
            if(str[0] == '*' ) continue;              // ignore this, goto next line
            if(str[0] == '\n') continue;              // empty line
            if(str[0] == ' ')  continue;              // space as first char
            if(str.indexOf('\t') < 0) continue;       // no tab
            str += "\t";
            uint p = 0, q = 0;
            s_ssid = "", s_password = "", s_info = "";
            for(int i = 0; i < str.length(); i++){
                if(str[i] == '\t'){
                    if(p == 0) s_ssid     = str.substring(q, i);
                    if(p == 1) s_password = str.substring(q, i);
                    if(p == 2) s_info     = str.substring(q, i);
                    p++;
                    i++;
                    q = i;
                }
            }
            //log_i("s_ssid=%s  s_password=%s  s_info=%s", s_ssid.c_str(), s_password.c_str(), s_info.c_str());
            if(s_ssid == "") continue;
            if(s_password == "") continue;
            wifiMulti.addAP(s_ssid.c_str(), s_password.c_str());
        }
        file.close();
    }
    Serial.println("WiFI_info  : Connecting WiFi...");
    if(wifiMulti.run() == WL_CONNECTED){
        WiFi.setSleep(false);
        return true;
    }else{
        Serial.printf("WiFi credentials are not correct\n");
        return false;  // can't connect to any network
    }
}
*/
/***********************************************************************************************************************
*                                                    A U D I O                                                        *
***********************************************************************************************************************/
void connecttohost(const char* host){
    _f_isWebConnected = audioConnecttohost(host);
    _f_isFSConnected = false;
}
void connecttoFS(const char* filename, uint32_t resumeFilePos){
    Serial.print("connecttoFS...");Serial.println(filename);
    Serial.print("_f_isFSConnected1...");Serial.println(_f_isFSConnected);
    _f_isFSConnected = audioConnecttoFS(filename, resumeFilePos);
    Serial.print("_f_isFSConnected2...");Serial.println(_f_isFSConnected);
    _f_isWebConnected = false;
    _audiotrack=filename;
    Serial.print("connecttoFS...");Serial.println(_audiotrack);
}
void stopSong(){
    audioStopSong();
    _f_isFSConnected = false;
    _f_isWebConnected = false;
}

/***********************************************************************************************************************
*                                                    S E T U P                                                         *
***********************************************************************************************************************/
void setup(){
    mutex_rtc     = xSemaphoreCreateMutex();
    mutex_display = xSemaphoreCreateMutex();

    Serial.begin(115200);

  /*i2s_pin_config_t my_pin_config = {
        .bck_io_num = 27,
        .ws_io_num = 26,
        .data_out_num = 25,
        .data_in_num = I2S_PIN_NO_CHANGE
  };
  a2dp_sink.set_pin_config(my_pin_config);
  a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
  a2dp_sink.start("BTESP32MusicPlayer");*/
    if(TFT_CONTROLLER < 2)  strcpy(_prefix, "/s");
    else                    strcpy(_prefix, "/m");
    pinMode(SD_CS, OUTPUT);      digitalWrite(SD_CS, HIGH);
    //pinMode(VS1053_CS, OUTPUT);  digitalWrite(VS1053_CS, HIGH);
    //pinMode(TFT_CS, OUTPUT);  digitalWrite(TFT_CS, HIGH);
    //pinMode(TP_CS, OUTPUT);  digitalWrite(TP_CS, HIGH);
    pref.begin("MiniWebRadio", false);  // instance of preferences for defaults (tone, volume ...)
    stations.begin("Stations", false);  // instance of preferences for stations (name, url ...)

    SerialPrintfln("setup: Init SD card");
    SPI.begin(VS1053_SCK, VS1053_MISO, VS1053_MOSI); //SPI forVS1053 and SD
    SD.end();       // to recognize SD after reset correctly
    Serial.println("setup      : Init SD card");
    SD.begin(SD_CS);
    vTaskDelay(100); // wait while SD is ready
    SD.begin(SD_CS, SPI, 160000000);  // fast SDcard set 80000000 (try 160000000), must have short SPI-wires
 
    SerialPrintfln("setup: SD card found");




    defaultsettings();  // first init

    /*if (!SPIFFS.begin()) {
    Serial.println("SPIFFS initialisation failed!"); //tft2.println("SPIFFS initialisation failed!");
    } else{
    Serial.println("SPIFFS available!");
    }*/

    if(TFT_CONTROLLER > 3) log_e("The value in TFT_CONTROLLER is invalid");

    SerialPrintfln("setup: seek for stations.csv");
    File file=SD.open("/stations.csv");
    if(!file){

        log_e("stations.csv not found");
        while(1){};  // endless loop, MiniWebRadio does not work without stations.csv
    }
    file.close();
    SerialPrintfln("setup: stations.csv found");
    wifi_conn();
    ftpSrv.begin(SD, FTP_USERNAME, FTP_PASSWORD); //username, password for ftp.
    configTime(0, 0, ntpServer); //set in platformIO.ini
    setenv("TZ", Timezone, 1);
    vTaskDelay(2000); 
    get_time(); //get local time
     SerialPrintfln("setup: init VS1053");
    //pinMode(VS1053_CS, OUTPUT);  digitalWrite(VS1053_CS, HIGH);
    //a2dp_sink.app_task_shut_down(); //shutdown BT task
    /*    i2s_pin_config_t my_pin_config = {
        .bck_io_num = 27,
        .ws_io_num = 26,
        .data_out_num = 25,
        .data_in_num = I2S_PIN_NO_CHANGE
    };
    a2dp_sink.set_pin_config(my_pin_config);*/
 
    //a2dp_sink.start("BTESP32MusicPlayer");
    audioInit();    //run audio to I2C (startup setting)

    _sum_stations = stations.getUInt("sumstations", 0);
    SerialPrintfln("Number of saved stations: %d", _sum_stations);
    _cur_station =  pref.getUInt("station", 1);
    SerialPrintfln("current station number: %d", _cur_station);
    _cur_volume = getvolume();

    SerialPrintfln("current volume: %d", _cur_volume);
    _f_mute = pref.getUShort("mute", 0);
    if(_f_mute) {
        SerialPrintfln("volume is muted: %d", _cur_volume);
        audioSetVolume(0);
    }
    else {
        audioSetVolume(_cur_volume);
    }
    _alarmdays = pref.getUShort("alarm_weekday");
    _alarmtime = pref.getUInt("alarm_time");
    _state = RADIO;

    if(CLEARLOG){
        File log;
        SD.remove("/log.txt");
        log = SD.open("/log.txt", FILE_WRITE); //create a new file
        log.close();
    }

    //ir.begin();  // Init InfraredDecoder

    webSrv.begin(80, 81); // HTTP port, WebSocket port
    //clearAll(); // Clear screen
    //showHeadlineItem(RADIO);
    //showHeadlineVolume(_cur_volume);
    setStation(_cur_station);
    if(DECODER == 0) setTone();    // HW Decoder
    else             setI2STone(); // SW Decoder
    //showFooter();
    //showVolumeBar();
    ticker.attach(1, timer1sec);
        // some info about the board
    Serial.println("\n\n##################################");
    Serial.printf("Internal Total heap %d, internal Free Heap %d\n", ESP.getHeapSize(), ESP.getFreeHeap());
    Serial.printf("SPIRam Total heap %d, SPIRam Free Heap %d\n", ESP.getPsramSize(), ESP.getFreePsram());
    Serial.printf("ChipRevision %d, Cpu Freq %d, SDK Version %s\n", ESP.getChipRevision(), ESP.getCpuFreqMHz(), ESP.getSdkVersion());
    Serial.printf("Flash Size %d, Flash Speed %d\n", ESP.getFlashChipSize(), ESP.getFlashChipSpeed());
    Serial.println("##################################\n\n");
    if(psramInit()){
      Serial.println("\nPSRAM is correctly initialized");
    }else{
      Serial.println("PSRAM not available");
    }
    //interupts

  
    timer = timerBegin(0, 80, true); // start at 0; divider for 80 MHz = 80 so we have 1 MHz timer; count up = true; timers are 64 bits
    timerAttachInterrupt(timer, &onTimer, false);   //edge doesn't work propperly on esp32, so false here
    timerAlarmWrite(timer, 1000000, true); // 1000000 = writes an alarm, that triggers an interupt, every sec with divider 80
    timerAlarmEnable(timer);
        if(LOG){
            File log;
            log = SD.open("/log.txt", FILE_APPEND);
            String str=gettime_s(); str.concat("\tsetup done\t"); str.concat(_state); str.concat("\t"); str.concat(_audiotrack); str.concat("\t"); str.concat(_stationURL); str.concat("\n");  //_stationURL
            log.print(str);
        }

    
}
/***********************************************************************************************************************
*                                                  C O M M O N                                                         *
***********************************************************************************************************************/
const char* byte_to_binary(int8_t x){
    static char b[9];
    b[0] = '\0';

    int z;
    for (z = 128; z > 0; z >>= 1){
        strcat(b, ((x & z) == z) ? "1" : "0");
    }
    return b;
}
bool startsWith (const char* base, const char* str) {
    char c;
    while ( (c = *str++) != '\0' )
      if (c != *base++) return false;
    return true;
}
bool endsWith (const char* base, const char* str) {
    int slen = strlen(str) - 1;
    const char *p = base + strlen(base) - 1;
    while(p > base && isspace(*p)) p--;  // rtrim
    p -= slen;
    if (p < base) return false;
    return (strncmp(p, str, slen) == 0);
}
int indexOf (const char* base, const char* str, int startIndex) {
    const char *p = base;
    for (; startIndex > 0; startIndex--)
        if (*p++ == '\0') return -1;
    char* pos = strstr(p, str);
    if (pos == nullptr) return -1;
    return pos - base;
}
boolean strCompare(char* str1, char* str2){
    return strCompare((const char*) str1, str2);
}
boolean strCompare(const char* str1, char* str2){ // returns true if str1 == str2
    if(!str1) return false;
    if(!str2) return false;
    if(strlen(str1) != strlen(str2)) return false;
    boolean f = true;
    uint16_t i = strlen(str1);
    while(i){
        i--;
        if(str1[i] != str2[i]){f = false; break;}
    }
    return f;
}
const char* scaleImage(const char* path){
    if((!endsWith(path, "bmp")) && (!endsWith(path, "jpg"))){ // not a image
        return UTF8toASCII(path);
    }
    static char pathBuff[256];
    memset(pathBuff, 0, sizeof(pathBuff));
    char* pch = strstr(path + 1, "/");
    if(pch){
        strncpy(pathBuff, path, (pch - path));
        if(TFT_CONTROLLER < 2) strcat(pathBuff, _prefix); // small pic,  320x240px
        else                   strcat(pathBuff, _prefix); // medium pic, 480x320px
        strcat(pathBuff, pch);
    }
    else{
        strcpy(pathBuff, "/common");
        if(TFT_CONTROLLER < 2) strcat(pathBuff, _prefix); // small pic,  320x240px
        else                   strcat(pathBuff, _prefix); // medium pic, 480x320px
        strcat(pathBuff, path);
    }
    return UTF8toASCII(pathBuff);
}
inline uint8_t getvolume(){
    return pref.getUShort("volume");
}
inline void setVolume(uint8_t vol){
    pref.putUShort("volume", vol);
    _cur_volume = vol;
    if(_f_mute==false) audioSetVolume(vol);
    //showHeadlineVolume(vol);
    if (_state == RADIO || _state == RADIOico || _state == PLAYER){
        //showVolumeBar();
    }   
}
uint8_t downvolume(){
    if(_cur_volume == 0) return _cur_volume;
    _cur_volume --;
    setVolume(_cur_volume);
    return _cur_volume;
}
uint8_t upvolume(){
    if(_cur_volume == _max_volume) return _cur_volume;
    _cur_volume++;
    setVolume(_cur_volume);
    return _cur_volume;
}
inline void mute(){
    if(_f_mute==false){_f_mute=true; audioSetVolume(0);  webSrv.send("mute=1");}
    else {_f_mute=false; audioSetVolume(getvolume());  webSrv.send("mute=0");}
    pref.putUShort("mute", _f_mute);
}

void setStation(uint16_t sta){
    //log_i("sta %d, _cur_station %d", sta, _cur_station );
    int vol=_cur_volume;
    setVolume(0);
    if(sta > _sum_stations) sta = _cur_station;
    sprintf (_chbuf, "station_%03d", sta);
    String content = stations.getString(_chbuf);
    //log_i("content %s", content.c_str());
    _stationName_nvs = content.substring(0, content.indexOf("#")); //get stationname
    content = content.substring(content.indexOf("#") + 1, content.length()); //get URL
    content.trim();
    free(_stationURL);
    _stationURL = strdup(content.c_str());
    _homepage = "";
    _icydescription = "";
    //if(_state != RADIOico) clearTitle();
    _cur_station = sta;
    if(!_f_isWebConnected) _streamTitle = "";
    //showFooterStaNr();
    pref.putUInt("station", sta);
    if(!_f_isWebConnected){
        connecttohost(_stationURL);
    }
    else{
        if(!strCompare(_stationURL, _lastconnectedhost)) connecttohost(_stationURL);
    }
    //showLogoAndStationName();
    StationsItems();
    vTaskDelay(1000);
    setVolume(vol);
}
void nextStation(){
    if(_cur_station >= _sum_stations) return;
    _cur_station++;
    setStation(_cur_station);
}
void prevStation(){
    if(_cur_station <= 1) return;
    _cur_station--;
    setStation(_cur_station);
}

void StationsItems(){
    webSrv.send("stationNr=" + String(pref.getUInt("station")));
    webSrv.send("stationURL=" + String(_stationURL));
    webSrv.send("stationName=" + _stationName_nvs);
}

void savefile(const char* fileName, uint32_t contentLength){ //save the uploadfile on SD
    char fn[256];

    if(endsWith(fileName, "jpg")){
        strcpy(fn, "/logo");
        strcat(fn, _prefix);
        if(!startsWith(fileName, "/")) strcat(fn, "/");
        strcat(fn, fileName);
        if(webSrv.uploadB64image(SD, UTF8toASCII(fn), contentLength)){
            SerialPrintfln("save image %s to SD card was successfully", fn);
            webSrv.reply("OK");
        }
        else webSrv.reply("failure");
    }
    else{
        if(!startsWith(fileName, "/")){
            strcpy(fn, "/");
            strcat(fn, fileName);
        }
        else{
            strcpy(fn, fileName);
        }
        if(webSrv.uploadfile(SD, UTF8toASCII(fn), contentLength)){
            SerialPrintfln("save file %s to SD card was successfully", fn);
            webSrv.reply("OK");
        }
        else webSrv.reply("failure");
        if(strcmp(fn, "/stations.csv") == 0) saveStationsToNVS();
    }
}
String setTone(){ // vs1053
    uint8_t ha =pref.getUShort("toneha");
    uint8_t hf =pref.getUShort("tonehf");
    uint8_t la =pref.getUShort("tonela");
    uint8_t lf =pref.getUShort("tonelf");
    audioSetTone(ha, hf, la, lf);
    sprintf(_chbuf, "toneha=%i\ntonehf=%i\ntonela=%i\ntonelf=%i\n", ha, hf, la, lf);
    String tone = String(_chbuf);
    return tone;
}

String setI2STone(){
    int8_t LP = pref.getShort("toneLP");
    int8_t BP = pref.getShort("toneBP");
    int8_t HP = pref.getShort("toneHP");
    audioSetTone(LP, BP, HP);
    sprintf(_chbuf, "LowPass=%i\nBandPass=%i\nHighPass=%i\n", LP, BP, HP);
    String tone = String(_chbuf);
    return tone;
}

void audiotrack(const char* fileName, uint32_t resumeFilePos){
    char* path = (char*)malloc(strlen(fileName) + 20);
    strcpy(path, "/audiofiles/");
    strcat(path, fileName);
    //clearFName();
    //showVolumeBar();
    //showHeadlineVolume(_cur_volume);
    //showFileName(fileName);
    //changeState(PLAYER);
    connecttoFS((const char*) path, resumeFilePos);
    if(_f_isFSConnected){
        free(_lastconnectedfile);
        _lastconnectedfile = strdup(fileName);
        _resumeFilePos = 0;
    }
    if(path) free(path);
}
void next_track_SD(int tracknbr)
  {
    char ch;
    String track = "";
    String sstr = "";  //Search string
    bool found = false;
    int trcknbr=0;
    File trcklst = SD.open("/tracklist.txt", FILE_READ); //open file for reading
    if (nbroftracks<=1){     //get nbr of tracks (only once)
        while (trcklst.available())
        {
            ch = trcklst.read();
            if (ch == '\n'){ //this is what we want
                nbroftracks++;
            }
        } 
        track = "";
    }
    Serial.print("nbr tracks ");Serial.println(nbroftracks);
    trcklst.close();
    if(tracknbr>0){trcknbr=tracknbr; if(trcknbr>nbroftracks) {trcknbr=1;}}
    else{ trcknbr = random(1, nbroftracks+1);}
    Serial.print("Random or fixed trcknbr = ");Serial.println(trcknbr);  //we have the track number

    //get the number and name of that track
    sstr=String(trcknbr)+"\t:";
    Serial.print("Searching for tracknumber ");Serial.println(trcknbr);  // search for /mp3files/tracknumber<space>
    trcklst = SD.open("/tracklist.txt", FILE_READ); //open file for reading
    while (trcklst.available())
    {
      ch = trcklst.read();
      track += ch;
      if (track == sstr) {found = true; track = "";} //search for the random track, empty track if the random track is found
      while (found)
      {
        ch = trcklst.read();
        if (ch == '\n') //this is what we want
        {
          Serial.print("This is the track ");Serial.println(sstr+track); //do what is usefull here
          _audiotrack=track;
          track = "";     //reset and exit
          found = false;
          trcklst.close();
          return;
        }
        else
        {
          track += ch;    //add chars until new line, while found = true 
        }
      }  
      
      if (ch == '\n') //new line; delete track, while not found
      {
        track = "";
      }
    }
} 
void next_audio_tracknbr_SD(bool prevnext)  //1=next; 0=prev
  {
    char ch;
    String track = "";
    String sstr = "";  //Search string
    int tracknbr=0;
    bool found = false;
    bool find_nbr=false;
    File trcklst = SD.open("/tracklist.txt", FILE_READ); //open file for reading
    if (nbroftracks<=1){     //get nbr of tracks (only once)
        while (trcklst.available())
        {
            ch = trcklst.read();
            if (ch == '\n'){ //this is what we want
                nbroftracks++;
            }
        } 
        track = "";
    }
    Serial.print("nbr tracks ");Serial.println(nbroftracks);
    trcklst.close();
    //get the number of the track, so we know where to start if not shuffle
    sstr=_audiotrack;    //search for current track number
    //get the number and name of that track
    //sstr=String(trcknbr)+"\t:";
    trcklst = SD.open("/tracklist.txt", FILE_READ); //open file for reading
    while (trcklst.available())
    {
        ch = trcklst.read();
        if (ch == '\n') //new line'; delete track, while not found
        {
            track = "";
        }
        else if (ch == '\t'){    //clear track after '\t:' start looking for "\t:"
            ch = trcklst.read();    //this should be ":" after \t
            if (ch == ':'){ //yes, now we have '\t:'
                tracknbr=tracknbr=atoi(track.c_str());  //store the number
                track="";   //now we read the track name, so clear
            }     
        }
        else track += ch;
        // now find the path to the current track
        if (track == sstr) {found = true; track = "";} //found current track in list, now find the next track
        while (found)   //found current track, now we need the next number
        {
            if(prevnext){
                ch = trcklst.read();    //after we found it this should be the next char '\n'
                track += ch;
                if (ch == '\n') //this is what we want
                {
                track = "";     //clear and keep reading untill we find the '\t'
                found=false;
                find_nbr=true;
                }
            }
            else{
                track = "";     //clear and keep reading untill we find the '\t'
                found=false;
                find_nbr=true;
                Serial.println(tracknbr);
                if(tracknbr==1) {tracknbr=nbroftracks;   //if 1 the go to last
                } else {tracknbr-=1;}
                next_track_SD(tracknbr);
                return; //done here
            }
        }    
        while(find_nbr){
            ch = trcklst.read();    //after we found it this should be the next char '\n'
            if (ch == '\t'){    //now start looking for "\t:" don't add to track nbr
                ch = trcklst.read();    //this should be ":" after \t
                if (ch == ':'){ //yes, now we have the tracknbr + '\t:' don't add to tracknumber
                    found=false;    //reset for next time
                    tracknbr=atoi(track.c_str());
                    Serial.println(tracknbr);
                    next_track_SD(tracknbr);
                    return; //done here
                }
                 
            }
            track += ch;
            if(track.length()>5){      //3 digits for 999 tracks, larger then 5 must be end of file
                next_track_SD(1);       //then start at track 1
                trcklst.close();
                return;
            }

        }

    }

}
void next_track_needed(bool prevnext){  //1=next; 0=prev
    if (mp3playall)
    {
        if (millis() - previousMillis >= 1000)
        {
            String str = "";
            Serial.println("mp3 ended...");
            if (shuffle){
                next_track_SD(0);
            } else {
                next_audio_tracknbr_SD(prevnext);
            }
            Serial.print("_audiotrack in next_track_needed= ");
            Serial.println(_audiotrack);
            connectto = _audiotrack.substring(12); // remove "/audiofiles/" from _audiotrack (it is added in function audiotrack)
            Serial.print("connectto= ");
            Serial.println(connectto);
            audiotrack(connectto.c_str(), 0);
            str = "MP3_data=Playing track: ";
            str.concat(_audiotrack.substring(_audiotrack.lastIndexOf("/") + 1));
            str.concat("\n");
            webSrv.send(str);
            str = _audiotrack.substring(12);
            //showFileName(str.c_str());
            previousMillis = millis();
        }
    }
}
void tracklist(File dir, int numTabs) {
    String str;
    File tracklst;
    if(nbroftracks<1){  //only first time after button push
        SD.remove("/tracklist.txt");
        tracklst = SD.open("/tracklist.txt", FILE_WRITE); //create a new file
        tracklst.close();
    }
    tracklst = SD.open("/tracklist.txt", FILE_APPEND);  //Open to add tracks
    while (true) {
        File entry =  dir.openNextFile();
        if (! entry) {      // no more files
        break;
        }
        if (entry.isDirectory()) {  //Directories
        tracklist(entry, numTabs + 1);
        } else {        //Files; files have sizes, directories do not
        nbroftracks++;
        str=String(nbroftracks); str.concat("\t:");str.concat(entry.path());str.concat("\n");
        Serial.print(str);
        tracklst.print(str);
        }
        entry.close();
    }
    str="MP3_data=Number of tracks : "; str.concat(nbroftracks); str.concat("\n"); webSrv.send(str);
    tracklst.close();
}


/***********************************************************************************************************************
*                                          M E N U E / B U T T O N S                                                   *
***********************************************************************************************************************/
/*void changeState(int state){
    if(state == _state) return;  //nothing todo
    switch(state) {
        case RADIO:{
            showHeadlineItem(RADIO);
            if(_state == RADIOico || _state == RADIOmenue){
                showStreamTitle();

            }
            else if(_state == PLAYER  || _state == PLAYERico){
                setStation(_cur_station);
                showStreamTitle();

            }
            else if(_state == CLOCKico || _state == SETTINGS){
                showLogoAndStationName();
                showStreamTitle();

            }
            else if(_state == SLEEP){
                //clearFName();
                //clearTitle();
                connecttohost(_lastconnectedhost);
                showLogoAndStationName();
                showFooter();

            }
            else{
                showLogoAndStationName();
                showStreamTitle();
                showFooter();

            }
            showVolumeBar();
            showHeadlineVolume(_cur_volume);
            break;
        }
        case RADIOico:{
            showHeadlineItem(RADIOico);
            _flashPressBtn[0] = MuteYellow;         _flashRelBtn[0] = _f_mute? MuteRed:MuteGreen;
            _flashPressBtn[1] = MP3Yellow;          _flashRelBtn[1] = MP3Blue;
            _flashPressBtn[2] = BTInYellow;         _flashRelBtn[2] = BTInBlue;
            _flashPressBtn[3] = PreviousYellow;     _flashRelBtn[3] = PreviousGreen;
            _flashPressBtn[4] = NextYellow;         _flashRelBtn[4] = NextGreen;
            _flashPressBtn[5] = ClockYellow;        _flashRelBtn[5] = ClockBlue;
            _flashPressBtn[6] = SettingsYellow;     _flashRelBtn[6] = SettingsGreen;
            //clearTitle();
            //showVolumeBar();
            //showHeadlineVolume(_cur_volume);
            for(int i = 0; i < 7 ; i++) {TJpgDec.drawJpg(i * _winButton.w, _winButton.y, _flashRelBtn[i], sizeof(AlarmBlue));}  //sizeof doesn't work within an array, just take the largest one for size
            break;
        }
        case RADIOmenue:{
            showHeadlineItem(RADIOmenue);
            _flashPressBtn[0] = MP3Yellow;  _pressBtn[0] = "/btn/MP3Yellow.jpg";     _flashRelBtn[0] = MP3Blue;
            _flashPressBtn[1] = ClockYellow;  _pressBtn[1] = "/btn/ClockYellow.jpg";   _flashRelBtn[1] = ClockBlue;
            _flashPressBtn[2] = RadioYellow;  _pressBtn[2] = "/btn/RadioYellow.jpg";   _flashRelBtn[2] = RadioBlue;
            _flashPressBtn[3] = SleepYellow;  _pressBtn[3] = "/btn/SleepYellow.jpg";   _flashRelBtn[3] = SleepBlue;
            if(TFT_CONTROLLER != 2){
                _flashPressBtn[4] = BrightnessYellow;  _pressBtn[4]="/btn/BrightnessYellow.jpg";       _flashRelBtn[4]=BrightnessBlue;
            }
            else{
                _flashPressBtn[4] = BtnBlack;  _pressBtn[4]="/btn/Black.jpg";                 _releaseBtn[4]="/btn/Black.jpg";
            }
            for(int i = 0; i < 5 ; i++) {TJpgDec.drawJpg(i * _winButton.w, _winButton.y, _flashRelBtn[i], sizeof(RadioBlue));}
            //for(int i = 0; i < 5 ; i++) {drawImage(_releaseBtn[i], i * _winButton.w, _winButton.y);}
            //clearVolBar();
            break;
        }
        case CLOCK:{
            if(_state == ALARM){
                pref.putUInt("alarm_time", _alarmtime);
                pref.putUShort("alarm_weekday", _alarmdays);
                SerialPrintfln("Alarm set to %2d:%2d on %s\n", _alarmtime / 60, _alarmtime % 60, byte_to_binary(_alarmdays));
                clearHeader();
            }
            _state = CLOCK;
            //clearAll();
            //showHeadlineItem(CLOCK);
            //if(!_f_mute) showHeadlineVolume(_cur_volume); else showHeadlineVolume(0);
            //showHeadlineTime();
            //showFooter();
            switch(clocktype){
                case 1:{display_time(true);break;}  //7_segment BMP
                case 2:{display_time(true);break;}  //Big sprite clock
                case 3:{display_time(true);break;}  //analog
            }
            break;
        }
        case CLOCKico:{
            _state = CLOCKico;
            //showHeadlineItem(CLOCKico);
            //showHeadlineVolume(_cur_volume);
            //clearMid();
            //display_time(true);
            _flashPressBtn[0] = AlarmYellow;  _pressBtn[0] = "/btn/AlarmYellow.jpg";              _flashRelBtn[0] = AlarmBlue;
            _flashPressBtn[1] = SleepYellow; _pressBtn[1] = "/btn/SleepYellow.jpg";              _flashRelBtn[1] = SleepBlue;
            _flashPressBtn[2] = RadioYellow; _pressBtn[2] = "/btn/RadioYellow.jpg";              _flashRelBtn[2] = RadioBlue;
            _flashPressBtn[3] = MuteYellow; _pressBtn[3] = "/btn/Mute_Red.jpg";                 _flashRelBtn[3] = _f_mute? MuteRed:MuteGreen;
            _flashPressBtn[4] = VolDownYellow; _pressBtn[4] = "/btn/VolDown_Yellow.jpg";           _flashRelBtn[4] = VolDown_Green;
            _flashPressBtn[5] = VolUpYellow; _pressBtn[5] = "/btn/VolUp_Yellow.jpg";             _flashRelBtn[5] = VolUp_Green;
            _flashPressBtn[0] = BtnBlack; _pressBtn[6] = "/btn/Black.jpg";                    _flashRelBtn[6] = BtnBlack; 
            //for(int i = 0; i < 6 ; i++) {drawImage(_releaseBtn[i], i * _winButton.w, _winButton.y);}
            int s=0;
            for(int i = 0; i < 7 ; i++) {TJpgDec.drawJpg(i * _winButton.w, _winButton.y, _flashRelBtn[i], sizeof(AlarmBlue));}
            break;
        }
        case BRIGHTNESS:{
            showHeadlineItem(BRIGHTNESS);
            _flashPressBtn[0] = PreviousYellow; _pressBtn[0] = "/btn/Previous_Yellow.jpg";      _flashRelBtn[0] = PreviousGreen;
            _flashPressBtn[1] = NextYellow; _pressBtn[1] = "/btn/Right_Yellow.jpg";         _flashRelBtn[1] = NextGreen;
            _flashPressBtn[2] = OKYellow; _pressBtn[2] = "/btn/OKYellow.jpg";             _flashRelBtn[2] = OKGreen;
            _flashPressBtn[3] = BtnBlack; _pressBtn[3] = "/btn/Black.jpg";                _flashRelBtn[3] = BtnBlack;
            _flashPressBtn[4] = BtnBlack; _pressBtn[4] = "/btn/Black.jpg";                _flashRelBtn[4] = BtnBlack;
            _flashPressBtn[5] = BtnBlack; _pressBtn[5] = "/btn/Black.jpg";                _flashRelBtn[5] = BtnBlack;
            _flashPressBtn[6] = BtnBlack; _pressBtn[6] = "/btn/Black.jpg";                _flashRelBtn[6] = BtnBlack;
            clearMid();
            clearFooter();
            drawImage("/common/Brightness.jpg", 0, _winName.y);
            showBrightnessBar();
            //for(int i = 0; i < 5 ; i++) {drawImage(_releaseBtn[i], i * _winButton.w, _winButton.y);}drawImage(_releaseBtn[btnNr], btnNr * _winButton.w , _dispHeight - _winButton.h);
            for(int i = 0; i < 7 ; i++) {TJpgDec.drawJpg(i * _winButton.w, _dispHeight - _winButton.h, _flashRelBtn[i], sizeof(OKGreen));}
            break;
        }
        case PLAYER:{
            if(_state == RADIO){
                clearFName();
                clearTitle();
            }
            showHeadlineItem(PLAYER);
            _flashPressBtn[0] = MuteYellow;        _flashRelBtn[0] = _f_mute? MuteRed:MuteGreen;
            _flashPressBtn[1] = RadioYellow;       _flashRelBtn[1] = RadioBlue;
            _flashPressBtn[2] = BTInYellow;        _flashRelBtn[2] = BTInBlue;
            _flashPressBtn[3] = PreviousYellow;    _flashRelBtn[3] = PreviousGreen;
            _flashPressBtn[4] = NextYellow;        _flashRelBtn[4] = NextGreen;
            _flashPressBtn[5] = shuffle?ShuffleYellow:ShuffleGreen;     _flashRelBtn[5] = shuffle?ShuffleYellow:ShuffleGreen;
            _flashPressBtn[6] = OKYellow;          _flashRelBtn[6] = OKGreen;
            //for(int i = 0; i < 5 ; i++) {drawImage(_releaseBtn[i], i * _winButton.w, _winButton.y);}
            for(int i = 0; i < 7 ; i++) {TJpgDec.drawJpg(i * _winButton.w, _winButton.y, _flashRelBtn[i], sizeof(BTInBlue));}
            clearFName();
            showVolumeBar();
            showHeadlineVolume(_cur_volume);
            break;
        }
        case PLAYERico:{
            showHeadlineItem(PLAYERico);
            _flashPressBtn[0] = MuteYellow; _pressBtn[0] = "/btn/Button_Mute_Red.jpg";         _flashRelBtn[0] = _f_mute? MuteRed:MuteGreen;
            _flashPressBtn[1] = VolDownYellow; _pressBtn[1] = "/btn/VolDown_Yellow.jpg";          _flashRelBtn[1] = VolDown_Green;
            _flashPressBtn[2] = VolUpYellow; _pressBtn[2] = "/btn/VolUp_Yellow.jpg";            _flashRelBtn[2] = VolUp_Green;
            _flashPressBtn[3] = MP3Yellow; _pressBtn[3] = "/btn/MP3Yellow.jpg";               _flashRelBtn[3]=MP3Blue;
            _flashPressBtn[4] = RadioYellow; _pressBtn[4] = "/btn/RadioYellow.jpg";             _flashRelBtn[4] = RadioBlue;
            //for(int i = 0; i < 5 ; i++) {drawImage(_releaseBtn[i], i * _winButton.w, _winButton.y);}
            for(int i = 0; i < 5 ; i++) {TJpgDec.drawJpg(i * _winButton.w, _winButton.y, _flashRelBtn[i], sizeof(AlarmBlue));}
            break;
        }
        case ALARM:{
            _flashPressBtn[0] = PreviousYellow; _pressBtn[0] = "/btn/Button_Left_Yellow.jpg";    _flashRelBtn[0] = PreviousGreen;
            _flashPressBtn[1] = NextYellow; _pressBtn[1] = "/btn/Button_Right_Yellow.jpg";   _flashRelBtn[1] = NextGreen;
            _flashPressBtn[2] = UpYellow; _pressBtn[2] = "/btn/Button_Up_Yellow.jpg";      _flashRelBtn[2] = UpGreen;
            _flashPressBtn[3] = DownYellow; _pressBtn[3] = "/btn/Button_Down_Yellow.jpg";    _flashRelBtn[3] = DownGreen;
            _flashPressBtn[4] = OKYellow; _pressBtn[4] = "/btn/Button_Ready_Yellow.jpg";   _flashRelBtn[4] = OKGreen;
            clearAll();
            display_alarmtime(0, 0, true);
            display_alarmDays(0, true);
            //for(int i = 0; i < 5 ; i++) {drawImage(_releaseBtn[i], i * _winButton.w,  _dispHeight - _winButton.h);}
            for(int i = 0; i < 5 ; i++) {TJpgDec.drawJpg(i * _winButton.w, _dispHeight - _winButton.h, _flashRelBtn[i], sizeof(OKGreen));}
            break;
        }
        case SLEEP:{
            showHeadlineItem(SLEEP);
            _flashPressBtn[0] = UpYellow; _pressBtn[0] = "/btn/Up_Yellow.jpg";                 _flashRelBtn[0] = UpGreen;
            _flashPressBtn[1] = DownYellow; _pressBtn[1] = "/btn/Down_Yellow.jpg";               _flashRelBtn[1] = DownGreen;
            _flashPressBtn[2] = OKYellow; _pressBtn[2] = "/btn/OKYellow.jpg";                  _flashRelBtn[2] = OKGreen;
            _flashPressBtn[3] = BtnBlack; _pressBtn[3] = "/btn/Black.jpg";                     _flashRelBtn[3] = BtnBlack;
            _flashPressBtn[4] = CancelYellow; _pressBtn[4] = "/btn/Button_Cancel_Yellow.jpg";      _flashRelBtn[4] = CancelGreen;
            clearMid();
            display_sleeptime();
            if(TFT_CONTROLLER < 2) drawImage("/common/Night_Gown.bmp", 198, 23);
            else                   drawImage("/common/Night_Gown.bmp", 280, 30);
            //for(int i = 0; i < 5 ; i++) {drawImage(_releaseBtn[i], i * _winButton.w, _winButton.y);}
            for(int i = 0; i < 5 ; i++) {TJpgDec.drawJpg(i * _winButton.w, _winButton.y, _flashRelBtn[i], sizeof(AlarmBlue));}
            break;
        }
        case SETTINGS:{
            showHeadlineItem(SETTINGS);
            //input click is change
            _flashPressBtn[0] = RadioYellow;                        if(input==1) {_flashRelBtn[0] = RadioBlue;}
                                                                    else if(input==2) {_flashRelBtn[0]=MP3Blue;}
                                                                    else if(input==3) {_flashRelBtn[0]=BTInBlue;} 
            _flashPressBtn[1] = AlarmYellow;                        _flashRelBtn[1]=AlarmBlue;                       
            _flashPressBtn[2] = SleepYellow;                        _flashRelBtn[2]= SleepBlue;                
            //output, click is change
            _flashPressBtn[3] = SpeakerOutBlue;                   if(output==1) {_flashRelBtn[3] = SpeakerOutBlue;}
                                                                    else if(output==2) {_flashRelBtn[3]=BTOutBlue;}           
            _flashPressBtn[4] = ClockYellow;                        _flashRelBtn[4]=ClockBlue;               
            _flashPressBtn[5] = BrightnessYellow;                   _flashRelBtn[5]=BrightnessBlue;                     
            _flashPressBtn[6] = OKYellow;                           _flashRelBtn[6]=OKGreen;                
            _state = SETTINGS;
            clearMid();
            tft.setCursor(5, 40);
            tft.loadFont(AA_FONT_NORMAL);
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.print("Input: "); 
            switch(input){
                case 1: {tft.println("Internet Radio"); break;}
                case 2: {tft.println("SD-card files"); break;}
                case 3: {tft.println("BT in"); break;}
            }
            tft.print("Output: ");
            switch(output){
                case 1: {tft.println("Speaker"); break;}
                case 2: {tft.println("BT out"); break;}
            }
            tft.print("Clock type: ");
            switch(clocktype){
                case 1:{tft.println("Old digi clock"); break;}
                case 2:{tft.println("Digital clock/date"); break;}
                case 3:{tft.println("Analog clock"); break;}
            }
            
            //for(int i = 0; i < 7 ; i++) {drawImage(_releaseBtn[i], i * _winButton.w, _winButton.y);}
            for(int i = 0; i < 7 ; i++) {TJpgDec.drawJpg(i * _winButton.w, _winButton.y, _flashRelBtn[i], sizeof(AlarmBlue));}
            break;
        }
    }
    _state = state;
}*/
/***********************************************************************************************************************
*                                                      L O O P                                                         *
***********************************************************************************************************************/
void DigiClock() //date and time, ip and strength
{
    //int xpos = tft.width() / 2; // Half the screen width
    int ypos = 40;
    //spr.setTextDatum(C_BASELINE);                                               //using spr (=sprite) there is no need for a black rectangle, to wipe out previous. 
    if (timeinfo.tm_mday > previous_day || previous_day==100){    //date change only once per day{                                        //Now there is no interuption in display
      //spr.setTextColor(TFT_YELLOW, TFT_BLACK);                                  //But fonts need to be in a .h file, SPIFFS will not do
      //spr.loadFont(AA_FONT_num50);                                              //So there are only 3 and only numbers to keep it small
      sprintf(timc, " %02d-%02d-%04d ", timeinfo.tm_mday,timeinfo.tm_mon+1,timeinfo.tm_year+1900);
      Serial.println(timc);
      //sprintf(buf,"%02d-%02d-%02d",  timeinfo.tm_mday, timeinfo.tm_mon, timeinfo.tm_year); 
      //tft.setCursor(xpos-((spr.textWidth(timc))/2), ypos);                      //cursor (reference) is set for tft, not spr
      //spr.printToSprite(timc);
      previous_day=timeinfo.tm_mday;
    }
  
    ypos += 100;  // move ypos down
    if (timeinfo.tm_sec == 0 || previous_sec==100){  //previous_sec=100 at start
      //sprintf(buf,"%02d:%02d",  timeinfo.tm_hour, timeinfo.tm_min);
      strftime(timc, 12, " %H:%M ", &timeinfo); 
      /*spr.setTextColor(TFT_WHITE, TFT_BLACK);
      spr.loadFont(AA_FONT_num100);
      tft.setCursor(xpos - 45 - ((spr.textWidth(timc))/2), ypos);
      spr.printToSprite(timc);    // Prints to tft cursor position, tft cursor NOT moved
      spr.loadFont(AA_FONT_SMALL);
      spr.setTextColor(TFT_DEEPSKYBLUE, TFT_BLACK);
      tft.setCursor(10, 300);
      spr.printToSprite(_myIP);//_myIP
      tft.setCursor(430, 300);
      itoa(WiFi.RSSI(), timc, 10);
      Serial.println(timc);
      spr.printToSprite(timc);*/
    }
    
    //spr.setTextColor(TFT_DARKGREEN, TFT_BLACK);
    //spr.loadFont(AA_FONT_num50);
    //tft.setCursor(xpos+90, ypos+40);
    sprintf(timc, ":%02d ", timeinfo.tm_sec);
    previous_sec=timeinfo.tm_sec;
    //spr.printToSprite(timc);
    //spr.unloadFont();
  
}
 
void timer_stuff(void)
{
    if (interruptCounter > 0) //run every sec
    {
        portENTER_CRITICAL(&timerMux); //portenter and exit are needed as to block it for other processes
        interruptCounter--;
        portEXIT_CRITICAL(&timerMux);
        totalcounter++;
        mediumcounter++;
        time(&now);
	    localtime_r(&now, &timeinfo);
        static uint8_t sec=0;
        /*if(_touchCnt){
            _touchCnt--;
            if(!_touchCnt){
                if(_state == RADIOico)   changeState(RADIO);
                if(_state == RADIOmenue) changeState(RADIO);
                if(_state == CLOCKico)   changeState(CLOCK);
            }
        }*/
        if(_f_rtc==true){ // true -> rtc has the current time
            int8_t h=0;
            String time_s;
            xSemaphoreTake(mutex_rtc, portMAX_DELAY);
            time_s = gettime_s();
            xSemaphoreGive(mutex_rtc);
            //if(!BTTask_runs && !audioTask_runs){audioInit();}
            //if(_state != ALARM && !_f_sleeping) showHeadlineTime();
            //if(_state == CLOCK || _state == CLOCKico) display_time();
            if(_f_eof && (_state == RADIO || _f_eof_alarm)){
                _f_eof = false;
                if(_f_eof_alarm){
                    _f_eof_alarm = false;
                    if(_f_mute){
                        mute(); // mute off
                    }
                }
                connecttohost(_lastconnectedhost);
            }
            if(_f_eof && _state == PLAYER){
                _f_eof = false;
                next_track_needed(true);
            }
            if((_f_mute==false)&&(!_f_sleeping)){
                if(time_s.endsWith("59:53") && _state == RADIO) { // speech the time 7 sec before a new hour is arrived
                    String hour = time_s.substring(0,2); // extract the hour
                    h = hour.toInt();
                    h++;
                    if( h== 24) h=0;
                    sprintf (_chbuf, "/voice_time/%d_00.mp3", h);
                    connecttoFS(_chbuf);
                }
            }
            if(_alarmtime == getMinuteOfTheDay()){ //is alarmtime?
                log_i("is alarmtime");
                if((_alarmdays>>getweekday())&1){ //is alarmday?
                    if(!_f_semaphore) {_f_alarm = true;  _f_semaphore = true;} //set alarmflag
                }
            }
            else _f_semaphore=false;
            if(_f_alarm){
                SerialPrintfln("Alarm");
                _f_alarm=false;
                connecttoFS("/ring/alarm_clock.mp3");
                audioSetVolume(21);
            }
        }
        if(_commercial_dur > 0){
            _commercial_dur--;
            //if((_commercial_dur == 2) && (_state == RADIO)) clearTitle();// end of commercial? clear streamtitle
        }
        if(_f_newIcyDescription){
            _f_newIcyDescription = false;
            webSrv.send("icy_description=" +_icydescription);
        }
        if(!WL_CONNECTED) {
            Serial.printf("WiFi not connected! trying to connect");
            wifi_conn();
        }
    }
    if (mediumcounter > 59) //run every minute
    {
        mediumcounter -= 60;  //back to 0 first, it might go higher again during the program
        if(!_f_rtc) {   //check every minute when time is not sync
            xSemaphoreTake(mutex_rtc, portMAX_DELAY);
            get_time();
            xSemaphoreGive(mutex_rtc);
        }
        //if ((_state==CLOCK || _state==CLOCKico) && (clocktype==1)) {if(clocktype==1)display_time(true); else if(clocktype==2) DigiClock();else if(clocktype==3) DigiClock();}
        //updateSleepTime();
        if(LOG){
            File log;
            log = SD.open("/log.txt", FILE_APPEND);
            String str=gettime_s(); str.concat("\t"); str.concat(_state); str.concat("\t"); str.concat(_audiotrack); str.concat("\t"); str.concat(_stationURL); str.concat("\n"); 
            log.print(str);
        }
    }
    if (totalcounter > 3599) //run every hour
    {
        totalcounter -= 3600;
        xSemaphoreTake(mutex_rtc, portMAX_DELAY);
        get_time(); //sync time every hour
        xSemaphoreGive(mutex_rtc);

    }
}

void loop() {
    static uint8_t sec=0;
    if(webSrv.loop()) return; // if true: ignore all other for faster response to web
    //ir.loop();
    ftpSrv.handleFTP();
    timer_stuff();
}
/***********************************************************************************************************************
*                                                    E V E N T S                                                       *
***********************************************************************************************************************/
//Events from vs1053_ext library
void vs1053_info(const char *info){
    SerialPrintfln("%s", info);
    if(endsWith(info, "Stream lost")) SerialPrintfln("%s", info);
}
void audio_info(const char *info){
    // SerialPrintfln("%s", info);
    if(startsWith(info, "FLAC")) SerialPrintfln("%s", info);
    if(endsWith(info, "Stream lost")) SerialPrintfln("%s", info);
}
//----------------------------------------------------------------------------------------
void vs1053_showstation(const char *info){
    _stationName_air = info;
    //if(!_cur_station) showLogoAndStationName();
}
void audio_showstation(const char *info){
    _stationName_air = info;
    //if(!_cur_station) showLogoAndStationName();
}
//----------------------------------------------------------------------------------------
void vs1053_showstreamtitle(const char *info){
    if(_f_irNumberSeen) return; // discard streamtitle
    _streamTitle = info;
    //if(_state == RADIO) showStreamTitle();
    SerialPrintfln("StreamTitle: %s", info);
}
void audio_showstreamtitle(const char *info){
    if(_f_irNumberSeen) return; // discard streamtitle
    _streamTitle = info;
    //if(_state == RADIO) showStreamTitle();
    SerialPrintfln("StreamTitle: %s", info);
}
//----------------------------------------------------------------------------------------
void vs1053_commercial(const char *info){
    _commercial_dur = atoi(info) / 1000;                // info is the duration of advertising in ms
    _streamTitle = "Advertising: " + (String) _commercial_dur + "s";
    //showStreamTitle();
    SerialPrintfln("StreamTitle: %s", info);
}
void audio_commercial(const char *info){
    _commercial_dur = atoi(info) / 1000;                // info is the duration of advertising in ms
    _streamTitle = "Advertising: " + (String) _commercial_dur + "s";
    //showStreamTitle();
    SerialPrintfln("StreamTitle: %s", info);
}
//----------------------------------------------------------------------------------------
void vs1053_eof_mp3(const char *info){                  // end of mp3 file (filename)
    _f_eof = true;
    if(startsWith(info, "alarm")) _f_eof_alarm = true;
    SerialPrintfln("end of file: %s", info);
}
void audio_eof_mp3(const char *info){                  // end of mp3 file (filename)
    _f_eof = true;
    if(startsWith(info, "alarm")) _f_eof_alarm = true;
    SerialPrintfln("end of file: %s", info);
}
//----------------------------------------------------------------------------------------
void vs1053_lasthost(const char *info){                 // really connected URL
    free(_lastconnectedhost);
    _lastconnectedhost = strdup(info);
    SerialPrintfln("lastURL: %s", _lastconnectedhost);
}
void audio_lasthost(const char *info){                 // really connected URL
    free(_lastconnectedhost);
    _lastconnectedhost = strdup(info);
    SerialPrintfln("lastURL: %s", _lastconnectedhost);
}
//----------------------------------------------------------------------------------------
void vs1053_icyurl(const char *info){                   // if the Radio has a homepage, this event is calling
    if(strlen(info) > 5){
        SerialPrintfln("icy-url: %s", info);
        _homepage = String(info);
        if(!_homepage.startsWith("http")) _homepage = "http://" + _homepage;
    }
}
void audio_icyurl(const char *info){                   // if the Radio has a homepage, this event is calling
    if(strlen(info) > 5){
        SerialPrintfln("icy-url: %s", info);
        _homepage = String(info);
        if(!_homepage.startsWith("http")) _homepage = "http://" + _homepage;
    }
}
//----------------------------------------------------------------------------------------
void vs1053_id3data(const char *info){
    SerialPrintfln("id3data: %s", info);
    String i3d=info;
    if (i3d.startsWith("Artist: ")) artsong=(i3d.substring(8).c_str());
    else if (i3d.startsWith("Title: ")) Title=(i3d.substring(7).c_str());
    else return;
    if (!artsong.isEmpty() && !Title.isEmpty()) {
        artsong.concat(" - "); artsong.concat(Title); 
        //showArtistSongAudioFile();
    }  
}
void audio_id3data(const char *info){
    SerialPrintfln("id3data: %s", info);
    String i3d=info;
    if (i3d.startsWith("Artist: "))  artsong=(i3d.substring(8).c_str());
    else if (i3d.startsWith("Title: ")) Title=(i3d.substring(7).c_str());
    else return;
    if (!artsong.isEmpty() && !Title.isEmpty()) {
        artsong.concat(" - "); artsong.concat(Title); 
        //showArtistSongAudioFile();
    }  
}
//----------------------------------------------------------------------------------------
void vs1053_icydescription(const char *info){
    _icydescription = String(info);
    if(_streamTitle.length()==0 && _state == RADIO){
        _streamTitle = String(info);
        //showStreamTitle();
    }
    if(strlen(info)){
        _f_newIcyDescription = true;
        SerialPrintfln("icy-descr: %s", info);
    }
}
void audio_icydescription(const char *info){
    _icydescription = String(info);
    if(_streamTitle.length()==0 && _state == RADIO){
        _streamTitle = String(info);
        //showStreamTitle();
    }
    if(strlen(info)){
        _f_newIcyDescription = true;
        SerialPrintfln("icy-descr: %s", info);
    }
}
//----------------------------------------------------------------------------------------
void ftp_debug(const char* info) {
    if(startsWith(info, "File Name")) return;
    SerialPrintfln("ftpsrv: %s", info);
}
//----------------------------------------------------------------------------------------
void RTIME_info(const char *info){
    Serial.printf("rtime_info : %s\n", info);
}

//Events from tft library
void tft_info(const char *info){
    Serial.printf("tft_info   : %s\n", info);
}
/*
// Events from IR Library
void ir_res(uint32_t res){
    _f_irNumberSeen = false;
    if(_state != RADIO) return;
    if(res != 0){
       setStation(res);
    }
    else{
        setStation(_cur_station); // valid between 1 ... 999
    }
    return;
}
void ir_number(const char* num){
    _f_irNumberSeen = true;
    if(_state != RADIO) return;
    /*tft.fillRect(_winName.x, _winName.y, _winName.w , _winName.h + _winTitle.h, TFT_BLACK);
    tft.setTextSize(7); // tft.setFont(Big_Numbers133x156);
    tft.setTextColor(TFT_GOLD);
    tft.setCursor(100, 80);
    tft.print(num);
}
void ir_key(const char* key){

    if(_f_sleeping) {_f_sleeping = false;  changeState(RADIO);} // awake

    switch(key[0]){
        case 'k':       if(_state == SLEEP) {                           // OK
                            updateSleepTime(true);
                            changeState(RADIO);
                        }
                        break;
        case 'r':       upvolume();                                     // right
                        break;
        case 'l':       downvolume();                                   // left
                        break;
        case 'u':       if(_state==RADIO) nextStation();                // up
                        if(_state==SLEEP) display_sleeptime(1);
                        break;
        case 'd':       if(_state==RADIO) prevStation();                // down
                        if(_state==SLEEP) display_sleeptime(-1);
                        break;
        case '#':       mute();                                         // #
                        break;
        case '*':       if(     _state == RADIO) changeState(SLEEP);    // *
                        else if(_state == SLEEP) changeState(RADIO);
                        break;
        default:        break;
    }
}
// Event from TouchPad
/*void changeBtn_pressed(uint8_t btnNr){
    if(_state == ALARM || _state == BRIGHTNESS) {TJpgDec.drawJpg(btnNr * _winButton.w, _dispHeight - _winButton.h, _flashPressBtn[btnNr], sizeof(RadioYellow));}//drawImage(_pressBtn[btnNr], btnNr * _winButton.w , _dispHeight - _winButton.h);
    else                {TJpgDec.drawJpg(btnNr * _winButton.w, _winButton.y, _flashPressBtn[btnNr], sizeof(RadioYellow));}//drawImage(_pressBtn[btnNr], btnNr * _winButton.w , _winButton.y);
}
void changeBtn_released(uint8_t btnNr){
    if(_state == RADIOico || _state == PLAYER){
        if(_f_mute)  _flashRelBtn[0] = MuteRed;
        else         _flashRelBtn[0] = MuteGreen;
        if(_BT_In)   _flashRelBtn[2] = BTInYellow;
        else         _flashRelBtn[2] = BTInBlue;

    }
    if(_state == PLAYER){
        if(shuffle)  _flashRelBtn[5] = ShuffleYellow;
        else         _flashRelBtn[5] = ShuffleGreen;
    }
    if(_state == CLOCKico){
        if(_f_mute)  _flashRelBtn[3] = MuteRed;
        else         _flashRelBtn[3] = MuteGreen;
    }
    if(_state == ALARM || _state == BRIGHTNESS) {TJpgDec.drawJpg(btnNr * _winButton.w, _dispHeight - _winButton.h, _flashRelBtn[btnNr], sizeof(RadioYellow));}//drawImage(_releaseBtn[btnNr], btnNr * _winButton.w , _dispHeight - _winButton.h);
    else                {TJpgDec.drawJpg(btnNr * _winButton.w, _winButton.y, _flashRelBtn[btnNr], sizeof(RadioYellow));}//drawImage(_releaseBtn[btnNr], btnNr * _winButton.w , _winButton.y);
}
void tp_pressed(uint16_t x, uint16_t y){
    log_i("tp_pressed, state is: %i", _state);
    _touchCnt = TouchCnt;
    enum : int8_t{none = -1, RADIO_1, RADIOico_1, RADIOico_2, RADIOmenue_1,
                             PLAYER_1, PLAYERico_1, ALARM_1, BRIGHTNESS_1,
                             CLOCK_1, CLOCKico_1, ALARM_2, SLEEP_1, VOLUME_1, SETTINGS_1};
    int8_t yPos    = none;
    int8_t btnNr   = none; // buttonnumber

    if(_f_sleeping) return; // awake in tp_released()

    switch(_state){
        case RADIO:         if(                     y <= _winTitle.y-40)                 {yPos = RADIO_1;}
                            else if(                y <= _winButton.y)                   {yPos = VOLUME_1;}
                            break;
        case RADIOico:      if(                     y <= _winTitle.y-40)                 {yPos = RADIOico_1;}
                            else if(_winButton.y <= y && y <= _winButton.y + _winButton.h) {yPos = RADIOico_2;   btnNr = x / _winButton.w;}
                            else if(                y <= _winButton.y)                   {yPos = VOLUME_1;}
                            break;
        case RADIOmenue:    if(_winButton.y <= y && y <= _winButton.y + _winButton.h) {yPos = RADIOmenue_1; btnNr = x / _winButton.w;}
        else if(                y <= _winButton.y)                   {yPos = VOLUME_1;}
                            break;
        case PLAYER:        if(_winButton.y <= y && y <= _winButton.y + _winButton.h) {yPos = PLAYER_1;     btnNr = x / _winButton.w;}
                            else if(                y <= _winButton.y)                   {yPos = VOLUME_1;}
                            break;
        case PLAYERico:     if(_winButton.y <= y && y <= _winButton.y + _winButton.h) {yPos = PLAYERico_1;  btnNr = x / _winButton.w;}
                            else if(                y <= _winButton.y)                   {yPos = VOLUME_1;}
                            break;
        case CLOCK:         if(                     y <= _winTitle.y)                 {yPos = CLOCK_1;}
                            break;
        case CLOCKico:      if(_winButton.y <= y && y <= _winButton.y + _winButton.h) {yPos = CLOCKico_1; btnNr = x / _winButton.w;}
                            break;
        case ALARM:         if(                     y <= _alarmdays_h)                {yPos = ALARM_1; btnNr = (x - 2) / _alarmdays_w;} //weekdays
                            if(                     y >= _winButton.y + _winFooter.h) {yPos = ALARM_2; btnNr = x / _winButton.w;}
                            break;
        case SLEEP:         if(_winButton.y <= y && y <= _winButton.y + _winButton.h) {yPos = SLEEP_1; btnNr = x / _winButton.w;}
                            break;
        case BRIGHTNESS:    if(_winButton.y <= y && y <= _winButton.y + _winButton.h) {yPos = BRIGHTNESS_1; btnNr = x / _winButton.w;}
                            break;
        case SETTINGS:      if(_winButton.y <= y && y <= _winButton.y + _winButton.h) {yPos = SETTINGS_1; btnNr = x / _winButton.w;}
                            break;
        default:            break;
    }
    if(yPos == none) {log_w("Touchpoint not valid x=%d, y=%d", x, y); return;}

    switch(yPos){
        case RADIO_1:       changeState(RADIOico); break;
        //case RADIOico_1:    changeState(RADIOmenue); break;
        case CLOCK_1:       changeState(CLOCKico);   break;
        case RADIOico_2:    if(btnNr == 0){_releaseNr =  0; mute();}
                            else if(btnNr == 1){_releaseNr =  1; } // Mute
                            else if(btnNr == 2){_releaseNr =  2; } // station--
                            else if(btnNr == 3){_releaseNr =  3; } // station++  MP3
                            else if(btnNr == 4){_releaseNr =  4; } // MP3  
                            else if(btnNr == 5){_releaseNr =  5; } // Clock
                            else if(btnNr == 6){_releaseNr =  6; } // Settings (RADIOmenue_1) => Sleep; Alarm; clocktype at start; brightness
                            changeBtn_pressed(btnNr); break;
        case RADIOmenue_1:  if(btnNr == 0){_releaseNr = 10; stopSong(); listAudioFile();} // AudioPlayer
                            if(btnNr == 1){_releaseNr = 11;} // Clock
                            if(btnNr == 2){_releaseNr = 12;} // Radio
                            if(btnNr == 3){_releaseNr = 13;} // Sleep
                            if(TFT_CONTROLLER != 2){
                            if(btnNr == 4){_releaseNr = 14;} // Brightness
                            }
                            changeBtn_pressed(btnNr); break;
        case CLOCKico_1:    if(btnNr == 0){_releaseNr = 20;} // Alarm
                            if(btnNr == 1){_releaseNr = 21;} // Sleep
                            if(btnNr == 2){_releaseNr = 22;} // Radio
                            if(btnNr == 3){_releaseNr = 23; mute();}
                            if(btnNr == 4){_releaseNr = 24; } // Vol-
                            if(btnNr == 5){_releaseNr = 25; } // Vol+
                            changeBtn_pressed(btnNr); break;
        case ALARM_2:       if(btnNr == 0){_releaseNr = 30;} // left
                            if(btnNr == 1){_releaseNr = 31;} // right
                            if(btnNr == 2){_releaseNr = 32;} // up
                            if(btnNr == 3){_releaseNr = 33;} // down
                            if(btnNr == 4){_releaseNr = 34;} // ready (return to CLOCK)
                            changeBtn_pressed(btnNr); break;
        case PLAYER_1:      if(btnNr == 0){_releaseNr = 40; mute();} // Mute
                            if(btnNr == 1){_releaseNr = 41;} // RADIO
                            if(btnNr == 2){_releaseNr = 42;} // BTIn
                            if(btnNr == 3){_releaseNr = 43;} // Previous
                            if(btnNr == 4){_releaseNr = 44;} // Next
                            if(btnNr == 5){_releaseNr = 45;} // Shuffle
                            if(btnNr == 6){_releaseNr = 46;} // OK/Play
                            changeBtn_pressed(btnNr); break;
        case PLAYERico_1:   if(btnNr == 0){_releaseNr = 50; mute();}
                            if(btnNr == 1){_releaseNr = 51; } // Vol-
                            if(btnNr == 2){_releaseNr = 52; } // Vol+
                            if(btnNr == 3){_releaseNr = 53;} // PLAYER
                            if(btnNr == 4){_releaseNr = 54;} // RADIO
                            changeBtn_pressed(btnNr); break;
        case ALARM_1:       if(btnNr == 0){_releaseNr = 60;} // mon
                            if(btnNr == 1){_releaseNr = 61;} // tue
                            if(btnNr == 2){_releaseNr = 62;} // wed
                            if(btnNr == 3){_releaseNr = 63;} // thu
                            if(btnNr == 4){_releaseNr = 64;} // fri
                            if(btnNr == 5){_releaseNr = 65;} // sat
                            if(btnNr == 6){_releaseNr = 66;} // sun
                            break;
        case SLEEP_1:       if(btnNr == 0){_releaseNr = 70;} // sleeptime up
                            if(btnNr == 1){_releaseNr = 71;} // sleeptime down
                            if(btnNr == 2){_releaseNr = 72;} // display_sleeptime(0, true);} // ready, return to RADIO
                            if(btnNr == 3){_releaseNr = 73;} // unused
                            if(btnNr == 4){_releaseNr = 74;} // return to RADIO without saving sleeptime
                            changeBtn_pressed(btnNr); break;
        case BRIGHTNESS_1:  if(btnNr == 0){_releaseNr = 80;} // darker
                            if(btnNr == 1){_releaseNr = 81;} // brighter
                            if(btnNr == 2){_releaseNr = 82;} // okay
                            changeBtn_pressed(btnNr); break;
        case SETTINGS_1:    if(btnNr == 0){_releaseNr = 90;} // Radio 21; Player_SD 10; Player network xx; BT_in xx
                            if(btnNr == 1){_releaseNr = 91;} // Output
                            if(btnNr == 2){_releaseNr = 92;} // Brightness
                            if(btnNr == 3){_releaseNr = 93;} // Alarm
                            if(btnNr == 4){_releaseNr = 94;} // Sleep
                            if(btnNr == 5){_releaseNr = 95;}         // spare
                            if(btnNr == 6){_releaseNr = 96;} // return to active input
                            changeBtn_pressed(btnNr); break;
        case VOLUME_1:      uint8_t vol=map(x, 0, 480, 0, 21);setVolume(vol); showVolumeBar(); showHeadlineVolume(_cur_volume); 
                            String str="displaysetvolume="; str.concat(_cur_volume); Serial.println(str); webSrv.send(str); break;
    }
}
void tp_released(){
    log_i("tp_released, state is: %i", _state);
    const char* chptr = NULL;
    char path[256 + 12] = "/audiofiles/";
    uint16_t w = 64, h = 64; 
    if(_f_sleeping == true){ //awake
        _f_sleeping = false;
        SerialPrintfln("awake");
        setTFTbrightness(pref.getUShort("brightness"));
        changeState(RADIO);
        connecttohost(_lastconnectedhost);
        showLogoAndStationName();
        showFooter();
        showHeadlineItem(RADIO);
        showHeadlineVolume(_cur_volume);
        return;
    }

    switch(_releaseNr){
         RADIOico ******************************
        case  0:    changeBtn_released(0); break;                                    // Mute
        case  1:    changeState(PLAYER);                                             // Player SD
                    if(setAudioFolder("/audiofiles")) chptr = listAudioFile();
                    if(chptr) strcpy(_afn, chptr);
                    showFileName(_afn); webSrv.send("StatePlayer"); break;
        case  2:    if(_BT_In) {a2dp_sink.end(false); _BT_In=false; wifi_conn();} else {a2dp_sink.start("BTESP32MusicPlayer");_BT_In=true;} break;         //BT_in
        case  3:    prevStation(); showFooterStaNr(); changeBtn_released(1); break;  // previousstation
        case  4:    nextStation(); showFooterStaNr(); changeBtn_released(2); break;  // nextstation
        case  5:    changeState(CLOCK); break;  //  
        case  6:    changeState(SETTINGS); break;                                   //  Settings

          Will be removed ***  RADIOmenue ******************************
        case 10:    changeState(PLAYER); webSrv.send("StatePlayer");
                    if(setAudioFolder("/audiofiles")) chptr = listAudioFile();
                    if(chptr) strcpy(_afn, chptr);
                    showFileName(_afn); break;
        case 11:    changeState(CLOCK); break;
        case 12:    changeState(RADIO); webSrv.send("StateRadio");break;
        case 13:    changeState(SLEEP); break;
        case 14:    changeState(BRIGHTNESS); break;

        / CLOCKico ******************************
        case 20:    changeState(ALARM); break;
        case 21:    changeState(SLEEP); break;
        case 22:    changeState(RADIO); webSrv.send("StateRadio");break;
        case 23:    changeBtn_released(3); break; // Mute
        case 24:    changeBtn_released(4); downvolume(); break;
        case 25:    changeBtn_released(5); upvolume();  break;

         ALARM ******************************
        case 30:    changeBtn_released(0); display_alarmtime(-1 ,  0); break;
        case 31:    changeBtn_released(1); display_alarmtime( 1 ,  0); break;
        case 32:    changeBtn_released(2); display_alarmtime( 0 ,  1); break;
        case 33:    changeBtn_released(3); display_alarmtime( 0 , -1); break;
        case 34:    changeState(CLOCK); break;

         AUDIOPLAYER ******************************
        case 40:    changeBtn_released(0); break; //Mute
        case 41:    changeState(RADIO); webSrv.send("StateRadio");break;
        case 42:    changeState(CLOCK); break;      //BTin
        case 43:    changeBtn_released(3); // first audiofile
                    if(setAudioFolder("/audiofiles")) chptr = listAudioFile();
                    if(chptr) strcpy(_afn, chptr);
                    showFileName(_afn); break;
        case 44:    changeBtn_released(4); // next audiofile
                    chptr = listAudioFile();
                    if(chptr) strcpy(_afn ,chptr);
                    showFileName(_afn); break;
        case 45:    if(!shuffle){shuffle=true;webSrv.send("shuffle=1");}else{shuffle=false;webSrv.send("shuffle=0");} changeBtn_released(5);break; 
        case 46:    showVolumeBar(); // ready
                    strcat(path, _afn);
                    connecttoFS((const char*) path);
                    if(_f_isFSConnected){
                        free(_lastconnectedfile);
                        _lastconnectedfile = strdup(path);
                    } break;

        / AUDIOPLAYERico ******************************
        case 50:    changeBtn_released(0); break; // Mute
        case 51:    changeBtn_released(1); downvolume(); showVolumeBar(); showHeadlineVolume(_cur_volume); break; // Vol-
        case 52:    changeBtn_released(2); upvolume();   showVolumeBar(); showHeadlineVolume(_cur_volume); break; // Vol+
        case 53:    changeState(PLAYER); webSrv.send("StatePlayer");showFileName(_afn); break;
        case 54:    changeState(RADIO); webSrv.send("StateRadio"); break;*/

        /* ALARM (weekdays) ******************************
        case 60:    display_alarmDays(0); break;
        case 61:    display_alarmDays(1); break;
        case 62:    display_alarmDays(2); break;
        case 63:    display_alarmDays(3); break;
        case 64:    display_alarmDays(4); break;
        case 65:    display_alarmDays(5); break;
        case 66:    display_alarmDays(6); break;

        / SLEEP ******************************************
        case 70:    display_sleeptime(1);  changeBtn_released(0); break;
        case 71:    display_sleeptime(-1); changeBtn_released(1); break;
        case 72:    updateSleepTime(true);
                    changeBtn_released(2);
                    changeState(RADIO); webSrv.send("StateRadio");break;
        case 73:    changeBtn_released(3); break; // unused
        case 74:    _sleeptime = 0;
                    changeBtn_released(4);
                    changeState(RADIO); webSrv.send("StateRadio");break;

        /* BRIGHTNESS ************************************
        case 80:    downBrightness(); changeBtn_released(0); break;
        case 81:    upBrightness();   changeBtn_released(1); break;
        case 82:    changeState(RADIO); webSrv.send("StateRadio");break;

        /* SETTINGS ************************************
        case 90:    if(input == 3){input=1;}else {input++;}  _state=BRIGHTNESS; changeState(SETTINGS);break;
        case 91:    changeState(ALARM); break;
        case 92:    changeState(SLEEP); break;
        case 93:    if(output == 2){output=1;}else {output++;} _state=BRIGHTNESS; changeState(SETTINGS);break;
        case 94:    if(clocktype == 3){clocktype=1;}else {clocktype++;}  _state=BRIGHTNESS; changeState(SETTINGS);break;
        case 95:    changeState(BRIGHTNESS); break;
        case 96:    switch(input){          //change buttons
                        case 1: {changeState(RADIO); break;}   //Radio
                        case 2: {changeState(PLAYER); break;}   //Player_SD
                        case 3: { break;}   //BT_In
                    } ; break;
    }
    _releaseNr = -1;
}
*/
//Events from websrv
void WEBSRV_onCommand(const String cmd, const String param, const String arg){                       // called from html
    //log_i("HTML_cmd=%s params=%s arg=%s", cmd.c_str(),param.c_str(), arg.c_str());
    String  str;
    if(cmd == "homepage"){          webSrv.send("homepage=" + _homepage); return;}
    if(cmd == "to_listen"){         StationsItems(); return;}// via websocket, return the name and number of the current station
     if(cmd == "gettone"){           if(DECODER) webSrv.reply(setI2STone().c_str()); else webSrv.reply(setTone().c_str()); return;}
    if(cmd == "getmute"){           webSrv.reply(String(int(_f_mute)).c_str()); return;}
    if(cmd == "getstreamtitle"){    webSrv.reply(_streamTitle.c_str());return;}
    if(cmd == "mute"){              mute();Serial.print("main.cpp running on core "); Serial.println(xPortGetCoreID());if(_f_mute) webSrv.reply("Mute on\n");else webSrv.reply("Mute off\n");return;}
    if(cmd == "toneha"){            pref.putUShort("toneha",(param.toInt()));                             // vs1053 tone
                                    webSrv.reply("Treble Gain set");
                                    setTone(); return;}
    if(cmd == "tonehf"){            pref.putUShort("tonehf",(param.toInt()));                             // vs1053 tone
                                    webSrv.reply("Treble Freq set");
                                    setTone(); return;}
    if(cmd == "tonela"){            pref.putUShort("tonela",(param.toInt()));                             // vs1053 tone
                                    webSrv.reply("Bass Gain set");
                                    setTone(); return;}
    if(cmd == "tonelf"){            pref.putUShort("tonelf",(param.toInt()));                             // vs1053 tone
                                    webSrv.reply("Bass Freq set");
                                    setTone(); return;}
    if(cmd == "LowPass"){           pref.putShort("toneLP", (param.toInt()));                           // audioI2S tone
                                    char lp[25] = "Lowpass set to "; strcat(lp, param.c_str()); strcat(lp, "dB");
                                    webSrv.reply(lp); setI2STone(); return;}
    if(cmd == "BandPass"){          pref.putShort("toneBP", (param.toInt()));                           // audioI2S tone
                                    char bp[25] = "Bandpass set to "; strcat(bp, param.c_str()); strcat(bp, "dB");
                                    webSrv.reply(bp); setI2STone(); return;}
    if(cmd == "HighPass"){          pref.putShort("toneHP", (param.toInt()));                           // audioI2S tone
                                    char hp[25] = "Highpass set to "; strcat(hp, param.c_str()); strcat(hp, "dB");
                                    webSrv.reply(hp); setI2STone(); return;}
    if(cmd == "audiolist"){         sendAudioList2Web("/audiofiles");                                   // via websocket
                                    return;}
    if(cmd == "audiotrack"){        audiotrack(param.c_str()); webSrv.reply("OK\n"); return;}
    if(cmd == "audiotrackall")      {mp3playall=true; _f_eof=true; next_track_needed(true);  str="Playing track: "; str.concat(_audiotrack.substring(_audiotrack.lastIndexOf("/") + 1)); str.concat("\n");webSrv.reply(str); return;}
    if(cmd == "next_track")         {mp3playall=true; _f_eof=true; next_track_needed(true);  str="Playing track: "; str.concat(_audiotrack.substring(_audiotrack.lastIndexOf("/") + 1)); str.concat("\n");webSrv.reply(str); return;}
    if(cmd == "prev_track")         {mp3playall=true; _f_eof=true; next_track_needed(false);  str="Playing track: "; str.concat(_audiotrack.substring(_audiotrack.lastIndexOf("/") + 1)); str.concat("\n");webSrv.reply(str); return;}
    if(cmd == "audiotracknew")      {webSrv.reply("generating new tracklist...\n"); nbroftracks=0;File root = SD.open("/audiofiles");tracklist(root, 0); return;}//tracklist1(SD, "/mp3files", 0);  return;}
    if(cmd == "shuffle")            {if(shuffle) {shuffle=false; webSrv.reply("Shuffle off\n");}else{shuffle=true; webSrv.reply("Shuffle on\n");} /*changeBtn_released(5);*/return;}
    if(cmd == "getshuffle"){        webSrv.reply(String(int(shuffle)).c_str()); return;}
    if(cmd == "uploadfile"){        _filename = param;  return;}
    if(cmd == "upvolume"){          str = "Volume is now " + (String)upvolume(); webSrv.reply(str.c_str()); SerialPrintfln("%s", str.c_str()); return;}
    if(cmd == "downvolume"){        str = "Volume is now " + (String)downvolume(); webSrv.reply(str.c_str()); SerialPrintfln("%s", str.c_str()); return;}
    if(cmd == "SLVolume"){          if (millis() - previousMillis >= 300){int v =param.toInt();pref.putUInt("volume",v); setVolume(v);previousMillis = millis();} return;}  //200ms between values to reduce traffic
    if(cmd == "getvolume"){         str=String(_cur_volume); str.concat("\n");webSrv.reply(str); SerialPrintfln("%s", str.c_str()); return;}
    if(cmd == "prev_station"){      prevStation(); return;}                                             // via websocket
    if(cmd == "next_station"){      nextStation(); return;}                                             // via websocket
    if(cmd == "set_station"){       setStation(param.toInt()); StationsItems(); return;}                // via websocket
    if(cmd == "stationURL"){        connecttohost(param.c_str());webSrv.reply("OK\n"); return;}
    if(cmd == "getnetworks"){       webSrv.reply(WiFi.SSID().c_str()); return;}
    if(cmd == "ping"){              webSrv.send("pong"); return;}
    if(cmd == "index.html"){        webSrv.show(index_html); return;}
    if(cmd == "get_tftSize"){       webSrv.send(_tftSize? "tftSize=m": "tftSize=s"); return;}
    if(cmd == "get_decoder"){       webSrv.send( DECODER? "decoder=s": "decoder=h"); return;}
    if(cmd == "favicon.ico"){       webSrv.streamfile(SD, "/favicon.ico"); return;}
    if(cmd.startsWith("SD")){       str = cmd.substring(2); webSrv.streamfile(SD, scaleImage(str.c_str())); return;}
    if(cmd == "change_state"){      /*changeState(param.toInt());*/ return;}
    if(cmd == "stop"){              _resumeFilePos = audioStopSong(); webSrv.reply("OK\n"); return;}
    if(cmd == "resumefile"){        if(!_lastconnectedfile) webSrv.reply("nothing to resume\n");
                                    else {audiotrack(_lastconnectedfile, _resumeFilePos); webSrv.reply("OK\n");} return;}
    if(cmd == "test"){              sprintf(_chbuf, "free heap: %u, Inbuff filled: %u, Inbuff free: %u\n", ESP.getFreeHeap(), audioInbuffFilled(), audioInbuffFree()); webSrv.reply(_chbuf); return;}

    log_e("unknown HTMLcommand %s", cmd.c_str());
}
void WEBSRV_onRequest(const String request, uint32_t contentLength){
    log_i("request %s contentLength %d", request.c_str(), contentLength);
    if(request.startsWith("------")) return;      // uninteresting WebKitFormBoundaryString
    if(request.indexOf("form-data") > 0) return;  // uninteresting Info
    if(request == "fileUpload"){savefile(_filename.c_str(), contentLength);  return;}
    log_e("unknown request: %s",request.c_str());
}
void WEBSRV_onInfo(const char* info){
    // if(startsWith(info, "WebSocket")) return;       // suppress WebSocket client available
    // if(!strcmp("ping", info)) return;               // suppress ping
    // if(!strcmp("to_listen", info)) return;          // suppress to_isten
    // if(startsWith(info, "Command client"))return;   // suppress Command client available
    SerialPrintfln("HTML_info  : %s", info);    // infos for debug
}