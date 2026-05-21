import tkinter as tk
from tkinter import ttk
import sqlite3
import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from datetime import datetime

# ─── Configuration ────────────────────────────────────────────────────────────
FICHIER_DB       = "batteries.db"
REFRESH_MS       = 5000   # rafraîchissement toutes les 5 secondes

# Seuils d'alerte
SEUILS = {
    "tensionBus_V":          (10.0, 14.8),   # (min, max)
    "temperature_C":         (0.0,  1200.0),
    "temperaturebatterie_C": (0.0,  45.0),
    "courant_A":             (-10.0, 15.0),
}

COULEUR_OK      = "#2ecc71"
COULEUR_ALERTE  = "#e74c3c"
COULEUR_FOND    = "#1e1e2e"
COULEUR_PANEL   = "#2a2a3e"
COULEUR_TEXTE   = "#cdd6f4"
COULEUR_TITRE   = "#89b4fa"
COULEUR_HEADER  = "#313244"

# ─── Base de données ──────────────────────────────────────────────────────────
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
            MIN(tensionBus_V) as v_min, MAX(tensionBus_V) as v_max, AVG(tensionBus_V) as v_moy,
            MIN(courant_A)    as i_min, MAX(courant_A)    as i_max, AVG(courant_A)    as i_moy,
            MIN(puissance_W)  as p_min, MAX(puissance_W)  as p_max, AVG(puissance_W)  as p_moy,
            MIN(temperature_C) as t_min, MAX(temperature_C) as t_max, AVG(temperature_C) as t_moy
        FROM mesures WHERE chip_id = ? AND num_batterie = ?
    """, (chip_id, num_batterie))
    return cur.fetchone()

# ─── Vérification alertes ─────────────────────────────────────────────────────
def verifier_alertes(mesure) -> list[str]:
    alertes = []
    if mesure is None:
        return alertes
    for champ, (vmin, vmax) in SEUILS.items():
        val = mesure[champ]
        if val is not None and not (vmin <= val <= vmax):
            alertes.append(f"{champ} = {val:.2f} (seuil [{vmin}, {vmax}])")
    return alertes

# ─── Application principale ───────────────────────────────────────────────────
class AppBatteries(tk.Tk):
    def __init__(self):
        super().__init__()
        self.conn            = connecter()
        self.batterie_sel    = None  # (chip_id, num_batterie) sélectionnée
        self.title("Surveillance Batteries")
        self.geometry("1200x750")
        self.configure(bg=COULEUR_FOND)
        self._build_ui()
        self._refresh()

    # ── Construction UI ───────────────────────────────────────────────────────
    def _build_ui(self):
        # ── Titre
        tk.Label(self, text="Surveillance Batteries",
                 bg=COULEUR_FOND, fg=COULEUR_TITRE,
                 font=("Helvetica", 16, "bold")).pack(pady=(12, 4))

        # ── Conteneur principal
        main = tk.Frame(self, bg=COULEUR_FOND)
        main.pack(fill="both", expand=True, padx=12, pady=6)

        # ── Colonne gauche : liste + alertes
        gauche = tk.Frame(main, bg=COULEUR_FOND, width=320)
        gauche.pack(side="left", fill="y", padx=(0, 8))
        gauche.pack_propagate(False)

        # Liste batteries
        tk.Label(gauche, text="Batteries", bg=COULEUR_FOND,
                 fg=COULEUR_TITRE, font=("Helvetica", 11, "bold")).pack(anchor="w")

        self.recherche_var = tk.StringVar()
        self.recherche_var.trace_add("write", lambda *_: self._filtrer_batteries())
        tk.Entry(gauche, textvariable=self.recherche_var,
                bg=COULEUR_PANEL, fg=COULEUR_TEXTE,
                insertbackground=COULEUR_TEXTE,
                font=("Helvetica", 10)).pack(fill="x", pady=(4, 2))

        # Menu déroulant
        self.combo_var = tk.StringVar()
        self.combo = ttk.Combobox(gauche, textvariable=self.combo_var,
                                state="readonly", font=("Helvetica", 10))
        self.combo.pack(fill="x", pady=(0, 8))
        self.combo.bind("<<ComboboxSelected>>", self._on_select)
        self._batteries_cache = []  # liste complète pour le filtre
        tk.Label(gauche, text="Alertes", bg=COULEUR_FOND,
                 fg=COULEUR_TITRE, font=("Helvetica", 11, "bold")).pack(anchor="w")
        self.alerte_frame = tk.Frame(gauche, bg=COULEUR_PANEL,
                                     relief="flat", bd=0)
        self.alerte_frame.pack(fill="both", expand=True, pady=(4, 0))
        self.alerte_labels = []

        # ── Colonne droite : courbes + stats
        droite = tk.Frame(main, bg=COULEUR_FOND)
        droite.pack(side="left", fill="both", expand=True)

        # Courbes
        self.fig, self.axes = plt.subplots(3, 1, figsize=(7, 5),
                                            facecolor=COULEUR_FOND)
        self.fig.subplots_adjust(hspace=0.45, top=0.95, bottom=0.08)
        for ax in self.axes:
            ax.set_facecolor(COULEUR_PANEL)
            ax.tick_params(colors=COULEUR_TEXTE, labelsize=7)
            for spine in ax.spines.values():
                spine.set_edgecolor("#45475a")

        self.canvas = FigureCanvasTkAgg(self.fig, master=droite)
        self.canvas.get_tk_widget().pack(fill="both", expand=True)

        # Stats
        stats_frame = tk.Frame(droite, bg=COULEUR_PANEL)
        stats_frame.pack(fill="x", pady=(6, 0))
        self.stats_labels = {}
        champs = [
            ("Tension (V)",  "v"),
            ("Courant (A)",  "i"),
            ("Puissance (W)","p"),
            ("Temp. (°C)",   "t"),
        ]
        for i, (nom, cle) in enumerate(champs):
            tk.Label(stats_frame, text=nom, bg=COULEUR_PANEL,
                     fg=COULEUR_TITRE, font=("Helvetica", 8, "bold"),
                     width=12).grid(row=0, column=i*4, padx=(8,0), pady=4)
            for j, stat in enumerate(["min", "moy", "max"]):
                lbl = tk.Label(stats_frame, text="—", bg=COULEUR_PANEL,
                               fg=COULEUR_TEXTE, font=("Helvetica", 8), width=7)
                lbl.grid(row=0, column=i*4+j+1, padx=2)
                self.stats_labels[f"{cle}_{stat}"] = lbl

        # Barre de statut
        self.status_var = tk.StringVar(value="En attente de données...")
        tk.Label(self, textvariable=self.status_var,
                 bg=COULEUR_FOND, fg="#6c7086",
                 font=("Helvetica", 8)).pack(pady=(2, 6))

    # ── Sélection batterie ────────────────────────────────────────────────────
    def _on_select(self, _event):
        val = self.combo_var.get()
        for b in self._batteries_cache:
            clé = f"{b['chip_id'][-4:]}_{b['num_batterie']}"
            if val.startswith(clé):
                self.batterie_sel = (b["chip_id"], b["num_batterie"])
                self._maj_courbes()
                self._maj_stats()
                break

    # ── Rafraîchissement ──────────────────────────────────────────────────────
    def _refresh(self):
        self._maj_liste()
        self._maj_alertes()
        if self.batterie_sel:
            self._maj_courbes()
            self._maj_stats()
        self.status_var.set(f"Dernière mise à jour : {datetime.now().strftime('%H:%M:%S')}")
        self.after(REFRESH_MS, self._refresh)

    # ── Mise à jour liste ─────────────────────────────────────────────────────
    def _filtrer_batteries(self, *_args):
        filtre = self.recherche_var.get().lower()
        options = []
        for b in self._batteries_cache:
            m   = derniere_mesure(self.conn, b["chip_id"], b["num_batterie"])
            clé = f"{b['chip_id'][-4:]}_{b['num_batterie']}"
            v   = f"{m['tensionBus_V']:.1f}V" if m else "—"
            maj = "—"
            if m and m["timestamp"]:
                try:
                    t   = datetime.fromisoformat(m["timestamp"])
                    maj = t.strftime("%H:%M")
                except Exception:
                    pass
            label = f"{clé}  {v}  {maj}"
            if filtre in label.lower():
                options.append(label)

        self.combo["values"] = options
        # Garder la sélection si toujours visible
        if self.combo_var.get() not in options and options:
            self.combo.set(options[0])
    
    def _maj_liste(self):
        self._batteries_cache = lister_batteries(self.conn)
        self._filtrer_batteries()

    # ── Mise à jour alertes ───────────────────────────────────────────────────
    def _maj_alertes(self):
        for w in self.alerte_frame.winfo_children():
            w.destroy()

        toutes = []
        for b in lister_batteries(self.conn):
            m       = derniere_mesure(self.conn, b["chip_id"], b["num_batterie"])
            alertes = verifier_alertes(m)
            clé     = f"{b['chip_id'][-4:]}_{b['num_batterie']}"
            for a in alertes:
                toutes.append(f"⚠ {clé} : {a}")

        if not toutes:
            tk.Label(self.alerte_frame, text="Aucune alerte",
                     bg=COULEUR_PANEL, fg=COULEUR_OK,
                     font=("Helvetica", 9)).pack(anchor="w", padx=8, pady=4)
        else:
            for txt in toutes:
                tk.Label(self.alerte_frame, text=txt, bg=COULEUR_PANEL,
                         fg=COULEUR_ALERTE, font=("Helvetica", 8),
                         wraplength=290, justify="left").pack(anchor="w", padx=8, pady=2)

    # ── Mise à jour courbes ───────────────────────────────────────────────────
    def _maj_courbes(self):
        if not self.batterie_sel:
            return
        chip_id, num = self.batterie_sel
        df = charger_mesures(self.conn, chip_id, num)
        if df.empty:
            return

        configs = [
            ("tensionBus_V",  "Tension (V)",   "#89b4fa"),
            ("courant_A",     "Courant (A)",   "#a6e3a1"),
            ("temperaturebatterie_C", "Temp. (°C)",    "#f38ba8"),
        ]

        for ax, (col, label, couleur) in zip(self.axes, configs):
            ax.clear()
            ax.set_facecolor(COULEUR_PANEL)
            ax.plot(df["timestamp"], df[col], color=couleur, linewidth=1.2)
            ax.set_ylabel(label, color=COULEUR_TEXTE, fontsize=7)
            ax.tick_params(colors=COULEUR_TEXTE, labelsize=6)
            ax.grid(True, alpha=0.15, color="#45475a")
            for spine in ax.spines.values():
                spine.set_edgecolor("#45475a")

        self.canvas.draw()

    # ── Mise à jour stats ─────────────────────────────────────────────────────
    def _maj_stats(self):
        if not self.batterie_sel:
            return
        chip_id, num = self.batterie_sel
        s = statistiques(self.conn, chip_id, num)
        if not s:
            return

        mapping = {
            "v_min": s["v_min"], "v_moy": s["v_moy"], "v_max": s["v_max"],
            "i_min": s["i_min"], "i_moy": s["i_moy"], "i_max": s["i_max"],
            "p_min": s["p_min"], "p_moy": s["p_moy"], "p_max": s["p_max"],
            "t_min": s["t_min"], "t_moy": s["t_moy"], "t_max": s["t_max"],
        }
        for k, val in mapping.items():
            lbl = self.stats_labels.get(k)
            if lbl and val is not None:
                lbl.config(text=f"{val:.2f}")

# ─── Lancement ────────────────────────────────────────────────────────────────
if __name__ == "__main__":
    app = AppBatteries()
    app.mainloop()