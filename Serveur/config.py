# ─── Configuration générale ───────────────────────────────────────────────────
FICHIER_DB = "batteries.db"
REFRESH_MS = 5000  # rafraîchissement toutes les 5 secondes

# ─── Seuils d'alerte ──────────────────────────────────────────────────────────
SEUILS = {
    "tensionBus_V":          (10.0, 14.8),
    "temperature_C":         (0.0,  1200.0),
    "temperaturebatterie_C": (0.0,  45.0),
    "courant_A":             (-10.0, 15.0),
}

# ─── Couleurs ─────────────────────────────────────────────────────────────────
COULEUR_OK     = "#2ecc71"
COULEUR_ALERTE = "#e74c3c"
COULEUR_FOND   = "#1e1e2e"
COULEUR_PANEL  = "#2a2a3e"
COULEUR_TEXTE  = "#cdd6f4"
COULEUR_TITRE  = "#89b4fa"
COULEUR_HEADER = "#313244"
