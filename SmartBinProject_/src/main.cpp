// 🗑️ ESP32 SMART BIN - WiFi + Firebase + LED Sistemi
#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// ============== YAPILANDIRMA ==============
// WiFi Bilgileri
#define WIFI_SSID "FIREBASE_API_KEY_BURAYA"              
#define WIFI_PASSWORD "WIFI_SIFRENIZ_BURAYA"     
// Firebase Bilgileri
#define API_KEY "FIREBASE_API_KEY_BURAYA"
#define DATABASE_URL "https://smartbinproject-45fe5-default-rtdb.firebaseio.com"

// Sensör Pinleri
const int trigPin = 5;
const int echoPin = 18;

// LED Pinleri
const int YESIL_LED_PIN = 19;    // NORMAL durumda (doluluk < 50%)
const int SARI_LED_PIN = 21;     // DOLUYOR durumda (doluluk 50-80%)
const int KIRMIZI_LED_PIN = 4;   // KRİTİK durumda (doluluk > 80%)

// Sistem Ayarları - 30cm Kutu İçin Ayarlandı
#define KUTU_DERINLIGI 30        // Çöp kutusu yüksekliği (cm)
#define GONDERIM_ARALIGI 5000     // 5 saniye
// ==========================================

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sonGonderim = 0;
bool firebaseReady = false;

// Mesafe ölçüm fonksiyonu
long mesafeOlc() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  long sure = pulseIn(echoPin, HIGH, 30000); 
  long mesafe = sure * 0.0343 / 2;
  
  return mesafe;
}

// Durum belirleme - 30cm için optimize edilmiş
String durumBelirle(int mesafe) {
  // 30cm kutu için:
  // 0-15cm mesafe = BOŞ/NORMAL (doluluk < 50%)
  // 15-24cm mesafe = DOLUYOR (doluluk 50-80%)
  // 24-30cm mesafe = KRİTİK (doluluk > 80%)
  
  if (mesafe > 15) {
    return "NORMAL";           // Mesafe > 15cm = Az dolu
  }
  else if (mesafe > 6) {
    return "DOLUYOR";          // Mesafe 6-15cm = Doluyor
  }
  else {
    return "KRITIK_DOLU";      // Mesafe < 6cm = Neredeyse dolu
  }
}

// Doluluk yüzdesini hesapla
int dolulukHesapla(int mesafe) {
  // Mesafe 30cm = %0 dolu (boş)
  // Mesafe 0cm = %100 dolu (tam)
  int doluluk = (int)(((30.0 - mesafe) / 30.0) * 100);
  return constrain(doluluk, 0, 100);
}

// LED kontrolü - Mesafeye göre
void ledKontrol(int mesafe) {
  // Tüm LED'leri kapat
  digitalWrite(YESIL_LED_PIN, LOW);
  digitalWrite(SARI_LED_PIN, LOW);
  digitalWrite(KIRMIZI_LED_PIN, LOW);
  
  // 30cm kutu için LED mantığı:
  if (mesafe > 15) {
    // NORMAL (0-15cm mesafe) - Sadece Yeşil
    digitalWrite(YESIL_LED_PIN, HIGH);
  }
  else if (mesafe > 6) {
    // DOLUYOR (6-15cm mesafe) - Yeşil + Sarı
    
    digitalWrite(SARI_LED_PIN, HIGH);
  }
  else if (mesafe > 0) {
    // KRİTİK (0-6cm mesafe) - Hepsi
   
    digitalWrite(KIRMIZI_LED_PIN, HIGH);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println();
  Serial.println("==========================================");
  Serial.println("🗑️  SMART BIN - 30cm Kutu - WiFi + Firebase");
  Serial.println("==========================================");
  
  // Pin ayarları
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(YESIL_LED_PIN, OUTPUT);
  pinMode(SARI_LED_PIN, OUTPUT);
  pinMode(KIRMIZI_LED_PIN, OUTPUT);
  
  // LED'leri kapat
  digitalWrite(YESIL_LED_PIN, LOW);
  digitalWrite(SARI_LED_PIN, LOW);
  digitalWrite(KIRMIZI_LED_PIN, LOW);
  
  // LED Testi
  Serial.println("💡 LED Testi...");
  digitalWrite(YESIL_LED_PIN, HIGH);
  delay(300);
  digitalWrite(YESIL_LED_PIN, LOW);
  digitalWrite(SARI_LED_PIN, HIGH);
  delay(300);
  digitalWrite(SARI_LED_PIN, LOW);
  digitalWrite(KIRMIZI_LED_PIN, HIGH);
  delay(300);
  digitalWrite(KIRMIZI_LED_PIN, LOW);
  delay(500);
  Serial.println("✅ LED Testi Tamamlandı");
  
  // WiFi Bağlantısı
  Serial.println();
  Serial.print("📡 WiFi'ye bağlanılıyor: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int wifiDeneme = 0;
  while (WiFi.status() != WL_CONNECTED && wifiDeneme < 20) {
    delay(500);
    Serial.print(".");
    wifiDeneme++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("✅ WiFi Bağlantısı Başarılı!");
    Serial.print("📍 IP Adresi: ");
    Serial.println(WiFi.localIP());
    
    // WiFi başarı göstergesi
    for(int i=0; i<3; i++) {
      digitalWrite(YESIL_LED_PIN, HIGH);
      delay(200);
      digitalWrite(YESIL_LED_PIN, LOW);
      delay(200);
    }
  } else {
    Serial.println();
    Serial.println("❌ WiFi Bağlantısı BAŞARISIZ!");
    Serial.println("💡 SSID ve şifre kontrol edin");
    while(1) {
      digitalWrite(KIRMIZI_LED_PIN, HIGH);
      delay(500);
      digitalWrite(KIRMIZI_LED_PIN, LOW);
      delay(500);
    }
  }
  
  // Firebase Konfigürasyonu
  Serial.println();
  Serial.println("🔥 Firebase'e bağlanılıyor...");
  
  //config.api_key = API_KEY;
  //config.database_url = DATABASE_URL;
  
  config.signer.tokens.legacy_token = "GNaDGxp2DbxrnXQqF9X64UTqo2qgbXt3s4lfw3g8";
 config.database_url = DATABASE_URL;
  // Anonymous authentication
 // auth.user.email = "";
  //auth.user.password = "";
  
  Firebase.reconnectWiFi(true);
  Firebase.begin(&config, &auth);
  
  // Token hazır olana kadar bekle
  Serial.print("⏳ Token bekleniyor");
  int tokenDeneme = 0;
  while (!Firebase.ready() && tokenDeneme < 20) {
    Serial.print(".");
    delay(500);
    tokenDeneme++;
  }
  
  if (Firebase.ready()) {
    firebaseReady = true;
    Serial.println();
    Serial.println("✅ Firebase Bağlantısı Başarılı!");
    Serial.println("==========================================");
    Serial.println("📊 SİSTEM HAZIR - 30cm Kutu Aktif");
    Serial.println("🟢 Yeşil: Mesafe > 15cm (Boş)");
    Serial.println("🟡 Sarı: Mesafe 6-15cm (Doluyor)");
    Serial.println("🔴 Kırmızı: Mesafe < 6cm (Kritik)");
    Serial.println("==========================================");
    Serial.println();
    
    // Firebase başarı göstergesi
    for(int i=0; i<2; i++) {
      digitalWrite(YESIL_LED_PIN, HIGH);
      digitalWrite(SARI_LED_PIN, HIGH);
      digitalWrite(KIRMIZI_LED_PIN, HIGH);
      delay(300);
      digitalWrite(YESIL_LED_PIN, LOW);
      digitalWrite(SARI_LED_PIN, LOW);
      digitalWrite(KIRMIZI_LED_PIN, LOW);
      delay(300);
    }
  } else {
    Serial.println();
    Serial.println("❌ Firebase Bağlantısı BAŞARISIZ!");
    Serial.println("💡 API Key ve Database URL kontrol edin");
  }
}

void loop() {
  unsigned long simdikiZaman = millis();
  
  // Mesafe oku
  long mesafe = mesafeOlc();
  
  // Geçerli mesafe kontrolü (0-30cm arası)
  if (mesafe > 0 && mesafe <= 30) {
    // Doluluk hesapla
    int doluluk = dolulukHesapla(mesafe);
    
    // Durumu belirle
    String durum = durumBelirle(mesafe);
    
    // LED'leri kontrol et
    ledKontrol(mesafe);
    
    // Konsola yazdır
    String emoji;
    if (durum == "NORMAL") emoji = "🟢";
    else if (durum == "DOLUYOR") emoji = "🟡";
    else emoji = "🔴";
    
    Serial.print(emoji);
    Serial.print(" Mesafe: ");
    Serial.print(mesafe);
    Serial.print("cm | Doluluk: %");
    Serial.print(doluluk);
    Serial.print(" | ");
    Serial.print(durum);
    
    // Firebase'e gönder
    if (firebaseReady && (simdikiZaman - sonGonderim >= GONDERIM_ARALIGI)) {
      sonGonderim = simdikiZaman;
      
      FirebaseJson json;
      json.set("doluluk", doluluk);
      json.set("mesafe_cm", (int)mesafe);
      json.set("durum", durum);
      json.set("timestamp", (int)(millis() / 1000));
      
      // Live data güncelle
      if (Firebase.RTDB.setJSON(&fbdo, "/cop_kutusu_1/live_data", &json)) {
        Serial.print(" ✅ Firebase");
      } else {
        Serial.print(" ❌ Hata: ");
        Serial.print(fbdo.errorReason());
      }
      
      // Log kaydet (Her 3. gönderimde)
      static int logSayaci = 0;
      logSayaci++;
      if (logSayaci % 3 == 0) {
        String logPath = "/cop_kutusu_1/logs/" + String(millis());
        Firebase.RTDB.setJSON(&fbdo, logPath.c_str(), &json);
      }
    }
    
    Serial.println();
  } else {
    // Geçersiz mesafe (30cm'den uzak veya hata)
    digitalWrite(YESIL_LED_PIN, LOW);
    digitalWrite(SARI_LED_PIN, LOW);
    digitalWrite(KIRMIZI_LED_PIN, LOW);
    Serial.println("⚠️ Mesafe 30cm dışında veya sensör hatası");
  }
  
  delay(1000); // 1 saniye bekle
}
