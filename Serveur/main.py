import asyncio
import struct
import json
import os
from datetime import datetime
from collections import deque
from bleak import BleakScanner

# ─── Configuration ───────────────────────────────────────────────────────────
MANUFACTURER_ID  = 65535
ESP32_NOM        = "Esp_batterie"
TAILLE_STRUCT    = 4 + 1 + (6 * 2)  # 17 octets
FICHIER_JSON     = "batteries.json"
MAX_RECENTES     = 500
MAX_PREMIERES    = 100

# ─── Stockage en mémoire ─────────────────────────────────────────────────────
# Clé : "CHIPID_NUMBAT" ex: "A1B2C3D4_0"
# Valeur : {
#   "info": {...},
#   "premieres": [...],   # 100 premières mesures reçues
#   "recentes": deque([], 500)  # 500 dernières mesures
# }
batteries = {}

# ─── Décodage ────────────────────────────────────────────────────────────────
def decoder_payload(raw_bytes: bytes) -> dict | None:
    if len(raw_bytes) != TAILLE_STRUCT:
        print(f"Taille inattendue : {len(raw_bytes)} octets (attendu {TAILLE_STRUCT})")
        return None

    chip_id      = struct.unpack_from("<I", raw_bytes, 0)[0]
    num_batterie = raw_bytes[4]
    bruts        = struct.unpack_from("<6h", raw_bytes, 5)

    return {
        "chip_id":               f"{chip_id:08X}",
        "num_batterie":          num_batterie,
        "tensionBus_V":          bruts[0] / 100.0,
        "courant_A":             bruts[1] / 1000.0,
        "puissance_W":           bruts[2] / 10.0,
        "tensionShunt_mV":       bruts[3] / 100.0,
        "temperature_C":         bruts[4] / 10.0,
        "temperaturebatterie_C": bruts[5] / 10.0,
        "timestamp":             datetime.now().isoformat(),
    }

# ─── Gestion des batteries ───────────────────────────────────────────────────
def clé_batterie(chip_id: str, num_batterie: int) -> str:
    return f"{chip_id}_{num_batterie}"

def enregistrer_mesure(donnees: dict):
    clé = clé_batterie(donnees["chip_id"], donnees["num_batterie"])

    # Créer l'entrée si nouvelle batterie
    if clé not in batteries:
        batteries[clé] = {
            "info": {
                "chip_id":      donnees["chip_id"],
                "num_batterie": donnees["num_batterie"],
                "premiere_vue": donnees["timestamp"],
            },
            "premieres": [],        # max 100
            "recentes":  deque(maxlen=MAX_RECENTES),  # max 500
        }
        print(f"  Nouvelle batterie détectée : {clé}")

    bat = batteries[clé]

    # Mesure à sauvegarder (sans chip_id et num_batterie, déjà dans la clé)
    mesure = {
        "timestamp":             donnees["timestamp"],
        "tensionBus_V":          donnees["tensionBus_V"],
        "courant_A":             donnees["courant_A"],
        "puissance_W":           donnees["puissance_W"],
        "tensionShunt_mV":       donnees["tensionShunt_mV"],
        "temperature_C":         donnees["temperature_C"],
        "temperaturebatterie_C": donnees["temperaturebatterie_C"],
    }

    # Sauvegarder dans premières (si pas encore plein)
    if len(bat["premieres"]) < MAX_PREMIERES:
        bat["premieres"].append(mesure)

    # Toujours sauvegarder dans recentes
    bat["recentes"].append(mesure)

# ─── Sauvegarde JSON ─────────────────────────────────────────────────────────
def sauvegarder_json():
    # Convertir les deque en liste pour la sérialisation JSON
    données_json = {}
    for clé, bat in batteries.items():
        données_json[clé] = {
            "info":      bat["info"],
            "premieres": bat["premieres"],
            "recentes":  list(bat["recentes"]),
        }

    with open(FICHIER_JSON, "w", encoding="utf-8") as f:
        json.dump(données_json, f, indent=2, ensure_ascii=False)

def charger_json():
    """Recharge les données existantes au démarrage du programme"""
    if not os.path.exists(FICHIER_JSON):
        return

    with open(FICHIER_JSON, "r", encoding="utf-8") as f:
        données_json = json.load(f)

    for clé, bat in données_json.items():
        batteries[clé] = {
            "info":      bat["info"],
            "premieres": bat["premieres"],
            "recentes":  deque(bat["recentes"], maxlen=MAX_RECENTES),
        }
    print(f"{len(batteries)} batterie(s) chargée(s) depuis {FICHIER_JSON}")

# ─── Affichage ───────────────────────────────────────────────────────────────
def afficher_donnees(donnees: dict):
    clé = clé_batterie(donnees["chip_id"], donnees["num_batterie"])
    bat = batteries[clé]
    print(f"\n{'─' * 45}")
    print(f"  Batterie     → {clé}")
    print(f"  Mesures      → {len(bat['recentes'])} récentes | {len(bat['premieres'])} premières")
    print(f"{'─' * 45}")
    print(f"  Tension bus    : {donnees['tensionBus_V']:.2f} V")
    print(f"  Courant        : {donnees['courant_A']:.3f} A")
    print(f"  Puissance      : {donnees['puissance_W']:.1f} W")
    print(f"  Tension shunt  : {donnees['tensionShunt_mV']:.2f} mV")
    print(f"  Température    : {donnees['temperature_C']:.1f} °C")
    print(f"  Temp. batterie : {donnees['temperaturebatterie_C']:.1f} °C")
    print(f"{'─' * 45}")

# ─── Callback BLE ───────────────────────────────────────────────────────────
def detection_callback(device, advertisement_data):
    if device.name != ESP32_NOM:
        return

    manufacturer_data = advertisement_data.manufacturer_data

    if MANUFACTURER_ID not in manufacturer_data:
        return

    raw_bytes = manufacturer_data[MANUFACTURER_ID]
    donnees   = decoder_payload(raw_bytes)

    if donnees is None:
        return

    enregistrer_mesure(donnees)
    afficher_donnees(donnees)
    sauvegarder_json()  # sauvegarde à chaque mesure reçue

# ─── Main ─────────────────────────────────────────────────────────────────────
async def main():
    charger_json()  # recharge les données existantes au démarrage
    print("Démarrage du scan BLE... (Ctrl+C pour arrêter)")

    scanner = BleakScanner(detection_callback)
    await scanner.start()

    try:
        while True:
            await asyncio.sleep(1)
    except KeyboardInterrupt:
        print("\nArrêt demandé.")
    finally:
        await scanner.stop()
        sauvegarder_json()
        print(f"Données sauvegardées dans {FICHIER_JSON}")

if __name__ == "__main__":
    asyncio.run(main())