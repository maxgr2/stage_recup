import json
import sqlite3
import threading
import logging
from datetime import datetime
from contextlib import contextmanager

import paho.mqtt.client as mqtt
from fastapi import FastAPI
from contextlib import asynccontextmanager

# ---------------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------------

MQTT_BROKER   = "192.168.1.100"   # IP du Raspberry Pi (ou "localhost" si script tourne sur le Pi)
MQTT_PORT     = 1883
MQTT_TOPIC    = "ets/salle1/mesures"
MQTT_CLIENT_ID = "rpi-fastapi-subscriber"

DB_PATH = "mesures.db"

logging.basicConfig(level=logging.INFO, format="[%(levelname)s] %(message)s")
log = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Base de données
# ---------------------------------------------------------------------------

def init_db():
    """Crée la table si elle n'existe pas encore."""
    with get_db() as conn:
        conn.execute("""
            CREATE TABLE IF NOT EXISTS mesures (
                id          INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp   TEXT    NOT NULL,
                room        INTEGER,
                slave       INTEGER,
                temperature REAL,
                humidity    REAL,
                mc1p0       REAL,
                mc2p5       REAL,
                mc4p0       REAL,
                mc10p0      REAL,
                nc0p5       REAL,
                nc1p0       REAL,
                nc2p5       REAL,
                nc4p0       REAL,
                nc10p0      REAL,
                tps         REAL
            )
        """)

@contextmanager
def get_db():
    """Context manager pour ouvrir/fermer proprement la connexion SQLite."""
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    try:
        yield conn
        conn.commit()
    except Exception:
        conn.rollback()
        raise
    finally:
        conn.close()

def insert_mesure(data: dict):
    """Insère une mesure dans la base de données."""
    with get_db() as conn:
        conn.execute("""
            INSERT INTO mesures (
                timestamp, room, slave,
                temperature, humidity,
                mc1p0, mc2p5, mc4p0, mc10p0,
                nc0p5, nc1p0, nc2p5, nc4p0, nc10p0,
                tps
            ) VALUES (
                :timestamp, :room, :slave,
                :temp, :hum,
                :mc1p0, :mc2p5, :mc4p0, :mc10p0,
                :nc0p5, :nc1p0, :nc2p5, :nc4p0, :nc10p0,
                :tps
            )
        """, {**data, "timestamp": datetime.now().isoformat()})
    log.info(f"Mesure insérée — salle {data['room']}, esclave {data['slave']}")

# ---------------------------------------------------------------------------
# Client MQTT
# ---------------------------------------------------------------------------

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        log.info(f"MQTT connecté au broker {MQTT_BROKER}")
        client.subscribe(MQTT_TOPIC)
        log.info(f"Souscrit à : {MQTT_TOPIC}")
    else:
        log.error(f"Connexion MQTT refusée, rc={rc}")

def on_disconnect(client, userdata, rc):
    if rc != 0:
        log.warning("Déconnexion MQTT inattendue, reconnexion automatique...")

def on_message(client, userdata, msg):
    """Callback appelé à chaque message reçu — parse le JSON et insère en DB."""
    try:
        data = json.loads(msg.payload.decode("utf-8"))
        insert_mesure(data)
    except json.JSONDecodeError as e:
        log.error(f"JSON invalide reçu : {e} — payload : {msg.payload}")
    except Exception as e:
        log.error(f"Erreur lors de l'insertion : {e}")

def start_mqtt_client():
    """Lance le client MQTT dans un thread séparé (non bloquant pour FastAPI)."""
    client = mqtt.Client(client_id=MQTT_CLIENT_ID)
    client.on_connect    = on_connect
    client.on_disconnect = on_disconnect
    client.on_message    = on_message

    # Reconnexion automatique intégrée à loop_forever()
    client.connect(MQTT_BROKER, MQTT_PORT, keepalive=60)

    thread = threading.Thread(target=client.loop_forever, daemon=True)
    thread.start()
    log.info("Thread MQTT démarré")
    return client

# ---------------------------------------------------------------------------
# FastAPI — lifespan (remplace les @app.on_event dépréciés)
# ---------------------------------------------------------------------------

@asynccontextmanager
async def lifespan(app: FastAPI):
    # Démarrage : init DB + lancement MQTT
    init_db()
    mqtt_client = start_mqtt_client()
    yield
    # Arrêt propre
    mqtt_client.disconnect()
    log.info("Client MQTT déconnecté")

app = FastAPI(lifespan=lifespan)

# ---------------------------------------------------------------------------
# Routes
# ---------------------------------------------------------------------------

@app.get("/mesures")
def get_mesures(limit: int = 100, room: int = None, slave: int = None):
    """Retourne les dernières mesures avec filtres optionnels."""
    query  = "SELECT * FROM mesures"
    params = []
    filters = []

    if room is not None:
        filters.append("room = ?")
        params.append(room)
    if slave is not None:
        filters.append("slave = ?")
        params.append(slave)
    if filters:
        query += " WHERE " + " AND ".join(filters)

    query += " ORDER BY id DESC LIMIT ?"
    params.append(limit)

    with get_db() as conn:
        rows = conn.execute(query, params).fetchall()

    return [dict(row) for row in rows]

@app.get("/mesures/latest")
def get_latest(room: int = None):
    """Retourne la dernière mesure par esclave."""
    query = """
        SELECT * FROM mesures
        WHERE id IN (
            SELECT MAX(id) FROM mesures
            {}
            GROUP BY slave
        )
        ORDER BY slave
    """.format("WHERE room = ?" if room is not None else "")

    params = [room] if room is not None else []

    with get_db() as conn:
        rows = conn.execute(query, params).fetchall()

    return [dict(row) for row in rows]

@app.get("/health")
def health():
    return {"status": "ok"}
