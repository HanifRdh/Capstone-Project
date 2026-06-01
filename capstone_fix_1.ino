// Konfigurasi identitas blynk
#define BLYNK_TEMPLATE_ID   "TMPL6P28Se8RG"
#define BLYNK_TEMPLATE_NAME "Monitoring Gas LPG"
#define BLYNK_AUTH_TOKEN    "zFxLNSe7uFqAwEBYfx9rp99u7I6fa7_1"

// Penyertaan library yang akan digunakan
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <math.h>
#include "HX711.h"

// Konfigurasi jaringan Wi-Fi
char ssid[] = "hayo siapa";
char pass[] = "setanyahu";

// Konfigurasi pin GPIO pada ESP32
#define MQ6_PIN       35  
#define BUZZER_PIN    25  
#define BATT_PIN      34  
#define LED_TERISI    26  
#define LED_LEMAH     27  
#define LOADCELL_DOUT_PIN 32   
#define LOADCELL_SCK_PIN  33   

// Inisialisasi parameter konstanta
const float RL = 20.0;         
const float R0 = 15.0;         
const float THRESHOLD_PPM = 1000.0; 
const float CALIBRATION_FACTOR = 228000.0; 

// Inisialisasi metadata dan macAddress
const String ID_TABUNG = "LPG-3KG-001"; 
String macAddress = "";

// Inisialisasi objek
HX711 scale;                   
BlynkTimer timer;              

// Fungsi utama
void sendSensorData() {
  
  // Proses 1 oversampling dan filtering sensor MQ6
  long adcGasSum = 0;          
  for (int i = 0; i < 50; i++) { 
    adcGasSum += analogRead(MQ6_PIN); 
    delay(2);                  
  }
  float rataRataGas = (float)adcGasSum / 50.0; 

  // Proses 2 konversi nilai ADC MQ6 ke PPM
  float vRL = (rataRataGas / 4095.0) * 3.3; 
  if (vRL < 0.1) vRL = 0.1;    
  float rS = ((3.3 - vRL) / vRL) * RL; 
  float rasio = rS / R0;       
  float gasPPM = 1000.0 * pow(rasio, -2.25); 

  // Proses 3 akuisisi dan kalkulasi berat (load cell)
  float beratKg = scale.get_units(10); 
  if (beratKg < 0.0) beratKg = 0.0; 

  // Proses 4 oversampling dan filtering tegangan baterai
  long adcBattSum = 0;         
  for (int i = 0; i < 50; i++) { 
    adcBattSum += analogRead(BATT_PIN); 
    delay(2);                  
  }
  float rataRataBatt = (float)adcBattSum / 50.0; 
  
  float vPin = (rataRataBatt / 4095.0) * 3.3; 
  float vBaterai = vPin * 2.0; 
  float kalkulasiPersen = ((vBaterai - 3.0) / (4.2 - 3.0)) * 100.0;

  if (kalkulasiPersen > 100.0) kalkulasiPersen = 100.0;
  if (kalkulasiPersen < 0.0) kalkulasiPersen = 0.0;

  // Proses 5 pengelompokan persentase banterai
  int persentaseBatt = 0;       
  int rawPersen = (int)kalkulasiPersen; 

  if (rawPersen == 0) {
    persentaseBatt = 0;        
  } else if (rawPersen > 0 && rawPersen <= 10) {
    persentaseBatt = 10;       
  } else if (rawPersen > 10 && rawPersen <= 20) {
    persentaseBatt = 20;       
  } else if (rawPersen > 20 && rawPersen <= 30) {
    persentaseBatt = 30;       
  } else if (rawPersen > 30 && rawPersen <= 40) {
    persentaseBatt = 40;       
  } else if (rawPersen > 40 && rawPersen <= 50) {
    persentaseBatt = 50;       
  } else if (rawPersen > 50 && rawPersen <= 60) {
    persentaseBatt = 60;       
  } else if (rawPersen > 60 && rawPersen <= 70) {
    persentaseBatt = 70;       
  } else if (rawPersen > 70 && rawPersen <= 80) {
    persentaseBatt = 80;       
  } else if (rawPersen > 80 && rawPersen <= 90) {
    persentaseBatt = 90;       
  } else if (rawPersen > 90 && rawPersen <= 100) {
    persentaseBatt = 100;      
  }

  // Proses 6 logika aksi buzzer dan indikator LED
  if (gasPPM > THRESHOLD_PPM) { 
    digitalWrite(BUZZER_PIN, LOW);  
    Blynk.virtualWrite(V3, 255);    
    Serial.print("[⚠️ BAHAYA: GAS BOCOR!]: "); Serial.print(gasPPM, 0); Serial.println(" PPM"); 
  } else {                     
    digitalWrite(BUZZER_PIN, HIGH); 
    Blynk.virtualWrite(V3, 0);      
  }

  if (persentaseBatt <= 20.0) { 
    digitalWrite(LED_LEMAH, HIGH);  
    digitalWrite(LED_TERISI, LOW);  
    Blynk.virtualWrite(V4, 0);      
    Blynk.virtualWrite(V5, 255);    
  } 
  else {                       
    digitalWrite(LED_LEMAH, LOW);   
    digitalWrite(LED_TERISI, HIGH); 
    Blynk.virtualWrite(V4, 255);    
    Blynk.virtualWrite(V5, 0);      
  }

  // Proses 7 debug data (cetak di serial monitor
  Serial.print("[" + macAddress + " | " + ID_TABUNG + "] ");
  Serial.print("Gas: "); Serial.print(gasPPM, 0); Serial.print(" PPM (ADC: "); Serial.print(rataRataGas, 0); 
  Serial.print(") | Berat Tabung: "); Serial.print(beratKg, 2); Serial.print(" Kg"); 
  Serial.print(" | Batt Volt: "); Serial.print(vBaterai, 2); 
  Serial.print(" V ("); Serial.print(persentaseBatt); Serial.println("%)"); 

  // Proses 8 transmisi data ke blynk melalui virtual pin
  Blynk.virtualWrite(V1, gasPPM);          
  Blynk.virtualWrite(V2, persentaseBatt);  
  Blynk.virtualWrite(V6, beratKg);         
}

// Fungsi inisialisasi kondisi awal sistem
void setup() {
  Serial.begin(115200);        
  Serial.println("\n=== MEMULAI SISTEM IOT BLYNK (MODE TOTAL SENSOR) ==="); 
  
  // konfigurasi fungsi masing-masing pin GPIO ESP32
  pinMode(MQ6_PIN, INPUT);     
  pinMode(BATT_PIN, INPUT);    
  pinMode(BUZZER_PIN, OUTPUT);   
  pinMode(LED_TERISI, OUTPUT);   
  pinMode(LED_LEMAH, OUTPUT);    

  // Kondisi awal LED dan buzzer
  digitalWrite(BUZZER_PIN, HIGH); 
  digitalWrite(LED_TERISI, LOW);  
  digitalWrite(LED_LEMAH, LOW);   

  // inisialisasi dan kalibrasi load cell
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN); 
  scale.set_scale(CALIBRATION_FACTOR); 
  scale.tare();                

  // Pengaturan sistem non-blocking
  Serial.print("Mencoba menyambungkan ke Wi-Fi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);      

  // Mengambil alamat chip Wi-Fi
  macAddress = WiFi.macAddress();

  Blynk.config(BLYNK_AUTH_TOKEN); 
  timer.setInterval(2000L, sendSensorData); 
}

// Fungsi looping sistem
void loop() {
  if (WiFi.status() == WL_CONNECTED) { 
    Blynk.run();               
  }                            
  timer.run();                 
}