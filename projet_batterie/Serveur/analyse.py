import sqlite3
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
from datetime import datetime

FICHIER_DB = "batteries.db"

# ─── Connexion ───────────────────────────────────────────────────────────────
def connecter():
    conn = sqlite3.connect(FICHIER_DB, check_same_thread=False)
    conn.row_factory = sqlite3.Row  # accès par nom de colonne
    return conn

# ─── Requêtes utiles ─────────────────────────────────────────────────────────
def lister_batteries(conn: sqlite3.Connection):
    """Retourne la liste de toutes les batteries connues."""
    cur = conn.execute("""
        SELECT b.chip_id, b.num_batterie, b.premiere_vue,
               COUNT(m.id) as nb_mesures,
               MAX(m.timestamp) as derniere_mesure
        FROM batteries b
        LEFT JOIN mesures m
            ON b.chip_id = m.chip_id AND b.num_batterie = m.num_batterie
        GROUP BY b.chip_id, b.num_batterie
        ORDER BY b.num_batterie
    """)
    return cur.fetchall()

def charger_mesures(conn: sqlite3.Connection, chip_id: str, num_batterie: int):
    """Charge toutes les mesures d'une batterie dans un DataFrame."""
    df = pd.read_sql("""
        SELECT timestamp, tensionBus_V, courant_A, puissance_W,
               tensionShunt_mV, temperature_C, temperaturebatterie_C
        FROM mesures
        WHERE chip_id = ? AND num_batterie = ?
        ORDER BY timestamp ASC
    """, conn, params=(chip_id, num_batterie))

    df["timestamp"] = pd.to_datetime(df["timestamp"])
    return df

def statistiques(df: pd.DataFrame) -> dict:
    """Calcule les stats de base sur les mesures."""
    cols = ["tensionBus_V", "courant_A", "puissance_W",
            "temperature_C", "temperaturebatterie_C"]
    return {
        col: {
            "min":     df[col].min(),
            "max":     df[col].max(),
            "moyenne": df[col].mean(),
        }
        for col in cols
    }

# ─── Affichage console ───────────────────────────────────────────────────────
def afficher_liste_batteries(conn: sqlite3.Connection):
    batteries = lister_batteries(conn)
    print(f"\n{'─' * 60}")
    print(f"  {'Batterie':<20} {'Mesures':>8}  {'Dernière mesure'}")
    print(f"{'─' * 60}")
    for b in batteries:
        clé = f"{b['chip_id']}_{b['num_batterie']}"
        print(f"  {clé:<20} {b['nb_mesures']:>8}  {b['derniere_mesure']}")
    print(f"{'─' * 60}\n")

def afficher_statistiques(conn: sqlite3.Connection, chip_id: str, num_batterie: int):
    df = charger_mesures(conn, chip_id, num_batterie)
    if df.empty:
        print("Aucune mesure disponible.")
        return

    stats = statistiques(df)
    clé   = f"{chip_id}_{num_batterie}"
    print(f"\n{'─' * 45}")
    print(f"  Statistiques → {clé}  ({len(df)} mesures)")
    print(f"{'─' * 45}")
    for col, valeurs in stats.items():
        print(f"  {col:<25} min={valeurs['min']:.2f}  max={valeurs['max']:.2f}  moy={valeurs['moyenne']:.2f}")
    print(f"{'─' * 45}\n")

# ─── Courbes ─────────────────────────────────────────────────────────────────
def tracer_courbes(conn: sqlite3.Connection, chip_id: str, num_batterie: int):
    df  = charger_mesures(conn, chip_id, num_batterie)
    clé = f"{chip_id}_{num_batterie}"

    if df.empty:
        print(f"Aucune mesure pour {clé}")
        return

    fig, axes = plt.subplots(3, 1, figsize=(12, 9), sharex=True)
    fig.suptitle(f"Batterie {clé}", fontsize=14, fontweight="bold")

    # Tension
    axes[0].plot(df["timestamp"], df["tensionBus_V"], color="#2196F3", linewidth=1.5)
    axes[0].set_ylabel("Tension (V)")
    axes[0].grid(True, alpha=0.3)

    # Courant + Puissance
    axes[1].plot(df["timestamp"], df["courant_A"],  color="#4CAF50", linewidth=1.5, label="Courant (A)")
    axes[1].plot(df["timestamp"], df["puissance_W"], color="#FF9800", linewidth=1.5, label="Puissance (W)")
    axes[1].set_ylabel("Courant / Puissance")
    axes[1].legend()
    axes[1].grid(True, alpha=0.3)

    # Températures
    axes[2].plot(df["timestamp"], df["temperature_C"],          color="#F44336", linewidth=1.5, label="Temp. INA (°C)")
    axes[2].plot(df["timestamp"], df["temperaturebatterie_C"],  color="#9C27B0", linewidth=1.5, label="Temp. batterie (°C)")
    axes[2].set_ylabel("Température (°C)")
    axes[2].legend()
    axes[2].grid(True, alpha=0.3)

    # Format axe X
    axes[2].xaxis.set_major_formatter(mdates.DateFormatter("%H:%M"))
    fig.autofmt_xdate()

    plt.tight_layout()
    plt.show()

def tracer_comparaison(conn: sqlite3.Connection):
    """Compare la tension de toutes les batteries sur un même graphique."""
    batteries = lister_batteries(conn)

    plt.figure(figsize=(12, 5))
    plt.title("Comparaison tension — toutes les batteries", fontsize=13, fontweight="bold")

    for b in batteries:
        df  = charger_mesures(conn, b["chip_id"], b["num_batterie"])
        clé = f"{b['chip_id']}_{b['num_batterie']}"
        if not df.empty:
            plt.plot(df["timestamp"], df["tensionBus_V"], linewidth=1.5, label=clé)

    plt.ylabel("Tension bus (V)")
    plt.xlabel("Temps")
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.gca().xaxis.set_major_formatter(mdates.DateFormatter("%H:%M"))
    plt.gcf().autofmt_xdate()
    plt.tight_layout()
    plt.show()

# ─── Main ────────────────────────────────────────────────────────────────────
if __name__ == "__main__":
    conn = connecter()

    # Liste toutes les batteries
    afficher_liste_batteries(conn)

    # Récupère la première batterie disponible pour l'exemple
    batteries = lister_batteries(conn)
    if batteries:
        b = batteries[0]
        afficher_statistiques(conn, b["chip_id"], b["num_batterie"])
        tracer_courbes(conn, b["chip_id"], b["num_batterie"])

    # Comparaison toutes batteries
    if len(batteries) > 1:
        tracer_comparaison(conn)

    conn.close()