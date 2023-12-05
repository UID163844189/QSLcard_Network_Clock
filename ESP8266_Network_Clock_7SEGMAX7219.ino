/**********************************************************************
 * 项目：8位数码管显示时间的NTP时钟
 * 硬件：适用于NodeMCU ESP8266 + MAX7219
 * 功能：连接WiFi后获取时间并在8位数码管上显示
 * 作者：flyAkari 会飞的阿卡林 bilibili UID:751219
 * 日期：2019/04/23
 **********************************************************************/
#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <SPI.h>
#include <EEPROM.h>

String ssid = "OnePlus Ace Pro Genshin Version"; // WiFi名
String pass = "password";						// WiFi密码
String ntpServerName = "ntp.aliyun.com";			// NTP服务器，上海交通大学
int timeZone = 8;								// 时区，北京时间为+8
// char config[128] = "CU_Ve4J;123456788;ntp.aliyun.com;8;"; // max 127
char config[128] = "OnePlus Ace Pro Genshin Version;password;ntp.aliyun.com;8;"; // max 127
// config format: SSID;password;ntp server;timezone;

WiFiUDP Udp;
unsigned int localPort = 8888; // 用于侦听UDP数据包的本地端口
const int slaveSelect = 5;
const int scanLimit = 7;

time_t getNtpTime();
void sendNTPpacket(IPAddress &address);
void digitalClockDisplay();
void sendCommand(int command, int value);
void initdisplay();

void setup()
{
    Serial.begin(9600);
    while (!Serial)
        continue;
    Serial.println("NTP Clock version v1.0");

    EEPROM.begin(128);
    loadConfig();
    applyConfig();

    SPI.begin();
    pinMode(slaveSelect, OUTPUT);
    digitalWrite(slaveSelect, LOW);
    sendCommand(12, 1);			// Shutdown,open
    sendCommand(15, 0);			// DisplayTest,no
    sendCommand(10, 5);		// Intensity,15(max)
    sendCommand(11, scanLimit); // ScanLimit,8-1=7
    sendCommand(9, 255);		// DecodeMode,Code B decode for digits 7-0
    digitalWrite(slaveSelect, HIGH);
    initdisplay();
    Serial.println("LED Ready");

    Serial.print("Connecting WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);
    int i = 0;
    for (int i = 0; i < 20; i++)
    // while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
        if (WiFi.status() == WL_CONNECTED)
            goto connected;
    }
    Serial.println("WiFi connect failed. ");
    for (;;)
        if (Serial.available() > 0)
            SerialEvent();

connected:
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println("Starting UDP");
    Udp.begin(localPort);
    Serial.print("Local port: ");
    Serial.println(Udp.localPort());
    Serial.println("waiting for sync");
    setSyncProvider(getNtpTime);
    setSyncInterval(300); // 每300秒同步一次时间
}

time_t prevDisplay = 0; // 当时钟已经显示

void loop()
{
    if (timeStatus() != timeNotSet)
    {
        if (now() != prevDisplay)
        { // 时间改变时更新显示
            prevDisplay = now();
            digitalClockDisplay();
        }
    }
    if (Serial.available() > 0)
        SerialEvent();
}

void sendCommand(int command, int value)
{
    digitalWrite(slaveSelect, LOW);
    SPI.transfer(command);
    SPI.transfer(value);
    digitalWrite(slaveSelect, HIGH);
}

void initdisplay()
{
    sendCommand(8, 0xa);
    sendCommand(7, 0xa);
    sendCommand(6, 0xa);
    sendCommand(5, 0xa);
    sendCommand(4, 0xa);
    sendCommand(3, 0xa);
    sendCommand(2, 0xa);
    sendCommand(1, 0xa);
}

void digitalClockDisplay()
{
    int years, months, days, hours, minutes, seconds, weekdays;
    years = year();
    months = month();
    days = day();
    hours = hour();
    minutes = minute();
    seconds = second();
    weekdays = weekday();
    Serial.printf("%d/%d/%d %d:%d:%d Weekday:%d\n", years, months, days, hours, minutes, seconds, weekdays);
    Serial.println();

    // sendCommand(8, hours / 10);
    // sendCommand(7, hours % 10);
    // sendCommand(6, 0xa);
    // sendCommand(5, minutes / 10);
    // sendCommand(4, minutes % 10);
    // sendCommand(3, 0xa);
    // sendCommand(2, seconds / 10);
    // sendCommand(1, seconds % 10);

    sendCommand(8, months / 10);
    sendCommand(7, (months % 10) | 0x80);
    sendCommand(6, days / 10);
    sendCommand(5, days % 10);

    sendCommand(4, hours / 10);
    // sendCommand(3, hours % 10 + 128);
    sendCommand(3, (hours % 10) | 0x80);
    sendCommand(2, minutes / 10);
    sendCommand(1, minutes % 10);

    delay(500);
    sendCommand(3, hours % 10);
}

void SerialEvent()
{
    String command;
    while (Serial.available() > 0)
    {
        command += char(Serial.read());
        delay(10); // 延时函数用于等待字符完全进入缓冲区，可以尝试没有延时，输出结果会是什么
    }
    Serial.print(">");
    Serial.println(command);
    if (command == "load")
        loadConfig();
    if (command == "apply")
        applyConfig();
    else if (command == "edit")
        editConfig();
    else if (command == "help")
    {
        const String helpText = "Available commands: help edit load apply\r\nSSID;password;ntp server;timezone;";
        Serial.println(helpText);
    }
}

void loadConfig()
{
    EEPROM.get(0, config);
    Serial.print("read config: ");
    Serial.println(config);
    return;
}

void editConfig()
{
    Serial.println("pls. send config");
    // config = "";
    while (!Serial.available() > 0)
        continue;
    for (int i = 0; i < 127; i++)
    {
        if (Serial.available() > 0)
            config[i] = char(Serial.read());
        else
            config[i] = 0x00;
        delay(10);
    }

    if (config == "exit")
        return;
    Serial.print("edit config: ");
    Serial.println(config);

    EEPROM.put(0, config);
    EEPROM.commit();
}

void applyConfig()
{
    String templist;
    String temp(config);

    // const char *ssid = temp.substring(0, temp.indexOf(";")).data();
    // strcpy(ssid, temp.substring(0, temp.indexOf(";")).c_str());
    ssid = temp.substring(0, temp.indexOf(";"));
    Serial.print("SSID: ");
    Serial.println(ssid);
    temp = temp.substring(temp.indexOf(";") + 1);

    // pass = (char *)temp.substring(0, temp.indexOf(";")).c_str();
    pass = temp.substring(0, temp.indexOf(";"));
    Serial.print("Passowrd: ");
    Serial.println(pass);
    temp = temp.substring(temp.indexOf(";") + 1);

    ntpServerName = temp.substring(0, temp.indexOf(";"));
    Serial.print("NTP Server: ");
    Serial.println(ntpServerName);
    temp = temp.substring(temp.indexOf(";") + 1);

    timeZone = atoi(temp.substring(0, temp.indexOf(";")).c_str());
    Serial.print("TZ: ");
    Serial.println(timeZone);
    temp = temp.substring(temp.indexOf(";") + 1);

    // Serial.println(temp);
}

#pragma region -------- NTP 代码 ----------

const int NTP_PACKET_SIZE = 48;		// NTP时间在消息的前48个字节里
byte packetBuffer[NTP_PACKET_SIZE]; // 输入输出包的缓冲区

time_t getNtpTime()
{
    IPAddress ntpServerIP; // NTP服务器的地址

    while (Udp.parsePacket() > 0)
        ; // 丢弃以前接收的任何数据包
    Serial.println("Transmit NTP Request");
    // 从池中获取随机服务器
    WiFi.hostByName(ntpServerName.c_str(), ntpServerIP);
    Serial.print(ntpServerName);
    Serial.print(": ");
    Serial.println(ntpServerIP);
    sendNTPpacket(ntpServerIP);
    uint32_t beginWait = millis();
    while (millis() - beginWait < 1500)
    {
        int size = Udp.parsePacket();
        if (size >= NTP_PACKET_SIZE)
        {
            Serial.println("Receive NTP Response");
            Udp.read(packetBuffer, NTP_PACKET_SIZE); // 将数据包读取到缓冲区
            unsigned long secsSince1900;
            // 将从位置40开始的四个字节转换为长整型，只取前32位整数部分
            secsSince1900 = (unsigned long)packetBuffer[40] << 24;
            secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
            secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
            secsSince1900 |= (unsigned long)packetBuffer[43];
            Serial.println(secsSince1900);
            Serial.println(secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR);
            return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
        }
    }
    Serial.println("No NTP Response :-("); // 无NTP响应
    return 0;							   // 如果未得到时间则返回0
}

// 向给定地址的时间服务器发送NTP请求
void sendNTPpacket(IPAddress &address)
{
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    packetBuffer[0] = 0b11100011; // LI, Version, Mode
    packetBuffer[1] = 0;		  // Stratum, or type of clock
    packetBuffer[2] = 6;		  // Polling Interval
    packetBuffer[3] = 0xEC;		  // Peer Clock Precision
    // 8 bytes of zero for Root Delay & Root Dispersion
    packetBuffer[12] = 49;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 49;
    packetBuffer[15] = 52;
    Udp.beginPacket(address, 123); // NTP需要使用的UDP端口号为123
    Udp.write(packetBuffer, NTP_PACKET_SIZE);
    Udp.endPacket();
}
#pragma endregion