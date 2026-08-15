import asyncio
import struct
import sqlite3
from datetime import datetime
from bleak import BleakScanner

# ------ Configuration ----------------------------------------------------------------------------------------------------------------------
MANUFACTURER_ID = 65535
ESP32_NOM       = "Esp_batterie"
# [chip_id: 4] [num_batterie: 1] [7 valeurs int16: 14] = 19 octets de données fabricant
TAILLE_STRUCT   = 4 + 1 + (7 * 2)
FICHIER_DB      = "batteries.db"

# ------ Base de données ------------------------------------------------------------------------------------------------------------------
def init_db(conn: sqlite3.Connection):
    """Crée les tables et assure la présence des colonnes du schéma courant."""
    conn.executescript("""
        CREATE TABLE IF NOT EXISTS batteries (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            chip_id       TEXT NOT NULL,
            num_batterie  INTEGER NOT NULL,
            premiere_vue  TEXT NOT NULL,
            UNIQUE(chip_id, num_batterie)
        );

        CREATE TABLE IF NOT EXISTS mesures (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            chip_id TEXT NOT NULL,
            num_batterie INTEGER NOT NULL,
            timestamp TEXT NOT NULL,
            tensionBus_V REAL,
            courant_A REAL,
            impedance_ohm REAL,  
            impedance_deg REAL,
            temperature_C REAL,
            temperaturebatterie_C REAL,
            tensionBus_charge_V REAL,
            FOREIGN KEY(chip_id, num_batterie) REFERENCES batteries(chip_id, num_batterie)
        );

        CREATE INDEX IF NOT EXISTS idx_mesures_batterie
            ON mesures(chip_id, num_batterie);
        CREATE INDEX IF NOT EXISTS idx_mesures_timestamp
            ON mesures(timestamp);
    """)

    # Migration non destructive des bases créées avec l'ancien schéma.
    colonnes = {row[1] for row in conn.execute("PRAGMA table_info(mesures)")}
    if "impedance_imag_ohm" not in colonnes:
        conn.execute("ALTER TABLE mesures ADD COLUMN impedance_imag_ohm REAL")
    conn.commit()

def enregistrer_batterie(conn: sqlite3.Connection, chip_id: str, num_batterie: int, timestamp: str):
    """Enregistre la batterie si elle n'existe pas encore."""
    conn.execute("""
        INSERT OR IGNORE INTO batteries (chip_id, num_batterie, premiere_vue)
        VALUES (?, ?, ?)
    """, (chip_id, num_batterie, timestamp))
    conn.commit()

def enregistrer_mesure(conn: sqlite3.Connection, donnees: dict):
    """Enregistre une mesure dans la base de données."""
    conn.execute("""
        INSERT INTO mesures
            (chip_id, num_batterie, timestamp,
             tensionBus_V, courant_A, impedance_ohm, impedance_deg,
             temperature_C, temperaturebatterie_C, tensionBus_charge_V)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    """, (
        donnees["chip_id"],
        donnees["num_batterie"],
        donnees["timestamp"],
        donnees["tensionBus_V"],
        donnees["courant_A"],
        donnees["impedance_ohm"],  # ← Partie réelle en ohms
        donnees["impedance_deg"],  # ← Phase en degrés
        donnees["temperature_C"],
        donnees["temperaturebatterie_C"],
        donnees["tensionBus_charge_V"],
    ))
    conn.commit()

def compter_mesures(conn: sqlite3.Connection, chip_id: str, num_batterie: int) -> int:
    """Retourne le nombre de mesures enregistrées pour une batterie."""
    cur = conn.execute("""
        SELECT COUNT(*) FROM mesures
        WHERE chip_id = ? AND num_batterie = ?
    """, (chip_id, num_batterie))
    return cur.fetchone()[0]

# ---- Décodage BLE ------------------------------------------------------------------------------------------------------------------------
def decoder_payload(raw_bytes: bytes) -> dict | None:
    """Décode les paquets reçus et les remet sous la bonne unité."""
    if len(raw_bytes) != TAILLE_STRUCT:
        print(f"Taille inattendue : {len(raw_bytes)} octets (attendu {TAILLE_STRUCT})")
        return None

    chip_id      = struct.unpack_from("<I", raw_bytes, 0)[0]
    num_batterie = raw_bytes[4]
    bruts        = struct.unpack_from("<7h", raw_bytes, 5)  # 7 valeurs

    return {
        "chip_id":               f"{chip_id:08X}",
        "num_batterie":          num_batterie,
        "tensionBus_V":          bruts[0] / 100.0,      # 0.01 V
        "courant_A":             bruts[1] / 1000.0,     # 0.001 A
        "impedance_ohm":         bruts[2] / 100.0,      # 0.01 Ω (partie réelle)
        "impedance_deg":         bruts[3] / 100.0,      # 0.01° (phase en degrés)
        "temperature_C":         bruts[4] / 10.0,       # 0.1°C
        "temperaturebatterie_C": bruts[5] / 10.0,       # 0.1°C
        "tensionBus_charge_V":   bruts[6] / 100.0,      # 0.01 V
        "timestamp":             datetime.now().isoformat(),
    }

# ------ Affichage ------------------------------------------------------------------------------------------------------------------------------
def afficher_donnees(donnees: dict, nb_mesures: int):
    """Affichage des données dans le terminal."""
    clé = f"{donnees['chip_id']}_{donnees['num_batterie']}"
    print(f"\n{'--' * 45}")
    print(f"  Batterie     → {clé}  ({nb_mesures}/500 mesures)")
    print(f"  Horodatage   → {donnees['timestamp']}")
    print(f"{'--' * 45}")
    print(f"  Tension bus    : {donnees['tensionBus_V']:.2f} V")
    print(f"  Courant        : {donnees['courant_A']:.3f} A")
    print(f"  Impédance (R)  : {donnees['impedance_ohm']:.2f} Ω")
    print(f"  Phase (θ)      : {donnees['impedance_deg']:.2f}°")
    print(f"  Température    : {donnees['temperature_C']:.1f} °C")
    print(f"  Temp. batterie : {donnees['temperaturebatterie_C']:.1f} °C")
    print(f"  Tension charge : {donnees['tensionBus_charge_V']:.2f} V")
    print(f"{'--' * 45}")

# ------ Callback BLE ------------------------------------------------------------------------------------------------------------------------
def make_callback(conn: sqlite3.Connection):
    """Fonction de callback BLE partageant la connexion SQLite."""
    def detection_callback(device, advertisement_data):
        if device.name != ESP32_NOM:
            return
        manufacturer_data = advertisement_data.manufacturer_data
        if MANUFACTURER_ID not in manufacturer_data:
            return
        donnees = decoder_payload(manufacturer_data[MANUFACTURER_ID])
        if donnees is None:
            return
        enregistrer_batterie(conn, donnees["chip_id"], donnees["num_batterie"], donnees["timestamp"])
        enregistrer_mesure(conn, donnees)
        nb = compter_mesures(conn, donnees["chip_id"], donnees["num_batterie"])
        afficher_donnees(donnees, nb)
    return detection_callback

# ------ Main ----------------------------------------------------------------------------------------------------------------------------------------
async def main():
    conn = sqlite3.connect(FICHIER_DB, check_same_thread=False)
    init_db(conn)
    print(f"Base de données : {FICHIER_DB}")
    print("Démarrage du scan BLE... (Ctrl+C pour arrêter)")
    scanner = BleakScanner(make_callback(conn))
    await scanner.start()
    try:
        while True:
            await asyncio.sleep(1)
    except KeyboardInterrupt:
        print("\nArrêt demandé.")
    finally:
        await scanner.stop()
        conn.close()
        print("Connexion base de données fermée.")

if __name__ == "__main__":
    asyncio.run(main())
