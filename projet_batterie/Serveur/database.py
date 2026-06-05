import sqlite3
import pandas as pd
from config import FICHIER_DB, SEUILS


def connecter() -> sqlite3.Connection:
    conn = sqlite3.connect(FICHIER_DB, check_same_thread=False)
    conn.row_factory = sqlite3.Row
    return conn


def lister_batteries(conn):
    cur = conn.execute("""
        SELECT b.chip_id, b.num_batterie, b.premiere_vue,
               COUNT(m.id) as nb_mesures,
               MAX(m.timestamp) as derniere_mesure
        FROM batteries b
        LEFT JOIN mesures m ON b.chip_id = m.chip_id AND b.num_batterie = m.num_batterie
        GROUP BY b.chip_id, b.num_batterie
        ORDER BY b.num_batterie
    """)
    return cur.fetchall()


def derniere_mesure(conn, chip_id, num_batterie):
    cur = conn.execute("""
        SELECT * FROM mesures
        WHERE chip_id = ? AND num_batterie = ?
        ORDER BY timestamp DESC LIMIT 1
    """, (chip_id, num_batterie))
    return cur.fetchone()

def premiere_mesure(conn, chip_id, num_batterie):
    cur = conn.execute("""
        SELECT * FROM mesures
        WHERE chip_id = ? AND num_batterie = ?
        ORDER BY timestamp ASC LIMIT 1
    """, (chip_id, num_batterie))
    return cur.fetchone()

def moyenne_premieres_mesures(conn, chip_id, num_batterie, limite=50): # Limite passée à 50
    """Calcule la moyenne des 50 premières mesures (la référence fixe)."""
    df = pd.read_sql("""
        SELECT tensionBus_V, courant_A, puissance_W,
               tensionShunt_mV, temperature_C, temperaturebatterie_C
        FROM mesures
        WHERE chip_id = ? AND num_batterie = ?
        ORDER BY id ASC LIMIT ?
    """, conn, params=(chip_id, num_batterie, limite))
    
    if df.empty:
        return None
    
    return df.mean().to_dict()



def moyenne_dernieres_mesures(conn, chip_id, num_batterie, limite=450):
    """Calcule la moyenne des 'limite' dernières mesures enregistrées."""
    df = pd.read_sql("""
        SELECT tensionBus_V, courant_A, puissance_W,
               tensionShunt_mV, temperature_C, temperaturebatterie_C
        FROM mesures
        WHERE chip_id = ? AND num_batterie = ?
        ORDER BY id DESC LIMIT ?
    """, conn, params=(chip_id, num_batterie, limite))
    
    if df.empty:
        return None
    
    return df.mean().to_dict()


def charger_mesures(conn, chip_id, num_batterie, limite=100):
    df = pd.read_sql("""
        SELECT timestamp, tensionBus_V, courant_A, puissance_W,
               tensionShunt_mV, temperature_C, temperaturebatterie_C
        FROM mesures
        WHERE chip_id = ? AND num_batterie = ?
        ORDER BY timestamp DESC LIMIT ?
    """, conn, params=(chip_id, num_batterie, limite))
    df["timestamp"] = pd.to_datetime(df["timestamp"])
    return df.sort_values("timestamp")


def statistiques(conn, chip_id, num_batterie):
    cur = conn.execute("""
        SELECT
            MIN(tensionBus_V)  as v_min, MAX(tensionBus_V)  as v_max, AVG(tensionBus_V)  as v_moy,
            MIN(courant_A)     as i_min, MAX(courant_A)     as i_max, AVG(courant_A)     as i_moy,
            MIN(puissance_W)   as p_min, MAX(puissance_W)   as p_max, AVG(puissance_W)   as p_moy,
            MIN(temperature_C) as t_min, MAX(temperature_C) as t_max, AVG(temperature_C) as t_moy
        FROM mesures WHERE chip_id = ? AND num_batterie = ?
    """, (chip_id, num_batterie))
    return cur.fetchone()


def verifier_alertes(mesure) -> list[str]:
    alertes = []
    if mesure is None:
        return alertes
    for champ, (vmin, vmax) in SEUILS.items():
        val = mesure[champ]
        if val is not None and not (vmin <= val <= vmax):
            alertes.append(f"{champ} = {val:.2f} (seuil [{vmin}, {vmax}])")
    return alertes
