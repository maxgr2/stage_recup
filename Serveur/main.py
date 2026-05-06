import asyncio
import struct
from bleak import BleakScanner

MANUFACTURER_ID = 65535  # 0xFFFF
ESP32_NOM       = ""
TAILLE_STRUCT   = 6 * 4  # 6 floats × 4 octets = 24 octets

def decoder_payload(raw_bytes: bytes) -> dict | None:
    # Les 2 premiers octets sont l'ID fabricant, déjà retirés par Bleak
    if len(raw_bytes) != TAILLE_STRUCT:
        print(f"Taille inattendue : {len(raw_bytes)} octets (attendu {TAILLE_STRUCT})")
        return None

    valeurs = struct.unpack("<6f", raw_bytes)

    return {
        "tensionBus_V":          valeurs[0],
        "courant_A":             valeurs[1],
        "puissance_W":           valeurs[2],
        "tensionShunt_mV":       valeurs[3],
        "temperature_C":         valeurs[4],
        "temperaturebatterie_C": valeurs[5],
    }

def afficher_donnees(data: dict, adresse: str):
    print(f"\n{'─' * 40}")
    print(f"  ESP32 → {adresse}")
    print(f"{'─' * 40}")
    print(f"  Tension bus       : {data['tensionBus_V']:.3f} V")
    print(f"  Courant           : {data['courant_A']:.3f} A")
    print(f"  Puissance         : {data['puissance_W']:.3f} W")
    print(f"  Tension shunt     : {data['tensionShunt_mV']:.3f} mV")
    print(f"  Température       : {data['temperature_C']:.1f} °C")
    print(f"  Temp. batterie    : {data['temperaturebatterie_C']:.1f} °C")
    print(f"{'─' * 40}")

def detection_callback(device, advertisement_data):
    if device.name != ESP32_NOM:
        return

    manufacturer_data = advertisement_data.manufacturer_data

    if MANUFACTURER_ID not in manufacturer_data:
        print(f"[{ESP32_NOM}] Paquet reçu mais sans données fabricant attendues")
        return

    raw_bytes = manufacturer_data[MANUFACTURER_ID]
    donnees   = decoder_payload(raw_bytes)

    if donnees is not None:
        afficher_donnees(donnees, device.address)

async def main():
    print("Démarrage du scan BLE... (Ctrl+C pour arrêter)")

    scanner = BleakScanner(detection_callback)
    await scanner.start()

    try:
        while True:
            await asyncio.sleep(1)  # laisse tourner le callback en arrière-plan
    except KeyboardInterrupt:
        print("\nArrêt demandé.")
    finally:
        await scanner.stop()
        print("Scan arrêté proprement.")

if __name__ == "__main__":
    asyncio.run(main())