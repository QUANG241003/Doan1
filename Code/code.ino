#include <FirebaseESP32.h>
#include <DHT.h>
#include <WiFi.h>
#include <PID_v1.h>
#include <RtcDS1302.h>

// Firebase credentials and database URL
#define FIREBASE_HOST "temperature-12-9-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "BbHL90qlnMhLLGeHxcHSHQq6BsbPwlNNTF15xKCc"

// WiFi credentials
#define WIFI_SSID "ARVR Lab" // Your WiFi SSID
#define WIFI_PASSWORD "ARVR@123456" // Your WiFi Password

// Pin definitions
#define DHTPIN 15 // DHT11 data pin
#define LED_PIN 2
#define LED_CHANNEL 0
#define RESOLUTION 8
#define FREQUENCY 5000
#define DHTTYPE DHT11

// DS1302 connections
#define DAT_PIN 18 // Data pin
#define CLK_PIN 5  // Clock pin
#define RST_PIN 19 // Reset pin

// PID parameters
double Setpoint = 75.0; // Initial setpoint
double Time = 1;        // Time in minutes
double Input;           // Current temperature sensor value
double Output;          // PID output to control the LED
double Kp = 58.5, Ki = 0.75, Kd = 1.0;
PID myPID(&Input, &Output, &Setpoint, Kp, Ki, Kd, DIRECT);

// DHT sensor
DHT dht(DHTPIN, DHTTYPE);

// RTC variables
ThreeWire myWire(DAT_PIN, CLK_PIN, RST_PIN); // IO, SCLK, CE
RtcDS1302<ThreeWire> Rtc(myWire);

FirebaseData fbdo;

unsigned long startMillis;
bool rtcHalted = false;
bool runPid = false; // Initial state of PID control
bool permit = false; // Permit for PID control
bool firstSetpointPrint = true; // Flag to print setpoint only once
float lastTemperature = 0.0; // Last temperature reading

void setup()
{
    Serial.begin(115200);
    delay(1000);

    // Connect to WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(500);
    }
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    // Initialize DHT sensor
    dht.begin();

    // Initialize PID control
    myPID.SetMode(AUTOMATIC);
    myPID.SetTunings(Kp, Ki, Kd);

    // Initialize LED PWM
    ledcSetup(LED_CHANNEL, FREQUENCY, RESOLUTION);
    ledcAttachPin(LED_PIN, LED_CHANNEL);

    // Initialize RTC
    Rtc.Begin();
    RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);

    Serial.print("Compiled: ");
    Serial.print(__DATE__);
    Serial.print(" ");
    Serial.println(__TIME__);

    printDateTime(compiled);
    Serial.println();

    if (!Rtc.IsDateTimeValid())
    {
        Serial.println("RTC lost confidence in the DateTime!");
        Rtc.SetDateTime(RtcDateTime(2024, 1, 1, 0, 0, 0));
    }

    if (Rtc.GetIsWriteProtected())
    {
        Serial.println("RTC was write protected, enabling writing now");
        Rtc.SetIsWriteProtected(false);
    }

    if (!Rtc.GetIsRunning())
    {
        Serial.println("RTC was not actively running, starting now");
        Rtc.SetIsRunning(true);
    }

    RtcDateTime now = Rtc.GetDateTime();
    if (now < compiled)
    {
        Serial.println("RTC is older than compile time! Updating DateTime");
        Rtc.SetDateTime(compiled);
    }
    else if (now > compiled)
    {
        Serial.println("RTC is newer than compile time. This is expected.");
    }
    else if (now == compiled)
    {
        Serial.println("RTC is the same as compile time! Not expected but all is fine.");
    }

    // Save the starting time in milliseconds
    startMillis = millis();

    // Initialize Firebase
    Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
    Firebase.reconnectWiFi(true);
}

// Function to convert string to double
double stringToDouble(String value)
{
    return value.toDouble();
}

// Function to convert string to integer
int stringToInt(String value)
{
    return value.toInt();
}

void loop()
{
    // Read temperature and humidity from DHT sensor
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (isnan(h) || isnan(t))
    {
        Serial.println("Failed to read from DHT sensor!");
        return;
    }

    // Print temperature only if it has changed
    if (t != lastTemperature)
    {
        lastTemperature = t;
        Serial.print("Temperature: ");
        Serial.print(t);
        Serial.println(" *C");
        Serial.print("Setpoint: ");
        Serial.print(Setpoint);
        Serial.print(" *C, PID Output: ");
        Serial.println(Output);
    }

    // Check the permit from Firebase
    if (Firebase.getInt(fbdo, "/permit"))
    {
        bool newPermit = fbdo.intData();
        if (newPermit != permit)
        {
            permit = newPermit;
            if (permit)
            {
                Serial.println("Permit granted. Starting PID control.");
            }
            else
            {
                Serial.println("Permit not granted. Stopping PID control.");
            }
        }
    }
    else
    {
        Serial.print("Failed to get permit data: ");
        Serial.println(fbdo.errorReason());
        permit = false;
    }

    // Run PID control only if permit is true
    if (permit && !runPid)
    {
        // Read Setpoint and Time from Firebase
        if (Firebase.getString(fbdo, "/Setpoint"))
        {
            String setpointStr = fbdo.stringData();
            Setpoint = stringToDouble(setpointStr);
            if (firstSetpointPrint)
            {
                Serial.print("Setpoint updated from Firebase: ");
                Serial.println(Setpoint);
                firstSetpointPrint = false; // Set flag to false after first print
            }
        }
        else
        {
            Serial.print("Failed to get Setpoint data: ");
            Serial.println(fbdo.errorReason());
        }

        if (Firebase.getString(fbdo, "/Time"))
        {
            String timeStr = fbdo.stringData();
            Time = stringToDouble(timeStr);
            Serial.print("Time updated from Firebase: ");
            Serial.println(Time);
        }
        else
        {
            Serial.print("Failed to get Time data: ");
            Serial.println(fbdo.errorReason());
        }

        runPid = true; // Start PID control
        startMillis = millis(); // Reset start time
    }

    // Run PID control for the specified time
    if (runPid && (millis() - startMillis) < Time * 60000)
    {
        Input = t;
        myPID.Compute();                          // Compute the PID output
        ledcWrite(LED_CHANNEL, Output);           // Use the PID output to control the LED brightness

        

        // Send temperature to Firebase
        if (Firebase.setFloat(fbdo, "/temperature", t))
        {
            Serial.println("Temperature data sent to Firebase");
        }
        else
        {
            Serial.print("Failed to send temperature data: ");
            Serial.println(fbdo.errorReason());
        }

        delay(1000); // 1 second delay
    }
    else
    {
        // After the specified time, stop PID control and halt RTC
        if (!rtcHalted)
        {
            Rtc.SetIsRunning(false); // Halt the RTC
            rtcHalted = true;        // Set the flag to avoid halting again
            Serial.println("RTC halted after specified time.");
        }
        runPid = false; // Stop further PID computation
    }

    // Continuous check of permit
    if (!permit)
    {
        runPid = false; // Stop PID if permit is not granted
    }
}

#define countof(a) (sizeof(a) / sizeof(a[0]))

void printDateTime(const RtcDateTime &dt)
{
    char datestring[26];
    snprintf_P(datestring,
               countof(datestring),
               PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
               dt.Month(),
               dt.Day(),
               dt.Year(),
               dt.Hour(),
               dt.Minute(),
               dt.Second());
    Serial.print(datestring);
}