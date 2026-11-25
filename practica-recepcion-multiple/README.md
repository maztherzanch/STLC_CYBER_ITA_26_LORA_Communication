# Práctica: Recepción múltiple LoRa

Sistema donde una **central** recibe señales de varios **nodos** LoRa al mismo tiempo,
monitorea si siguen en rango y activa una alarma cuando alguno se aleja o deja de transmitir.

## Estructura

practica-recepcion-multiple/
├── central/
│   └── central.ino     # Recibe a los nodos y controla la alarma
├── nodo/
│   └── nodo.ino        # Se conecta a la central y manda latidos
├── docs/
│   └── Recepcion_multiple_LoRa.pdf
└── README.md

## Hardware
- 1 BastWAN (Electronic Cats, RAK4260) como central
- 1 o más BastWAN como nodos
- Buzzer activo o LED en el **pin 5** de la central

## Software
- Arduino IDE con la placa **"Electronic Cats SAMD Boards" → BastWAN**
- Librería **LoRa** de Sandeep Mistry

## Funcionamiento
1. El nodo manda `JOIN` cada ~3 s hasta que la central le responde con `ACK`.
2. Ya conectado, el nodo manda un latido (`BEAT`) cada ~1 s.
3. La central registra hasta 10 nodos y revisa la intensidad de su señal (RSSI).
4. Un nodo se marca **fuera de rango** si:
   - su RSSI queda por debajo de **-90 dBm** en 3 lecturas seguidas, o
   - pasan **6 s** sin recibir latidos.
5. Mientras haya un nodo fuera de rango, el buzzer y el LED parpadean.

| Parámetro      | Valor     |
|----------------|-----------|
| Frecuencia     | 915 MHz   |
| Spreading Factor | 7       |
| Ancho de banda | 125 kHz   |
| Sync Word      | 0x5A      |

## Uso
1. Cargar `central/central.ino` en la placa central.
2. Cargar `nodo/nodo.ino` en cada nodo, cambiando `ID_NODO` (1, 2, 3...) en cada placa. **No se puede repetir.**
3. Abrir el Monitor Serie a **115200** baudios en la central para ver el estado de los nodos.

## Equipo
- Emiliano(s), Axel, Rita y Fernando

## Documentación
El reporte completo está en `docs/`.