import tkinter as tk
from tkinter import ttk
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from datetime import datetime

from config   import *
from database import (connecter, lister_batteries, derniere_mesure,charger_mesures, moyenne_dernieres_mesures, moyenne_premieres_mesures, statistiques, verifier_alertes)


class AppBatteries(tk.Tk):
    def __init__(self):
        super().__init__()
        self.conn            = connecter()
        self.batterie_sel    = None  # (chip_id, num_batterie)
        self._batteries_cache = []
        self.title("Surveillance Batteries")
        self.geometry("1200x750")
        self.configure(bg=COULEUR_FOND)
        self._build_ui()
        self._refresh()

    # ---- Construction UI --------------------------------------------------------------------------------------------------------------
    def _build_ui(self):
        # Titre
        tk.Label(self, text="Surveillance Batteries",
                 bg=COULEUR_FOND, fg=COULEUR_TITRE,
                 font=("Helvetica", 16, "bold")).pack(pady=(12, 4))

        # Conteneur principal
        main = tk.Frame(self, bg=COULEUR_FOND)
        main.pack(fill="both", expand=True, padx=12, pady=6)

        # ---- Colonne gauche
        gauche = tk.Frame(main, bg=COULEUR_FOND, width=320)
        gauche.pack(side="left", fill="y", padx=(0, 8))
        gauche.pack_propagate(False)

        tk.Label(gauche, text="Batteries", bg=COULEUR_FOND,fg=COULEUR_TITRE, font=("Helvetica", 11, "bold")).pack(anchor="w")

        # Champ de recherche
        self.recherche_var = tk.StringVar()
        self.recherche_var.trace_add("write", lambda *_: self._filtrer_batteries())
        tk.Entry(gauche, textvariable=self.recherche_var,bg=COULEUR_PANEL, fg=COULEUR_TEXTE,insertbackground=COULEUR_TEXTE,font=("Helvetica", 10)).pack(fill="x", pady=(4, 2))

        # Menu déroulant
        self.combo_var = tk.StringVar()
        self.combo = ttk.Combobox(gauche, textvariable=self.combo_var,state="readonly", font=("Helvetica", 10))
        self.combo.pack(fill="x", pady=(0, 8))
        self.combo.bind("<<ComboboxSelected>>", self._on_select)

        #tableau moyenne 
        tk.Label(gauche, text="Comparatif Moyennes", bg=COULEUR_FOND, fg=COULEUR_TITRE, font=("Helvetica", 10, "bold")).pack(anchor="w", pady=(10, 0))

        # Configuration du style pour le thème sombre
        style = ttk.Style()
        style.theme_use("clam")
        style.configure("Treeview", background=COULEUR_PANEL, foreground=COULEUR_TEXTE, fieldbackground=COULEUR_PANEL, rowheight=25, borderwidth=0)
        style.configure("Treeview.Heading", background=COULEUR_HEADER, foreground=COULEUR_TITRE)

        colonnes = ("Mesure", "Début (50)", "Récent (450)")
        self.tree = ttk.Treeview(gauche, columns=colonnes, show="headings", height=7)
        
        # Définition des entêtes
        for col in colonnes:
            self.tree.heading(col, text=col)
            largeur = 120 if col == "Mesure" else 90
            self.tree.column(col, width=largeur, anchor="center")
        
        self.tree.pack(fill="x", pady=(4, 10))

        # Alertes
        tk.Label(gauche, text="Alertes", bg=COULEUR_FOND,fg=COULEUR_TITRE, font=("Helvetica", 11, "bold")).pack(anchor="w")
        self.alerte_frame = tk.Frame(gauche, bg=COULEUR_PANEL, relief="flat", bd=0)
        self.alerte_frame.pack(fill="both", expand=True, pady=(4, 0))

        # ---- Colonne droite
        droite = tk.Frame(main, bg=COULEUR_FOND)
        droite.pack(side="left", fill="both", expand=True)

        # Courbes
        self.fig, self.axes = plt.subplots(3, 1, figsize=(7, 5), facecolor=COULEUR_FOND)
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
            ("Tension (V)",   "v"),
            ("Courant (A)",   "i"),
            ("Puissance (W)", "p"),
            ("Temp. (°C)",    "t"),
        ]
        for i, (nom, cle) in enumerate(champs):
            tk.Label(stats_frame, text=nom, bg=COULEUR_PANEL,fg=COULEUR_TITRE, font=("Helvetica", 8, "bold"),width=12).grid(row=0, column=i * 4, padx=(8, 0), pady=4)
            for j, stat in enumerate(["min", "moy", "max"]):
                lbl = tk.Label(stats_frame, text="—", bg=COULEUR_PANEL,fg=COULEUR_TEXTE, font=("Helvetica", 8), width=7)
                lbl.grid(row=0, column=i * 4 + j + 1, padx=2)
                self.stats_labels[f"{cle}_{stat}"] = lbl

        # Barre de statut
        self.status_var = tk.StringVar(value="En attente de données...")
        tk.Label(self, textvariable=self.status_var,bg=COULEUR_FOND, fg="#6c7086",font=("Helvetica", 8)).pack(pady=(2, 6))

    # ---- Sélection batterie --------------------------------------------------------------------------------------------------------
    def _on_select(self, _event):
        val = self.combo_var.get()
        for b in self._batteries_cache:
            clé = f"{b['chip_id'][-4:]}_{b['num_batterie']}"
            if val.startswith(clé):
                self.batterie_sel = (b["chip_id"], b["num_batterie"])
                self._maj_courbes()
                self._maj_stats()
                break

    # ---- Rafraîchissement ------------------------------------------------------------------------------------------------------------
    def _refresh(self):
        self._maj_liste()
        self._maj_alertes()
        if self.batterie_sel:
            self._maj_courbes()
            self._maj_stats()
            self._maj_tableau_comparatif()
        self.status_var.set(f"Dernière mise à jour : {datetime.now().strftime('%H:%M:%S')}")
        self.after(REFRESH_MS, self._refresh)

    # ---- Mise à jour liste ----------------------------------------------------------------------------------------------------------
    def _maj_liste(self):
        self._batteries_cache = lister_batteries(self.conn)
        self._filtrer_batteries()

    def _filtrer_batteries(self):
        filtre  = self.recherche_var.get().lower()
        options = []
        for b in self._batteries_cache:
            m   = derniere_mesure(self.conn, b["chip_id"], b["num_batterie"])
            clé = f"{b['chip_id'][-4:]}_{b['num_batterie']}"
            v   = f"{m['tensionBus_V']:.1f}V" if m else "—"
            maj = "—"
            if m and m["timestamp"]:
                try:
                    maj = datetime.fromisoformat(m["timestamp"]).strftime("%d/%m/%Y %H:%M")
                except Exception:
                    pass
            label = f"{clé}  {v}  {maj}"
            if filtre in label.lower():
                options.append(label)

        self.combo["values"] = options
        if self.combo_var.get() not in options and options:
            self.combo.set(options[0])

    # ---- Mise à jour alertes ------------------------------------------------------------------------------------------------------
    def _maj_alertes(self):
        for w in self.alerte_frame.winfo_children():
            w.destroy()

        toutes = []
        for b in lister_batteries(self.conn):
            m       = derniere_mesure(self.conn, b["chip_id"], b["num_batterie"])
            alertes = verifier_alertes(m)
            clé     = f"{b['chip_id']}_{b['num_batterie']}"
            for a in alertes:
                toutes.append(f"⚠ {clé} : {a}")

        if not toutes:
            tk.Label(self.alerte_frame, text="✓ Aucune alerte",bg=COULEUR_PANEL, fg=COULEUR_OK,font=("Helvetica", 9)).pack(anchor="w", padx=8, pady=4)
        else:
            for txt in toutes:
                tk.Label(self.alerte_frame, text=txt, bg=COULEUR_PANEL,fg=COULEUR_ALERTE, font=("Helvetica", 8),wraplength=290, justify="left").pack(anchor="w", padx=8, pady=2)

    # ---- Mise à jour courbes ------------------------------------------------------------------------------------------------------
    def _maj_courbes(self):
        if not self.batterie_sel:
            return
        chip_id, num = self.batterie_sel
        df = charger_mesures(self.conn, chip_id, num)
        moyennes = moyenne_premieres_mesures(self.conn, chip_id, num, limite=50)
        if df.empty:
            return

        configs = [
            ("tensionBus_V",          "Tension (V)",  "#89b4fa"),
            ("courant_A",             "Courant (A)",  "#a6e3a1"),
            ("temperaturebatterie_C", "Temp. (°C)",   "#f38ba8"),
        ]
        for ax, (col, label, couleur) in zip(self.axes, configs):
            ax.clear()
            ax.set_facecolor(COULEUR_PANEL)
            ax.plot(df["timestamp"], df[col], color=couleur, linewidth=1.2)
            ax.set_ylabel(label, color=COULEUR_TEXTE, fontsize=7)
            ax.tick_params(colors=COULEUR_TEXTE, labelsize=6)
            ax.grid(True, alpha=0.15, color="#45475a")
            if moyennes and col in moyennes:
                moy = moyennes[col]
                ax.axhline(y=moy, color="#bac2de", linestyle="--", linewidth=1.5, alpha=0.7)
                # Petit texte pour afficher la valeur de la moyenne
                ax.text(df["timestamp"].iloc[0], moy, "valeur_moy_première_mesure", color="#bac2de", fontsize=7, va='bottom')
            for spine in ax.spines.values():
                spine.set_edgecolor("#45475a")

        self.canvas.draw()

    # ---- Mise à jour tableau comparatif ----------------------------------------------------------------------------------
    def _maj_tableau_comparatif(self):
        if not self.batterie_sel:
            return
        chip_id, num = self.batterie_sel
        
        # Récupération des deux sets de moyennes
        moy_debut = moyenne_premieres_mesures(self.conn, chip_id, num, limite=50)
        moy_recent = moyenne_dernieres_mesures(self.conn, chip_id, num, limite=450)
        
        # Nettoyage du tableau
        for item in self.tree.get_children():
            self.tree.delete(item)
            
        mapping = [
            ("Tension (V)",    "tensionBus_V"),
            ("Courant (A)",    "courant_A"),
            ("Puissance (W)",  "puissance_W"),
            ("Shunt (mV)",     "tensionShunt_mV"),
            ("Temp. Amb (°C)", "temperature_C"),
            ("Temp. Bat (°C)", "temperaturebatterie_C")
        ]
        
        for nom, cle in mapping:
            val_debut = f"{moy_debut[cle]:.2f}" if moy_debut and cle in moy_debut else "—"
            val_recent = f"{moy_recent[cle]:.2f}" if moy_recent and cle in moy_recent else "—"
            self.tree.insert("", "end", values=(nom, val_debut, val_recent))

    # ---- Mise à jour stats ----------------------------------------------------------------------------------------------------------
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


# ------ Lancement --------------------------------------------------------------------------------------------------------------------------------
if __name__ == "__main__":
    app = AppBatteries()
    app.mainloop()
