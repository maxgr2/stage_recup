// Backend Node.js — MQTT → SQLite + API REST
const mqtt    = require("mqtt");
const Database = require("better-sqlite3");
const express = require("express");

// ---------------------------------------------------------------------------
// Config (injectée via variables d'environnement docker-compose)
// ---------------------------------------------------------------------------

const MQTT_BROKER = process.env.MQTT_BROKER || "localhost";
const MQTT_PORT   = process.env.MQTT_PORT   || 1883;
const MQTT_TOPIC  = process.env.MQTT_TOPIC  || "ets/salle1/mesures";
const DB_PATH     = process.env.DB_PATH     || "./mesures.db";
const API_PORT    = process.env.API_PORT    || 3000;

// ---------------------------------------------------------------------------
// Base de données SQLite
// ---------------------------------------------------------------------------

const db = new Database(DB_PATH);

// Optimisations pour Raspberry Pi (RAM limitée)
db.pragma("journal_mode = WAL");
db.pragma("synchronous = NORMAL");

db.exec(`
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
`);

// Requête préparée (plus efficace que de la recompiler à chaque insertion)
const insertMesure = db.prepare(`
    INSERT INTO mesures (
        timestamp, room, slave,
        temperature, humidity,
        mc1p0, mc2p5, mc4p0, mc10p0,
        nc0p5, nc1p0, nc2p5, nc4p0, nc10p0,
        tps
    ) VALUES (
        @timestamp, @room, @slave,
        @temp, @hum,
        @mc1p0, @mc2p5, @mc4p0, @mc10p0,
        @nc0p5, @nc1p0, @nc2p5, @nc4p0, @nc10p0,
        @tps
    )
`);

// ---------------------------------------------------------------------------
// Client MQTT
// ---------------------------------------------------------------------------

const mqttClient = mqtt.connect(`mqtt://${MQTT_BROKER}:${MQTT_PORT}`, {
    clientId: "node-backend",
    reconnectPeriod: 5000,   // Reconnexion automatique toutes les 5s
    keepalive: 60,
});

mqttClient.on("connect", () => {
    console.log(`[MQTT] Connecté à ${MQTT_BROKER}:${MQTT_PORT}`);
    mqttClient.subscribe(MQTT_TOPIC, (err) => {
        if (err) console.error("[MQTT] Erreur souscription :", err);
        else     console.log(`[MQTT] Souscrit à : ${MQTT_TOPIC}`);
    });
});

mqttClient.on("reconnect", () => {
    console.warn("[MQTT] Reconnexion en cours...");
});

mqttClient.on("error", (err) => {
    console.error("[MQTT] Erreur :", err.message);
});

mqttClient.on("message", (topic, payload) => {
    try {
        const data = JSON.parse(payload.toString());

        insertMesure({
            timestamp : new Date().toISOString(),
            room      : data.room   ?? null,
            slave     : data.slave  ?? null,
            temp      : data.temp   ?? null,
            hum       : data.hum    ?? null,
            mc1p0     : data.mc1p0  ?? null,
            mc2p5     : data.mc2p5  ?? null,
            mc4p0     : data.mc4p0  ?? null,
            mc10p0    : data.mc10p0 ?? null,
            nc0p5     : data.nc0p5  ?? null,
            nc1p0     : data.nc1p0  ?? null,
            nc2p5     : data.nc2p5  ?? null,
            nc4p0     : data.nc4p0  ?? null,
            nc10p0    : data.nc10p0 ?? null,
            tps       : data.tps    ?? null,
        });

        console.log(`[DB] Mesure insérée — salle ${data.room}, esclave ${data.slave}`);
    } catch (err) {
        console.error("[MQTT] Message invalide :", err.message);
    }
});

// ---------------------------------------------------------------------------
// API REST Express
// ---------------------------------------------------------------------------

const app = express();

// GET /api/mesures?limit=100&room=1&slave=4
app.get("/api/mesures", (req, res) => {
    const limit = parseInt(req.query.limit) || 100;
    const filters = [];
    const params  = [];

    if (req.query.room) {
        filters.push("room = ?");
        params.push(parseInt(req.query.room));
    }
    if (req.query.slave) {
        filters.push("slave = ?");
        params.push(parseInt(req.query.slave));
    }

    const where = filters.length ? `WHERE ${filters.join(" AND ")}` : "";
    const rows  = db.prepare(
        `SELECT * FROM mesures ${where} ORDER BY id DESC LIMIT ?`
    ).all([...params, limit]);

    res.json(rows);
});

// GET /api/mesures/latest — dernière mesure par esclave
app.get("/api/mesures/latest", (req, res) => {
    const where = req.query.room ? "WHERE room = ?" : "";
    const params = req.query.room ? [parseInt(req.query.room)] : [];

    const rows = db.prepare(`
        SELECT * FROM mesures
        WHERE id IN (
            SELECT MAX(id) FROM mesures ${where} GROUP BY slave
        )
        ORDER BY slave
    `).all(params);

    res.json(rows);
});

// GET /api/health
app.get("/api/health", (req, res) => {
    res.json({ status: "ok", broker: MQTT_BROKER, db: DB_PATH });
});

app.listen(API_PORT, () => {
    console.log(`[API] Serveur Express démarré sur le port ${API_PORT}`);
});

// ---------------------------------------------------------------------------
// Arrêt propre
// ---------------------------------------------------------------------------

process.on("SIGTERM", () => {
    console.log("[SYS] Arrêt propre...");
    mqttClient.end();
    db.close();
    process.exit(0);
});
