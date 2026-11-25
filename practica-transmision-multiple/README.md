# Práctica: Transmisión múltiple LoRa

Central que monitorea varios nodos LoRa y suena una alarma cuando alguno sale de rango
(RSSI bajo o sin latidos).

## Hardware
- BastWAN (Electronic Cats) con RAK4260
- Buzzer en pin 5 (central)

## Software
- Arduino IDE + "Electronic Cats SAMD Boards"
- Librería LoRa (Sandeep Mistry)

## Uso
1. Cargar `central/central.ino` en una placa.
2. Cargar `nodo/nodo.ino` en las demás, cambiando `ID_NODO` (1, 2, 3...) en cada una.
3. Abrir el monitor serie a 115200.

## Documentación
Reporte en `docs/`.