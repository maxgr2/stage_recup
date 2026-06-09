import asyncio
import struct
import sqlite3
from datetime import datetime
from bleak import BleakScanner

from projet_batterie.Serveur.main_json import MANUFACTURER_ID

# ------ Configuration ----------------------------------------------------------------------------------------------------------------------
ESP32_NOM       = "Esp_maitreX" #  a modifier pour la salle blanche
TAILLE_STRUCT   = 0 #a modifier plus tard
FICHIER_DB      = "salle_blanche.db"

# ------ Base de données ------------------------------------------------------------------------------------------------------------------
def init_db(conn: sqlite3.Connection):
    """Crée les tables si elles n'existent pas encore."""
    conn.executescript("""
        CREATE TABLE IF NOT EXISTS salle_blanche (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            Capteur_number       TEXT NOT NULL,
            premiere_vue  TEXT NOT NULL,
            UNIQUE(Capteur_number)   -- une seule entrée par batterie
        );

        CREATE TABLE IF NOT EXISTS mesures (
            id                     INTEGER PRIMARY KEY AUTOINCREMENT,
            Capteur_number         TEXT NOT NULL,
            timestamp              TEXT NOT NULL,
            HUMIDITY           REAL,
            TEMPERATURE              REAL,
            PRESSURE              REAL,
            PM1_0                 REAL,
            PM2_5                 REAL,
            PM10                  REAL
        );

        -- Index pour accélérer les requêtes par salle et par date
        CREATE INDEX IF NOT EXISTS idx_mesures_salle
            ON mesures(Capteur_number);
        CREATE INDEX IF NOT EXISTS idx_mesures_timestamp
            ON mesures(timestamp);
    """)
    conn.commit()

def enregistrer_salle(conn: sqlite3.Connection, Capteur_number: str, NB_PM: int, timestamp: str):
    """Enregistre la salle si elle n'existe pas encore."""
    conn.execute("""
        INSERT OR IGNORE INTO salle_blanche (Capteur_number, premiere_vue)
        VALUES (?, ?)
    """, (Capteur_number, timestamp))
    conn.commit()

def enregistrer_mesure(conn: sqlite3.Connection, donnees: dict):
    """Insère une mesure et protège les 50 premières + les 450 dernières."""

    conn.execute("""
        INSERT INTO mesures
            (Capteur_number, NB_PM, timestamp,
             HUMIDITY, TEMPERATURE, PRESSURE, PM1_0, PM2_5, PM10)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    """, (
        donnees["Capteur_number"],
        donnees["NB_PM"],
        donnees["timestamp"],
        donnees["HUMIDITY"],
        donnees["TEMPERATURE"],
        donnees["PRESSURE"],
        donnees["PM1_0"],
        donnees["PM2_5"],
        donnees["PM10"]
        donnees["tensionBus_charge_V"],
        donnees["impedance_ohms"]
    ))

    # Nettoyage intelligent A faire


def compter_mesures(conn: sqlite3.Connection, Capteur_number: str, NB_PM: int) -> int:
    """Retourne le nombre de mesures enregistrées pour une batterie."""
    cur = conn.execute("""
        SELECT COUNT(*) FROM mesures
        WHERE Capteur_number = ? 
    """, (Capteur_number))
    return cur.fetchone()[0]

# ---- Décodage BLE ------------------------------------------------------------------------------------------------------------------------
def decoder_payload(raw_bytes: bytes) -> dict | None:
    """Décode les paquets recut et les remet sous la bonne unité car transmis en entier,  *10 ou *100"""
    if len(raw_bytes) != TAILLE_STRUCT:
        print(f"Taille inattendue : {len(raw_bytes)} octets (attendu {TAILLE_STRUCT})")
        return None

    chip_id      = struct.unpack_from("<I", raw_bytes, 0)[0]
    bruts        = struct.unpack_from("<7h", raw_bytes, 5)

    #A vérifier avec les données du capteur, à adapter en fonction de ce qui est envoyé
    return {
        "chip_id":               f"{chip_id:08X}",
        "NB_PM":                bruts[0],
        "HUMIDITY":             bruts[1] / 10.0,
        "TEMPERATURE":          bruts[2] / 10.0,
        "PRESSURE":             bruts[3] / 10.0,
        "PM1_0":                bruts[4] / 10.0,
        "PM2_5":                bruts[5] / 10.0,
        "PM10":                 bruts[6] / 10.0,
        "timestamp":             datetime.now().isoformat(),
    }
# ------ Callback BLE ------------------------------------------------------------------------------------------------------------------------
def make_callback(conn: sqlite3.Connection):
    """fonction de callback, c'est une fonction de fonction pour pouvoir prendre plus de 2 arguments"""
    def detection_callback(device, advertisement_data):
        if device.name != ESP32_NOM:
            return

        manufacturer_data = advertisement_data.manufacturer_data
        if MANUFACTURER_ID not in manufacturer_data:
            return

        donnees = decoder_payload(manufacturer_data[MANUFACTURER_ID])
        if donnees is None:
            return

        enregistrer_batterie(conn, donnees["chip_id"], donnees["NB_PM"], donnees["timestamp"])
        enregistrer_mesure(conn, donnees)
        nb = compter_mesures(conn, donnees["chip_id"], donnees["NB_PM"])
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