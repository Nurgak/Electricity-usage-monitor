#include <WiFi.h>
#include <inc/hw_types.h>
#include <inc/hw_timer.h>
#include <driverlib/prcm.h>
#include <driverlib/timer.h>

// Macros
#define LED_OK_ON digitalWrite(GREEN_LED, HIGH)
#define LED_OK_OFF digitalWrite(GREEN_LED, LOW)
#define LED_BUSY_ON digitalWrite(YELLOW_LED, HIGH)
#define LED_BUSY_OFF digitalWrite(YELLOW_LED, LOW)
#define LED_ERROR_ON digitalWrite(RED_LED, HIGH)
#define LED_ERROR_OFF digitalWrite(RED_LED, LOW)

// Input pin is on pin 4 (P03)
#define PIN_INPUT 4
// 15 days worth of backlog data space
#define BACKLOG_SIZE 24*15
// Hour on which the clock is synced again with a NTP server (0-23)
#define CLOCK_SYNC_HOUR 3
// Milliseconds during which a new blink is not registered
#define TIME_DEBOUNCE 200

// Configuration
char ssid[] = "SSID";
char password[] = "password";
char address[] = "script.google.com";
char script[] = "/macros/s/***/exec";
IPAddress timeServer(206,246,122,250); // time.nist.gov NTP server

WiFiClient client;
//WiFiServer server(80);
boolean connected = false;

struct dataPoint
{
  unsigned long time; // UNIX time
  uint16_t power;
};
dataPoint data = {0, 0};

// Backlog data in case of loss of Wi-Fi
dataPoint backlog[BACKLOG_SIZE];
uint16_t backlogCounter = 0;
uint16_t generalPurposeCounter;
unsigned long timeDebounce = 0;

// Buffer for GET request
char buffer[128] = {0};

// Current Unix timestamp
unsigned long currentTime = 0;
uint8_t currentHour = 0;
//uint16_t livePower = 0;
//uint16_t livePowerCommit = 0;
//uint8_t livePowerCounter = 0;

// Connection for time synchronisation
unsigned int localPort = 2390;      // Local port to listen for UDP packets
const int NTP_PACKET_SIZE = 48;     // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[NTP_PACKET_SIZE]; // Buffer to hold incoming and outgoing packets
WiFiUDP Udp;

void setup()
{
  // Initialize serial and wait for port to open
  Serial.begin(115200);
  
  // Status LEDs
  pinMode(GREEN_LED, OUTPUT);  // Blinking LED
  pinMode(YELLOW_LED, OUTPUT); // Busy LED
  pinMode(RED_LED, OUTPUT);    // Problem LED
  
  // Connectring to the Wi-Fi
  wifiConnect();
  
  // Start UDP for time syncronisation
  Udp.begin(localPort);
  
  syncTime();
  
  // Start the web server on port 80
  /*server.begin();
  Serial.println("Webserver started");*/
  
  // Timer setup for clock
  MAP_PRCMPeripheralClkEnable(PRCM_TIMERA0, PRCM_RUN_MODE_CLK);
  MAP_PRCMPeripheralReset(PRCM_TIMERA0);
  MAP_TimerIntRegister(TIMERA0_BASE, TIMER_A, clock);
  MAP_TimerConfigure(TIMERA0_BASE, TIMER_CFG_A_PERIODIC);
  MAP_TimerIntEnable(TIMERA0_BASE, TIMER_TIMA_TIMEOUT);
  MAP_TimerPrescaleSet(TIMERA0_BASE, TIMER_A, 0);
  MAP_TimerLoadSet(TIMERA0_BASE, TIMER_A, F_CPU);
  MAP_TimerEnable(TIMERA0_BASE, TIMER_A);
  
  // Input is on pin 4 (P03)
  pinMode(PIN_INPUT, INPUT);
  attachInterrupt(PIN_INPUT, blinkDetect, RISING );
  
  // First test
  backlog[backlogCounter].time = 1420891954;
  backlog[backlogCounter].power = 1234;
  backlogCounter++;
  
  LED_OK_ON;
}

void loop()
{
  // If there are incoming bytes available from the server
  while(client.available())
  {
    // Dump the data
    char c = client.read();
    //Serial.print(c);
  }
  
  /*if(Serial.available())
  {
    switch(Serial.read())
    {
      case '1':
        syncTime();
        break;
      case '2':
        Serial.print("Current timestamp: ");
        Serial.println(currentTime);
        break;
      case '3':
        Serial.print("Current data: time = ");
        Serial.print(data.time);
        Serial.print(", power = ");
        Serial.println(data.power);
        break;
      case '4':
        if(backlogCounter > 0)
        {
          for(generalPurposeCounter = 0; generalPurposeCounter < backlogCounter; generalPurposeCounter++)
          {
            Serial.print("Backlog #");
            Serial.print(generalPurposeCounter);
            Serial.print(": time = ");
            Serial.print(backlog[generalPurposeCounter].time);
            Serial.print(", power = ");
            Serial.println(backlog[generalPurposeCounter].power);
          }
        }
        else
        {
          Serial.println("No data in the backlog");
        }
        break;
      case '5':
        wifiConnect();
        break;
      default:
        Serial.println("Available actions:\n1: Synchronise time with Network Time Protocol server\n2: Show timestamp\n3: Dump current data\n4: Dump backlog\n5: Reconnect to Wi-Fi");
    }
  }*/
  
  // Check if the hour has changed
  uint8_t hour = (currentTime % 86400L) / 3600;
  //uint8_t minutes = (currentTime % 3600) / 60;
  if(hour != currentHour)
  {
    // Copy counter data to the backlog
    if(backlogCounter < BACKLOG_SIZE - 1)
    {
      backlog[backlogCounter].time = data.time;
      backlog[backlogCounter].power = data.power;
      backlogCounter++;
    }
    
    // Reset data
    data.time = currentTime;
    data.power = 0;
    
    // Save the hour
    currentHour = hour;
    
    // Resync the clock
    if(currentHour == CLOCK_SYNC_HOUR)
    {
      syncTime();
    }
  }
  
  // If there is something in the backlog try to commit it
  if(backlogCounter > 0)
  {
    // Wi-Fi disconnected, try to reconnect
    if(WiFi.status() != WL_CONNECTED)
    {
      wifiConnect();
      // Also sync the time because it might've been a while since the last sync
      syncTime();
    }
    
    // Commit all that is in the backlog
    if(connected)
    {
      commitData();
    }
  }
  
  //localServer();
}

/*void localServer()
{
  WiFiClient client = server.available();
  if(client)
  {
    LED_BUSY_ON;
    // An http request ends with a blank line
    boolean currentLineIsBlank = true;
    while(client.connected())
    {
      if(client.available())
      {
        char c = client.read();
        //Serial.write(c);
        if (c == '\n' && currentLineIsBlank)
        {
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          // Refresh the page automatically every 10 seconds
          client.println("Refresh: 10");
          client.println("Connection: close");
          client.println();
          client.println("<html><head><title>Electricity usage monitor</title></head><body>");
          client.println("<h1>Electricity usage monitor</h1>");
          client.println("<h2>Current time</h2>");
          client.print("Timestamp: ");
          client.println(currentTime);
          client.print("<br />Time: ");
          client.print((currentTime % 86400L) / 3600);
          client.print(":");
          client.print((currentTime % 3600) / 60);
          client.print(":");
          client.println(currentTime % 60);
          client.println("<h2>Current data</h2>");
          client.print("Timestamp: ");
          client.println(data.time);
          client.print("<br />Power: ");
          client.print(data.power);
          client.println("W");
          client.println("<h2>Live electricity consumption</h2>");
          client.print(livePowerCommit);
          client.println("W (last 10 seconds)");
          client.println("<h2>Backlog</h2>");
          uint16_t i;
          if(backlogCounter > 0)
          {
            client.print("<ol>");
            for(generalPurposeCounter = 0; generalPurposeCounter < backlogCounter; generalPurposeCounter++)
            {
              client.print("<li>");
              client.print(generalPurposeCounter);
              client.print("Time = ");
              client.print(backlog[generalPurposeCounter].time);
              client.print(", power = ");
              client.print(backlog[generalPurposeCounter].power);
              client.print("</li>");
            }
            client.print("</ol>");
          }
          else
          {
            client.println("No data in the backlog");
          }
          client.println("</body></html>");
          client.println();
          break;
        }
        if(c == '\n')
        {
          currentLineIsBlank = true;
        }
        else if(c != '\r')
        {
          currentLineIsBlank = false;
        }
      }
    }
    delay(1);
    client.stop();
    LED_BUSY_OFF;
  } 
}*/

// Clock interrupt called every second, one impulse is about 80-100ms
void clock()
{ 
  // Reset interrupt flag
  HWREG(TIMERA0_BASE + TIMER_O_ICR) = 0x1;
  // Increment second
  currentTime++;
  
  // Cound the passed Watts in live over the last 10 seconds
  /*livePowerCounter++;
  if(livePowerCounter == 9)
  {
    livePowerCommit = livePower;
    livePower = 0;
    livePowerCounter = 0;
  }*/
}

// Interrupt routine for registering each blink of the LED
void blinkDetect()
{
  // Debounce correction: assume 2 ticks must be separated by at least 50ms
  // This might have an overflow problem somewhere, someday...
  if(millis() < timeDebounce)
  {
    return;
  }
  
  // Increment power counter
  data.power++;
  
  //livePower++;
  
  // Toggle green LED
  digitalWrite(GREEN_LED, !digitalRead(GREEN_LED));
  
  timeDebounce = millis() + TIME_DEBOUNCE;
}

// Send data to server
void commitData()
{
  LED_BUSY_ON;
  
  Serial.print("Data in queue: ");
  Serial.println(backlogCounter);
  
  while(backlogCounter)
  {
    // Decrement first
    backlogCounter--;
    
    Serial.println("Sending data to server over SSL...");
    if(client.sslConnect(address, 443))
    {
      Serial.print("\tTime: ");
      Serial.println(backlog[backlogCounter].time);
      Serial.print("\tHour: ");
      Serial.println((backlog[backlogCounter].time % 86400L) / 3600);
      Serial.print("\tPower: ");
      Serial.println(backlog[backlogCounter].power);
      
      Serial.println("Connected..."); 
      // Clear the buffer
      memset(buffer, 0, sizeof(buffer));
      // Fill the buffer
      sprintf(buffer, "GET %s?time=%d&power=%d HTTP/1.1", script, backlog[backlogCounter].time, backlog[backlogCounter].power);
      // For debug
      //Serial.println(buffer);
      
      client.println(buffer);
      client.println("Host: script.google.com");
      client.println("Connection: close");
      client.println();
      
      Serial.println("Done");
    }
  }
  
  LED_BUSY_OFF;
}

// (Re)connect to Wi-Fi if disconnected
boolean wifiConnect()
{
  uint8_t retries;
  
  LED_BUSY_ON;
  
  // attempt to connect to Wifi network:
  Serial.print("Attempting to connect to Network named: ");
  // print the network name (SSID);
  Serial.println(ssid); 
  // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
  WiFi.begin(ssid, password);
  
  retries = 10;
  while(WiFi.status() != WL_CONNECTED && retries--)
  {
    // Print dots while we wait to connect
    Serial.print(".");
    delay(300);
  }
  
  Serial.println("OK");
  Serial.println("Waiting for an IP address");
  
  retries = 10;
  while(WiFi.localIP() == INADDR_NONE && retries--)
  {
    // Print dots while we wait for an IP addresss
    Serial.print(".");
    delay(300);
  }
  
  Serial.println("OK");
  
  LED_BUSY_OFF;
  
  // Connection failed of no IP address
  if(WiFi.status() == WL_CONNECTED && !(WiFi.localIP() == INADDR_NONE))
  {
    LED_ERROR_OFF;
    connected = true;
    return true;
  }
  
  LED_ERROR_ON;
  connected = false;
  return false;
}

void syncTime()
{
  LED_BUSY_ON;
  Serial.println("Time synchronisation");
  uint8_t retries;
  for(retries = 0; retries < 10; retries++)
  {
    Serial.print("Try #");
    Serial.println(retries);
      
    sendNTPpacket(timeServer);
    delay(1000);
    if(Udp.parsePacket())
    {
      Udp.read(packetBuffer, NTP_PACKET_SIZE);
      unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
      unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
      unsigned long secsSince1900 = highWord << 16 | lowWord;
      const unsigned long seventyYears = 2208988800UL;
      
      unsigned long oldTime = currentTime;
      currentTime = secsSince1900 - seventyYears;
      currentHour = (currentTime % 86400L) / 3600;
      
      // Update the current data as well
      data.time = currentTime;
      
      Serial.println("Complete");
      Serial.print("\tOld timestamp: ");
      Serial.println(oldTime);
      Serial.print("\tNew timestamp: ");
      Serial.println(currentTime);
      Serial.print("\tDifference: ");
      Serial.println((int32_t) (currentTime - oldTime));
      LED_ERROR_OFF;
      break;
    }
    else
    {
      Serial.println("Could not synchronise the time");
      LED_ERROR_ON;
    }
  }
  LED_BUSY_OFF;
}

// Send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress& address)
{
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0b11100011;
  packetBuffer[1] = 0;
  packetBuffer[2] = 6;
  packetBuffer[3] = 0xEC;
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  
  Udp.beginPacket(address, 123);
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

