import asyncio
import struct
import sqlite3
from datetime import datetime
from bleak import BleakScanner

# ─── Configuration ───────────────────────────────────────────────────────────
MANUFACTURER_ID = 65535
ESP32_NOM       = "Esp_batterie"
TAILLE_STRUCT   = 4 + 1 + (6 * 2)  # 17 octets
FICHIER_DB      = "batteries.db"

# ─── Base de données ─────────────────────────────────────────────────────────
def init_db(conn: sqlite3.Connection):
    """Crée les tables si elles n'existent pas encore."""
    conn.executescript("""
        CREATE TABLE IF NOT EXISTS batteries (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            chip_id       TEXT NOT NULL,
            num_batterie  INTEGER NOT NULL,
            premiere_vue  TEXT NOT NULL,
            UNIQUE(chip_id, num_batterie)   -- une seule entrée par batterie
        );

        CREATE TABLE IF NOT EXISTS mesures (
            id                     INTEGER PRIMARY KEY AUTOINCREMENT,
            chip_id                TEXT NOT NULL,
            num_batterie           INTEGER NOT NULL,
            timestamp              TEXT NOT NULL,
            tensionBus_V           REAL,
            courant_A              REAL,
            puissance_W            REAL,
            tensionShunt_mV        REAL,
            temperature_C          REAL,
            temperaturebatterie_C  REAL
        );

        -- Index pour accélérer les requêtes par batterie et par date
        CREATE INDEX IF NOT EXISTS idx_mesures_batterie
            ON mesures(chip_id, num_batterie);
        CREATE INDEX IF NOT EXISTS idx_mesures_timestamp
            ON mesures(timestamp);
    """)
    conn.commit()

def enregistrer_batterie(conn: sqlite3.Connection, chip_id: str, num_batterie: int, timestamp: str):
    """Enregistre la batterie si elle n'existe pas encore."""
    conn.execute("""
        INSERT OR IGNORE INTO batteries (chip_id, num_batterie, premiere_vue)
        VALUES (?, ?, ?)
    """, (chip_id, num_batterie, timestamp))
    conn.commit()

def enregistrer_mesure(conn: sqlite3.Connection, donnees: dict):
    """Insère une nouvelle mesure et garde seulement les 500 dernières."""
    conn.execute("""
        INSERT INTO mesures
            (chip_id, num_batterie, timestamp,
             tensionBus_V, courant_A, puissance_W,
             tensionShunt_mV, temperature_C, temperaturebatterie_C)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    """, (
        donnees["chip_id"],
        donnees["num_batterie"],
        donnees["timestamp"],
        donnees["tensionBus_V"],
        donnees["courant_A"],
        donnees["puissance_W"],
        donnees["tensionShunt_mV"],
        donnees["temperature_C"],
        donnees["temperaturebatterie_C"],
    ))

    # Supprimer les mesures au-delà des 500 dernières pour cette batterie
    conn.execute("""
        DELETE FROM mesures
        WHERE chip_id = ? AND num_batterie = ?
        AND id NOT IN (
            SELECT id FROM mesures
            WHERE chip_id = ? AND num_batterie = ?
            ORDER BY id DESC
            LIMIT 500
        )
    """, (
        donnees["chip_id"], donnees["num_batterie"],
        donnees["chip_id"], donnees["num_batterie"],
    ))

    conn.commit()

def compter_mesures(conn: sqlite3.Connection, chip_id: str, num_batterie: int) -> int:
    """Retourne le nombre de mesures enregistrées pour une batterie."""
    cur = conn.execute("""
        SELECT COUNT(*) FROM mesures
        WHERE chip_id = ? AND num_batterie = ?
    """, (chip_id, num_batterie))
    return cur.fetchone()[0]

# ─── Décodage BLE ────────────────────────────────────────────────────────────
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

# ─── Affichage ───────────────────────────────────────────────────────────────
def afficher_donnees(donnees: dict, nb_mesures: int):
    clé = f"{donnees['chip_id']}_{donnees['num_batterie']}"
    print(f"\n{'─' * 45}")
    print(f"  Batterie     → {clé}  ({nb_mesures}/500 mesures)")
    print(f"  Horodatage   → {donnees['timestamp']}")
    print(f"{'─' * 45}")
    print(f"  Tension bus    : {donnees['tensionBus_V']:.2f} V")
    print(f"  Courant        : {donnees['courant_A']:.3f} A")
    print(f"  Puissance      : {donnees['puissance_W']:.1f} W")
    print(f"  Tension shunt  : {donnees['tensionShunt_mV']:.2f} mV")
    print(f"  Température    : {donnees['temperature_C']:.1f} °C")
    print(f"  Temp. batterie : {donnees['temperaturebatterie_C']:.1f} °C")
    print(f"{'─' * 45}")

# ─── Callback BLE ────────────────────────────────────────────────────────────
def make_callback(conn: sqlite3.Connection):
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

# ─── Main ────────────────────────────────────────────────────────────────────
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