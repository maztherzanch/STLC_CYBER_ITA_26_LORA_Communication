/*
 * NODO LoRa - Se conecta a la central y manda latidos periodicos
 * Hardware: BastWAN (Electronic Cats) con modulo RAK4260
 * Placa en Arduino IDE: "Electronic Cats SAMD Boards" -> BastWAN
 * Libreria: "LoRa" de Sandeep Mistry
 *
 * IMPORTANTE: cambia ID_NODO en cada placa (1, 2, 3, ...). No puede repetirse.
 */

#include <SPI.h>
#include <LoRa.h>

#define ID_NODO     1              // <-- UNICO por cada nodo

#define PIN_LED     LED_BUILTIN    // Parpadea buscando, fijo al conectarse

// ---------- Radio (DEBE ser igual que en la central) ----------
#define FRECUENCIA   915E6
#define SYNC_WORD    0x5A
#define ID_RED       0xA7

// ---------- Tiempos ----------
#define INTERVALO_LATIDO_MS  1000
#define INTERVALO_JOIN_MS    3000

#define TIPO_JOIN  1
#define TIPO_BEAT  2
#define TIPO_ACK   3

bool conectado = false;
unsigned long proximoEnvio = 0;

// Switch de antena del BastWAN: HIGH = recibir, LOW = transmitir
void radioRX() { digitalWrite(RFM_SWITCH, HIGH); }
void radioTX() { digitalWrite(RFM_SWITCH, LOW); }

void enviar(uint8_t tipo) {
  radioTX();
  LoRa.beginPacket();
  LoRa.write(ID_RED);
  LoRa.write(ID_NODO);
  LoRa.write(tipo);
  LoRa.endPacket();
  radioRX();
}

// Revisa si llego el ACK de la central dirigido a este nodo
void revisarRecepcion() {
  int tam = LoRa.parsePacket();
  if (!tam) return;
  if (tam < 3) { while (LoRa.available()) LoRa.read(); return; }

  uint8_t red  = LoRa.read();
  uint8_t id   = LoRa.read();
  uint8_t tipo = LoRa.read();
  while (LoRa.available()) LoRa.read();

  if (red == ID_RED && id == ID_NODO && tipo == TIPO_ACK && !conectado) {
    conectado = true;
    digitalWrite(PIN_LED, HIGH);
    Serial.print("Conectado a la central (RSSI ");
    Serial.print(LoRa.packetRssi());
    Serial.println(" dBm)");
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_LED, OUTPUT);
  pinMode(RFM_SWITCH, OUTPUT);
  radioRX();

  LoRa.setPins(SS, RFM_RST, RFM_DIO0);
  if (!LoRa.begin(FRECUENCIA)) {
    while (true) {   // parpadeo rapido = fallo al iniciar el radio
      Serial.println("Error al iniciar LoRa");
      digitalWrite(PIN_LED, !digitalRead(PIN_LED));
      delay(100);
    }
  }
  LoRa.setSyncWord(SYNC_WORD);
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.enableCrc();

  randomSeed(analogRead(A0) + ID_NODO * 97);
  Serial.println("Nodo buscando central...");
}

void loop() {
  revisarRecepcion();

  if (millis() >= proximoEnvio) {
    if (conectado) {
      enviar(TIPO_BEAT);
      // Pequena variacion aleatoria para que los nodos no choquen entre si
      proximoEnvio = millis() + INTERVALO_LATIDO_MS + random(0, 200);
    } else {
      enviar(TIPO_JOIN);
      digitalWrite(PIN_LED, !digitalRead(PIN_LED));   // parpadeo = buscando
      proximoEnvio = millis() + INTERVALO_JOIN_MS + random(0, 500);
    }
  }
}
