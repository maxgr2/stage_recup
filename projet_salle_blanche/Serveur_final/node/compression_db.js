const Database = require("better-sqlite3");
const path = require("path");

// Chemin vers ta base de données (utilise la même variable d'environnement que server.js)
const DB_PATH = process.env.DB_PATH || "./mesures.db";
const db = new Database(DB_PATH);

console.log(`[${new Date().toISOString()}] Démarrage de la compression de la base de données...`);

// Utilisation d'une transaction pour s'assurer que si une erreur survient, aucune donnée n'est perdue
const compressData = db.transaction(() => {
    
    // ---------------------------------------------------------
    // ÉTAPE 1 : Compression à 5 minutes (pour les données entre 1 et 7 jours)
    // 300 secondes = 5 minutes
    // ---------------------------------------------------------
    console.log("-> Compression des données de 1 à 7 jours (résolution 5 min)...");
    
    db.exec(`
        CREATE TEMP TABLE IF NOT EXISTS avg_5m AS
        SELECT 
            datetime((CAST(strftime('%s', timestamp) AS INTEGER) / 300) * 300, 'unixepoch') AS timestamp,
            room, slave,
            ROUND(AVG(temperature), 2) as temperature, ROUND(AVG(humidity), 2) as humidity,
            ROUND(AVG(mc1p0), 2) as mc1p0, ROUND(AVG(mc2p5), 2) as mc2p5, 
            ROUND(AVG(mc4p0), 2) as mc4p0, ROUND(AVG(mc10p0), 2) as mc10p0,
            ROUND(AVG(nc0p5), 2) as nc0p5, ROUND(AVG(nc1p0), 2) as nc1p0, 
            ROUND(AVG(nc2p5), 2) as nc2p5, ROUND(AVG(nc4p0), 2) as nc4p0, 
            ROUND(AVG(nc10p0), 2) as nc10p0, ROUND(AVG(tps), 2) as tps
        FROM mesures
        WHERE timestamp <= datetime('now', '-1 day') 
          AND timestamp > datetime('now', '-7 days')
        GROUP BY (CAST(strftime('%s', timestamp) AS INTEGER) / 300), room, slave
    `);

    db.exec(`DELETE FROM mesures WHERE timestamp <= datetime('now', '-1 day') AND timestamp > datetime('now', '-7 days')`);

    db.exec(`
        INSERT INTO mesures (timestamp, room, slave, temperature, humidity, mc1p0, mc2p5, mc4p0, mc10p0, nc0p5, nc1p0, nc2p5, nc4p0, nc10p0, tps)
        SELECT timestamp, room, slave, temperature, humidity, mc1p0, mc2p5, mc4p0, mc10p0, nc0p5, nc1p0, nc2p5, nc4p0, nc10p0, tps FROM avg_5m
    `);
    db.exec(`DROP TABLE avg_5m`);

    // ---------------------------------------------------------
    // ÉTAPE 2 : Compression à 15 minutes (pour les données de plus de 7 jours)
    // 900 secondes = 15 minutes
    // ---------------------------------------------------------
    console.log("-> Compression des données de plus de 7 jours (résolution 15 min)...");
    
    db.exec(`
        CREATE TEMP TABLE IF NOT EXISTS avg_15m AS
        SELECT 
            datetime((CAST(strftime('%s', timestamp) AS INTEGER) / 900) * 900, 'unixepoch') AS timestamp,
            room, slave,
            ROUND(AVG(temperature), 2) as temperature, ROUND(AVG(humidity), 2) as humidity,
            ROUND(AVG(mc1p0), 2) as mc1p0, ROUND(AVG(mc2p5), 2) as mc2p5, 
            ROUND(AVG(mc4p0), 2) as mc4p0, ROUND(AVG(mc10p0), 2) as mc10p0,
            ROUND(AVG(nc0p5), 2) as nc0p5, ROUND(AVG(nc1p0), 2) as nc1p0, 
            ROUND(AVG(nc2p5), 2) as nc2p5, ROUND(AVG(nc4p0), 2) as nc4p0, 
            ROUND(AVG(nc10p0), 2) as nc10p0, ROUND(AVG(tps), 2) as tps
        FROM mesures
        WHERE timestamp <= datetime('now', '-7 days')
        GROUP BY (CAST(strftime('%s', timestamp) AS INTEGER) / 900), room, slave
    `);

    db.exec(`DELETE FROM mesures WHERE timestamp <= datetime('now', '-7 days')`);

    db.exec(`
        INSERT INTO mesures (timestamp, room, slave, temperature, humidity, mc1p0, mc2p5, mc4p0, mc10p0, nc0p5, nc1p0, nc2p5, nc4p0, nc10p0, tps)
        SELECT timestamp, room, slave, temperature, humidity, mc1p0, mc2p5, mc4p0, mc10p0, nc0p5, nc1p0, nc2p5, nc4p0, nc10p0, tps FROM avg_15m
    `);
    db.exec(`DROP TABLE avg_15m`);
});

try {
    compressData();
    console.log("-> Opérations SQL réussies. Lancement du VACUUM...");
    
    // Le VACUUM est obligatoire dans SQLite pour réellement libérer 
    // l'espace physique sur le disque après des requêtes DELETE
    db.exec("VACUUM");
    
    console.log(`[${new Date().toISOString()}] Compression terminée avec succès !`);
} catch (err) {
    console.error("Erreur critique lors de la compression :", err);
} finally {
    db.close();
}
