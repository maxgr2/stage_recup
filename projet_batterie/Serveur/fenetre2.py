import tkinter as tk
from tkinter import ttk
from datetime import datetime
from math import radians, sin

import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

from config import *
from database import (
    connecter,
    lister_batteries,
    derniere_mesure,
    charger_mesures,
    moyenne_premieres_mesures,
    moyenne_dernieres_mesures,
    statistiques,
    verifier_alertes,
)


# Noms correspondant au schéma courant de main.py/database.py.
COLONNES_MESURES = (
    "tensionBus_V",
    "courant_A",
    "impedance_ohm",
    "impedance_deg",
    "temperature_C",
    "temperaturebatterie_C",
    "tensionBus_charge_V",
)


class AppBatteries(tk.Tk):
    def __init__(self):
        super().__init__()
        self.conn = connecter()
        self.batterie_sel = None
        self._batteries_cache = []
        self.title("Surveillance Batteries")
        self.geometry("1280x850")
        self.configure(bg=COULEUR_FOND)
        self._build_ui()
        self._refresh()

    def _build_ui(self):
        tk.Label(
            self, text="Surveillance Batteries", bg=COULEUR_FOND,
            fg=COULEUR_TITRE, font=("Helvetica", 16, "bold")
        ).pack(pady=(10, 4))

        main = tk.Frame(self, bg=COULEUR_FOND)
        main.pack(fill="both", expand=True, padx=12, pady=6)

        gauche = tk.Frame(main, bg=COULEUR_FOND, width=350)
        gauche.pack(side="left", fill="y", padx=(0, 8))
        gauche.pack_propagate(False)

        tk.Label(
            gauche, text="Batteries", bg=COULEUR_FOND, fg=COULEUR_TITRE,
            font=("Helvetica", 11, "bold")
        ).pack(anchor="w")
        self.recherche_var = tk.StringVar()
        self.recherche_var.trace_add("write", lambda *_: self._filtrer_batteries())
        tk.Entry(
            gauche, textvariable=self.recherche_var, bg=COULEUR_PANEL,
            fg=COULEUR_TEXTE, insertbackground=COULEUR_TEXTE,
            font=("Helvetica", 10)
        ).pack(fill="x", pady=(4, 2))

        self.combo_var = tk.StringVar()
        self.combo = ttk.Combobox(
            gauche, textvariable=self.combo_var, state="readonly", font=("Helvetica", 10)
        )
        self.combo.pack(fill="x", pady=(0, 8))
        self.combo.bind("<<ComboboxSelected>>", self._on_select)

        tk.Label(
            gauche, text="Comparatif des moyennes", bg=COULEUR_FOND,
            fg=COULEUR_TITRE, font=("Helvetica", 10, "bold")
        ).pack(anchor="w", pady=(8, 0))
        style = ttk.Style()
        style.theme_use("clam")
        style.configure(
            "Treeview", background=COULEUR_PANEL, foreground=COULEUR_TEXTE,
            fieldbackground=COULEUR_PANEL, rowheight=23, borderwidth=0
        )
        style.configure("Treeview.Heading", background=COULEUR_HEADER, foreground=COULEUR_TITRE)
        self.tree = ttk.Treeview(
            gauche, columns=("Mesure", "Début (50)", "Récent (450)"),
            show="headings", height=9
        )
        for col in self.tree["columns"]:
            self.tree.heading(col, text=col)
            self.tree.column(col, width=140 if col == "Mesure" else 95, anchor="center")
        self.tree.pack(fill="x", pady=(4, 10))

        tk.Label(
            gauche, text="Alertes", bg=COULEUR_FOND, fg=COULEUR_TITRE,
            font=("Helvetica", 11, "bold")
        ).pack(anchor="w")
        self.alerte_frame = tk.Frame(gauche, bg=COULEUR_PANEL)
        self.alerte_frame.pack(fill="both", expand=True, pady=(4, 0))

        droite = tk.Frame(main, bg=COULEUR_FOND)
        droite.pack(side="left", fill="both", expand=True)
        self.fig, self.axes = plt.subplots(5, 1, figsize=(8, 7), facecolor=COULEUR_FOND)
        self.fig.subplots_adjust(hspace=0.72, top=0.97, bottom=0.07, left=0.10, right=0.98)
        self.canvas = FigureCanvasTkAgg(self.fig, master=droite)
        self.canvas.get_tk_widget().pack(fill="both", expand=True)

        stats_frame = tk.Frame(droite, bg=COULEUR_PANEL)
        stats_frame.pack(fill="x", pady=(5, 0))
        self.stats_labels = {}
        champs = [
            ("Tension (V)", "v"), ("Courant (A)", "i"),
            ("Temp. capteur 1 (°C)", "t1"), ("Temp. capteur 2 (°C)", "t2"),
            ("Impédance réelle (Ω)", "z"), ("Impédance imaginaire (Ω)", "zi"),
            ("Phase (°)", "phase"), ("Tension charge (V)", "vc"),
        ]
        for col, (nom, cle) in enumerate(champs):
            tk.Label(
                stats_frame, text=nom, bg=COULEUR_PANEL, fg=COULEUR_TITRE,
                font=("Helvetica", 8, "bold"), width=19
            ).grid(row=0, column=col, padx=2, pady=(4, 1))
            for row, stat in enumerate(("min", "moy", "max"), start=1):
                tk.Label(
                    stats_frame, text="—", bg=COULEUR_PANEL, fg=COULEUR_TEXTE,
                    font=("Helvetica", 8), width=10
                ).grid(row=row, column=col, padx=2, pady=1)
                self.stats_labels[f"{cle}_{stat}"] = stats_frame.grid_slaves(row=row, column=col)[0]

        self.status_var = tk.StringVar(value="En attente de données...")
        tk.Label(
            self, textvariable=self.status_var, bg=COULEUR_FOND,
            fg="#6c7086", font=("Helvetica", 8)
        ).pack(pady=(2, 5))

    @staticmethod
    def _key(batterie):
        return f"{batterie['chip_id'][-4:]}_{batterie['num_batterie']}"

    def _on_select(self, _event=None):
        valeur = self.combo_var.get()
        for batterie in self._batteries_cache:
            if valeur.startswith(self._key(batterie)):
                self.batterie_sel = (batterie["chip_id"], batterie["num_batterie"])
                self._maj_courbes()
                self._maj_stats()
                self._maj_tableau_comparatif()
                return

    def _refresh(self):
        self._maj_liste()
        self._maj_alertes()
        if self.batterie_sel:
            self._maj_courbes()
            self._maj_stats()
            self._maj_tableau_comparatif()
        self.status_var.set(f"Dernière mise à jour : {datetime.now().strftime('%H:%M:%S')}")
        self.after(REFRESH_MS, self._refresh)

    def _maj_liste(self):
        self._batteries_cache = lister_batteries(self.conn)
        self._filtrer_batteries()

    def _filtrer_batteries(self):
        filtre = self.recherche_var.get().lower()
        options = []
        for batterie in self._batteries_cache:
            mesure = derniere_mesure(self.conn, batterie["chip_id"], batterie["num_batterie"])
            tension = f"{mesure['tensionBus_V']:.1f}V" if mesure and mesure["tensionBus_V"] is not None else "—"
            maj = "—"
            if mesure and mesure["timestamp"]:
                try:
                    maj = datetime.fromisoformat(mesure["timestamp"]).strftime("%d/%m/%Y %H:%M")
                except (TypeError, ValueError):
                    pass
            label = f"{self._key(batterie)}  {tension}  {maj}"
            if filtre in label.lower():
                options.append(label)
        self.combo["values"] = options
        if options and self.combo_var.get() not in options:
            self.combo.set(options[0])
            self._on_select()

    def _maj_alertes(self):
        for widget in self.alerte_frame.winfo_children():
            widget.destroy()
        toutes = []
        for batterie in self._batteries_cache:
            mesure = derniere_mesure(self.conn, batterie["chip_id"], batterie["num_batterie"])
            for alerte in verifier_alertes(mesure):
                toutes.append(f"⚠ {batterie['chip_id']}_{batterie['num_batterie']} : {alerte}")
        if not toutes:
            tk.Label(
                self.alerte_frame, text="✓ Aucune alerte", bg=COULEUR_PANEL,
                fg=COULEUR_OK, font=("Helvetica", 9)
            ).pack(anchor="w", padx=8, pady=4)
        else:
            for texte in toutes:
                tk.Label(
                    self.alerte_frame, text=texte, bg=COULEUR_PANEL,
                    fg=COULEUR_ALERTE, font=("Helvetica", 8), wraplength=315,
                    justify="left"
                ).pack(anchor="w", padx=8, pady=2)

    @staticmethod
    def _imaginaire(df):
        return df["impedance_ohm"] * df["impedance_deg"].map(lambda angle: sin(radians(angle)))

    @staticmethod
    def _bornes_phase(valeurs):
        moyenne = float(valeurs.mean())
        bas = float(valeurs.min())
        haut = float(valeurs.max())
        if haut - bas < 10.0:
            bas, haut = moyenne - 5.0, moyenne + 5.0
        else:
            marge = max((haut - bas) * 0.10, 1.0)
            bas, haut = bas - marge, haut + marge
        return bas, haut

    def _style_ax(self, ax):
        ax.set_facecolor(COULEUR_PANEL)
        ax.tick_params(colors=COULEUR_TEXTE, labelsize=6)
        ax.grid(True, alpha=0.15, color="#45475a")
        for spine in ax.spines.values():
            spine.set_edgecolor("#45475a")

    def _maj_courbes(self):
        if not self.batterie_sel:
            return
        df = charger_mesures(self.conn, *self.batterie_sel)
        if df.empty:
            return
        imaginaire = self._imaginaire(df)
        configs = [
            [("tensionBus_V", "Tension (V)", "#89b4fa")],
            [("courant_A", "Courant (A)", "#a6e3a1")],
            [("temperature_C", "Température capteur 1", "#f38ba8"),
             ("temperaturebatterie_C", "Température capteur 2", "#fab387")],
            [("impedance_ohm", "Impédance réelle (Ω)", "#f9e2af"),
             ("__imaginaire__", "Impédance imaginaire (Ω)", "#cba6f7")],
            [("impedance_deg", "Phase (°)", "#89dceb")],
        ]
        for ax, courbes in zip(self.axes, configs):
            ax.clear()
            self._style_ax(ax)
            for colonne, label, couleur in courbes:
                valeurs = imaginaire if colonne == "__imaginaire__" else df[colonne]
                ax.plot(df["timestamp"], valeurs, color=couleur, linewidth=1.2, label=label)
            ax.set_ylabel("Temp. (°C)" if len(courbes) == 2 and courbes[0][0] == "temperature_C"
                          else ("Impédance (Ω)" if len(courbes) == 2 else courbes[0][1]),
                          color=COULEUR_TEXTE, fontsize=7)
            if courbes[0][0] == "impedance_deg":
                ax.set_ylim(*self._bornes_phase(df["impedance_deg"]))
            if len(courbes) > 1:
                legende = ax.legend(loc="upper left", fontsize=6, facecolor=COULEUR_PANEL,
                                    edgecolor="#45475a", labelcolor=COULEUR_TEXTE)
                for texte in legende.get_texts():
                    texte.set_color(COULEUR_TEXTE)
        self.canvas.draw_idle()

    def _maj_tableau_comparatif(self):
        if not self.batterie_sel:
            return
        debut = moyenne_premieres_mesures(self.conn, *self.batterie_sel, limite=50)
        recent = moyenne_dernieres_mesures(self.conn, *self.batterie_sel, limite=450)
        for item in self.tree.get_children():
            self.tree.delete(item)
        lignes = [
            ("Tension (V)", "tensionBus_V"), ("Courant (A)", "courant_A"),
            ("Temp. capteur 1 (°C)", "temperature_C"),
            ("Temp. capteur 2 (°C)", "temperaturebatterie_C"),
            ("Tension charge (V)", "tensionBus_charge_V"),
            ("Impédance réelle (Ω)", "impedance_ohm"),
            ("Phase (°)", "impedance_deg"),
        ]
        for nom, cle in lignes:
            val_debut = f"{debut[cle]:.2f}" if debut and debut.get(cle) is not None else "—"
            val_recent = f"{recent[cle]:.2f}" if recent and recent.get(cle) is not None else "—"
            self.tree.insert("", "end", values=(nom, val_debut, val_recent))

    def _maj_stats(self):
        if not self.batterie_sel:
            return
        df = charger_mesures(self.conn, *self.batterie_sel, limite=1000000)
        if df.empty:
            return
        valeurs = {
            "v": df["tensionBus_V"], "i": df["courant_A"],
            "t1": df["temperature_C"], "t2": df["temperaturebatterie_C"],
            "z": df["impedance_ohm"], "zi": self._imaginaire(df),
            "phase": df["impedance_deg"], "vc": df["tensionBus_charge_V"],
        }
        for cle, serie in valeurs.items():
            for stat, valeur in (("min", serie.min()), ("moy", serie.mean()), ("max", serie.max())):
                label = self.stats_labels.get(f"{cle}_{stat}")
                if label is not None and valeur == valeur:
                    label.config(text=f"{valeur:.2f}")


if __name__ == "__main__":
    app = AppBatteries()
    app.mainloop()
