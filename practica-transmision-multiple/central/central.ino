/*
 * CENTRAL LoRa - Monitor de nodos con alarma por salida de rango
 * Hardware: BastWAN (Electronic Cats) con modulo RAK4260 (SAMR34 = SAML21 + SX1276)
 * Placa en Arduino IDE: "Electronic Cats SAMD Boards" -> BastWAN
 * Libreria: "LoRa" de Sandeep Mistry
 *
 * Funcionamiento:
 *  - La central escucha. Cuando un nodo manda JOIN, lo registra y le responde ACK.
 *  - Cada nodo registrado manda un latido (BEAT) cada ~2 s.
 *  - Un nodo se considera FUERA DE RANGO si:
 *      a) su senal (RSSI) cae por debajo de RSSI_MINIMO varias veces seguidas, o
 *      b) no se recibe ningun latido durante TIEMPO_PERDIDO_MS.
 *  - Mientras haya al menos un nodo fuera de rango, suena la alarma.
 *
 * Nota: SS, RFM_RST, RFM_DIO0 y RFM_SWITCH ya vienen definidos por la
 * placa BastWAN (el radio es interno al modulo), no hay que cablear nada.
 */

#include <SPI.h>
#include <LoRa.h>

// ---------- Pines ----------
#define PIN_ALARMA  5              // Buzzer activo (o LED externo) en el pin 5
#define PIN_LED     LED_BUILTIN    // LED rojo integrado (pin 13), tambien indica alarma

// ---------- Radio (DEBE ser igual en central y nodos) ----------
#define FRECUENCIA   915E6   // RAK4260(H) = banda alta; 915 MHz para Mexico
#define SYNC_WORD    0x5A    // "Canal privado": ignora otras redes LoRa
#define ID_RED       0xA7    // Identificador de nuestra red dentro del paquete

// ---------- Rango y tiempos (calibra estos valores) ----------
#define RSSI_MINIMO        -90     // dBm. Por debajo de esto = fuera del rango permitido
#define HISTERESIS_DB      3       // Debe recuperar RSSI_MINIMO + 3 para volver a "dentro"
#define SUAVIZADO          0.5f    // 0.1 = lento y estable, 1.0 = sin suavizar (solo al alejarse)
#define LECTURAS_DEBILES   3       // Latidos debiles seguidos para disparar la alarma
#define TIEMPO_PERDIDO_MS  6000UL  // Sin latidos durante este tiempo = nodo perdido
#define MAX_NODOS          10

// ---------- Tipos de paquete ----------
#define TIPO_JOIN  1
#define TIPO_BEAT  2
#define TIPO_ACK   3

struct Nodo {
  bool     activo;
  uint8_t  id;
  unsigned long ultimoLatido;
  float    rssi;          // RSSI suavizado
  uint8_t  debiles;       // Contador de latidos debiles consecutivos
  bool     enAlarma;
};

Nodo nodos[MAX_NODOS];
unsigned long ultimoReporte = 0;

// El BastWAN tiene un switch de antena: HIGH = recibir, LOW = transmitir
void radioRX() { digitalWrite(RFM_SWITCH, HIGH); }
void radioTX() { digitalWrite(RFM_SWITCH, LOW); }

// Busca un nodo por ID. Devuelve su indice o -1.
int buscarNodo(uint8_t id) {
  for (int i = 0; i < MAX_NODOS; i++)
    if (nodos[i].activo && nodos[i].id == id) return i;
  return -1;
}

// Registra un nodo nuevo. Devuelve su indice o -1 si la tabla esta llena.
int registrarNodo(uint8_t id, int rssi) {
  for (int i = 0; i < MAX_NODOS; i++) {
    if (!nodos[i].activo) {
      nodos[i].activo = true;
      nodos[i].id = id;
      nodos[i].ultimoLatido = millis();
      nodos[i].rssi = rssi;
      nodos[i].debiles = 0;
      nodos[i].enAlarma = false;
      Serial.print("[+] Nodo "); Serial.print(id);
      Serial.print(" conectado (RSSI "); Serial.print(rssi); Serial.println(" dBm)");
      return i;
    }
  }
  Serial.println("[!] Tabla de nodos llena");
  return -1;
}

void enviarAck(uint8_t idNodo) {
  radioTX();
  LoRa.beginPacket();
  LoRa.write(ID_RED);
  LoRa.write(idNodo);
  LoRa.write(TIPO_ACK);
  LoRa.endPacket();   // bloqueante
  radioRX();          // volver a escuchar
}

void procesarPaquete(int tam) {
  if (tam < 3) { while (LoRa.available()) LoRa.read(); return; }

  uint8_t red  = LoRa.read();
  uint8_t id   = LoRa.read();
  uint8_t tipo = LoRa.read();
  while (LoRa.available()) LoRa.read();   // descarta el resto
  int rssi = LoRa.packetRssi();

  if (red != ID_RED) return;              // no es de nuestra red
  if (tipo != TIPO_JOIN && tipo != TIPO_BEAT) return;

  int i = buscarNodo(id);
  // Un BEAT de un nodo desconocido tambien lo registra (p. ej. si la central se reinicio)
  if (i < 0) i = registrarNodo(id, rssi);
  if (i < 0) return;
  if (tipo == TIPO_JOIN) enviarAck(id);

  Nodo &n = nodos[i];
  bool estabaPerdido = (millis() - n.ultimoLatido > TIEMPO_PERDIDO_MS);
  n.ultimoLatido = millis();

  // Si la senal mejora (el nodo se acerca) o venia de estar perdido, se toma la
  // lectura nueva de inmediato. Solo se suaviza cuando la senal empeora, para
  // que un bajon momentaneo no dispare la alarma.
  if (estabaPerdido || rssi > n.rssi) n.rssi = rssi;
  else n.rssi = (1.0f - SUAVIZADO) * n.rssi + SUAVIZADO * rssi;

  if (n.rssi < RSSI_MINIMO) {
    if (n.debiles < 255) n.debiles++;
    if (n.debiles >= LECTURAS_DEBILES && !n.enAlarma) {
      n.enAlarma = true;
      Serial.print("[ALARMA] Nodo "); Serial.print(n.id);
      Serial.print(" salio del rango (RSSI "); Serial.print(n.rssi, 0); Serial.println(" dBm)");
    }
  } else if (n.rssi >= RSSI_MINIMO + HISTERESIS_DB) {
    n.debiles = 0;
    if (n.enAlarma) {
      n.enAlarma = false;
      Serial.print("[OK] Nodo "); Serial.print(n.id);
      Serial.print(" regreso al rango (RSSI "); Serial.print(n.rssi, 0); Serial.println(" dBm)");
    }
  }
}

// Revisa nodos que dejaron de transmitir
void revisarTiempos() {
  unsigned long ahora = millis();
  for (int i = 0; i < MAX_NODOS; i++) {
    Nodo &n = nodos[i];
    if (n.activo && !n.enAlarma && ahora - n.ultimoLatido > TIEMPO_PERDIDO_MS) {
      n.enAlarma = true;
      Serial.print("[ALARMA] Nodo "); Serial.print(n.id); Serial.println(" sin senal (perdido)");
    }
  }
}

// Buzzer y LED intermitentes mientras haya algun nodo en alarma
void actualizarAlarma() {
  bool hayAlarma = false;
  for (int i = 0; i < MAX_NODOS; i++)
    if (nodos[i].activo && nodos[i].enAlarma) hayAlarma = true;

  bool estado = hayAlarma && ((millis() / 300) % 2);
  digitalWrite(PIN_ALARMA, estado);
  digitalWrite(PIN_LED, estado);
}

void imprimirEstado() {
  Serial.println("---- Estado de nodos ----");
  bool alguno = false;
  for (int i = 0; i < MAX_NODOS; i++) {
    Nodo &n = nodos[i];
    if (!n.activo) continue;
    alguno = true;
    Serial.print("Nodo "); Serial.print(n.id);
    Serial.print(" | RSSI "); Serial.print(n.rssi, 0);
    Serial.print(" dBm | hace "); Serial.print((millis() - n.ultimoLatido) / 1000);
    Serial.println(n.enAlarma ? " s | FUERA DE RANGO" : " s | en rango");
  }
  if (!alguno) Serial.println("(sin nodos conectados)");
}

void setup() {
  Serial.begin(115200);
  // No se espera a Serial para que funcione tambien sin PC (con bateria)

  pinMode(PIN_ALARMA, OUTPUT);
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

  Serial.println("Central lista. Esperando nodos...");
}

void loop() {
  int tam = LoRa.parsePacket();
  if (tam) procesarPaquete(tam);

  revisarTiempos();
  actualizarAlarma();

  if (millis() - ultimoReporte > 5000) {
    ultimoReporte = millis();
    imprimirEstado();
  }
}
