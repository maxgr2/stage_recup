# Projet de Surveillance de Batteries

## Introduction 

Ce projet a été réalisé dans le cadre d'un stage de 4ème année d'école d'ingénieurs à l'**École de Technologie Supérieure (ÉTS) de Montréal**, au sein du laboratoire **LACIME**, par un étudiant de **Polytech Grenoble**.

L'objectif de ce système est de développer une preuve de concept (PoC) pour la surveillance de parcs de batteries à budget limité (objectif d'environ 10$ par batterie). Le module est conçu pour inspecter les batteries lors de phases de stockage à long terme dans des entrepôts. 

Pour assurer la sécurité et le suivi, le système mesure :
- La tension de chaque cellule (en charge et à vide).
- L'impédance/courant via une résistance de shunt.
- La température (externe) de chaque batterie.

Si des valeurs critiques sont observées, le module transmet instantanément une alerte sans fil à un ordinateur central. Le système est conçu pour être ultra-basse consommation, s'alimente directement sur les batteries surveillées (plage de tension acceptée : 8V à 65V), et utilise le mode *Deep Sleep* de l'ESP32 entre deux sessions de mesure.

---

## Présentation Hardware

L'architecture matérielle repose sur trois piliers :
1. **Un ordinateur central (Serveur) :** Collecte les données envoyées sans fil et gère l'affichage/historique (fichiers disponibles dans le dossier `Serveur`).
2. **Une carte principale (Master) :** Intègre l'intelligence (ESP32) et le bloc de conversion analogique-numérique principal (`INA237`). Elle permet de surveiller nativement jusqu'à 10 batteries.
3. **Des cartes secondaires (Filles) :** Permettent d'étendre la capacité du système. Chaque carte fille permet de multiplexer et surveiller des batteries supplémentaires en partageant l'ADC central via le bus I2C.

### Fonctionnement général et Multiplexage
Pour minimiser les coûts, **un unique circuit intégré INA237** est positionné sur la carte principale. Le système utilise des expandeurs (`MCP23017`) pour piloter des relais statiques (SSR - `ISOM8600DFGR`). Ces relais permettent de connecter une seule batterie à la fois sur l'INA237 pour effectuer la mesure, isolant complètement les autres canaux. Les ADC de l'esp sont utilisé pour faire la lecture de température tandis que pour la carte fille il s'agit d'un ADC to I2C dédiées.

L'interconnexion entre la carte principale et les cartes filles s'effectue par le bus I2C, la carte principale pouvant accueillir plusieurs cartes secondaires (limité uniquement par l'adressage matériel I2C du MCP23017).

Les fichiers de conception électronique (schématiques et routages) sont disponibles dans le dossier contenant le **projet KiCad**.

### Liste des composants principaux
* **Microcontrôleur :** ESP32 DevKit V1 (30 pins)
* **Mesure Électrique (ADC) :** INA237 (un seul composant centralisé)
* **Gestion du Multiplexage :** Extenseur d'E/S I2C MCP23017-E/SO
* **Isolation / Commutation :** Relais statiques / Optocoupleurs ISOM8600DFGR
* **Alimentation (Buck 70V -> 5V) :** Référence 173950575 de chez Würth Elektronik
* **Régulateur LDO (5V -> 3.3V) :** TLV75533PDYDR
* **Capteurs de Température :** TMP6131QLPGMQ1 (thermistance PTC linéaire installée sur chaque batterie)
* **Conditionnement Température :** Ponts diviseurs de tension de précision (10 kΩ, 0.25W) adossés à une tension de référence ($V_{ref}$) stabilisée et mesurée en amont par l'ESP32.
* **Shunt de courant :** 1 résistance de 150 mΩ (1W)
* **Résistance de charge :** 1 résistance de 2.2 kΩ

---

## Présentation du Code Architecture

### Structure du Logiciel (Firmware ESP32)
Le code embarqué est modulaire et structuré de la manière suivante :

* **`main.cpp` :** Point d'entrée du programme. Gère l'initialisation des périphériques, la machine à états de mesure séquentielle (boucle sur les batteries locales puis sur les cartes filles), l'invocation des transmissions Bluetooth, et la mise en veille profonde (*Deep Sleep*) de l'ESP32.
* **`I2C.cpp` / `I2C.h` :** Contient les pilotes bas niveau pour le bus I2C. Gère la configuration des registres de l'INA237 (étalonnage du shunt, configuration de l'ADC) ainsi que l'écriture/lecture sur le MCP23017 pour l'activation sélective des relais SSR.
* **`Temperature.cpp` / `Temperature.h` :** Détermine la température de chaque batterie. Le code lit la tension aux bornes de la thermistance PTC via l'ADC interne de l'ESP32, applique la formule du pont diviseur par rapport à la tension $V_{ref}$ lue sur le GPIO 39, puis linéarise le comportement du capteur TMP6131.
* **`bluetooth.cpp` / `bluetooth.h` :** Implémente le protocole de communication sans fil.

### Format de la Trame Bluetooth (Payload BLE)
Pour réduire drastiquement la consommation d'énergie, les données ne sont pas envoyées via une connexion Bluetooth classique, mais encapsulées directement dans des paquets d'**Advertising BLE** (mode *Broadcaster*).

Chaque message de mesure est optimisé pour tenir dans un payload compact de **19 octets**, structuré ainsi :

| Taille | Description | Format / Rôle |
| :--- | :--- | :--- |
| **2 octets** | ID Fabricant | `0xFFFF` (Champ requis pour le format constructeur) |
| **4 octets** | ID Unique Chip | Dérivé de l'adresse MAC unique de l'ESP32 |
| **1 octet** | Numéro de Batterie | Index de la batterie mesurée (ex: 1 à 10, ou index fille) |
| **12 octets** | Données Capteurs | 6 variables au format `int16_t` (2 octets chacune) avec mise à l'échelle |

**Détail de la mise à l'échelle des données (12 octets) :**
1. **Tension Bus (`tensionBus_V`) :** Facteur $\times 100$ (Précision de 0.01 V)
2. **Courant (`courant_A`) :** Facteur $\times 1000$ (Précision de 0.001 A)
3. **Puissance (`puissance_W`) :** Facteur $\times 10$ (Précision de 0.1 W)
4. **Tension Shunt (`tensionShunt_mV`) :** Facteur $\times 100$ (Précision de 0.01 mV)
5. **Température Interne (`temperature_C`) :** Facteur $\times 10$ (Précision de 0.1 °C)
6. **Température Batterie (`temperaturebatterie_C`) :** Facteur $\times 10$ (Précision de 0.1 °C)
7. **Tension en Charge (`tensionBus_charge_V`) :** Facteur $\times 100$ (Précision de 0.01 V)

---
## Fonctionnement du Serveur

Le serveur reçoit les données transmises par les cartes, les stocke dans une base de données locale (SQLite) et les affiche sur une interface graphique en temps réel.

### Prérequis Logiciels
Il est impératif que la machine hébergeant le serveur possède **Python** 

La majorité des modules utilisés (comme `sqlite3`, `tkinter`, `asyncio` ou `struct`) sont inclus nativement dans Python. Cependant, certaines bibliothèques externes doivent être ajoutées pour garantir le bon fonctionnement de la réception Bluetooth et de l'affichage des graphiques.

Il est cependant nécassiare d'avoir pandas, matplotlib et bleak (librairies qui ne sont par installé par défault lors de l'instalation de pyhton). Vous pouvez installer toutes les dépendances nécessaires via la commande suivante dans votre terminal :

```bash
pip install pandas matplotlib bleak
```
Une fois l'installation réalisé il est important de lancer le fichier `main.py`(s'occupe d'écouter le bluetooth et de stocker les données sur le fichier sql (batterie.db)) et `fenetre2.py` (c'est simplement l'interface graphique)

---

## Installation et Utilisation

### Prérequis Logiciels
* **IDE / Environnement :** [PlatformIO](https://platformio.org/) (recommandé) ou Arduino IDE v2.
* **Framework :** Arduino pour ESP32.
* **Bibliothèques requises :** * `Wire` (incluse nativement pour la gestion I2C)
  * `BLEDevice` (incluse nativement pour le support Bluetooth)

### Compilation et Flash
1. Clonez le dépôt GitHub sur votre machine.
2. Ouvrez le dossier racine avec votre IDE (PlatformIO dans Vscodes ont été utilisé pour le développement).
3. Connectez l'ESP32 en USB.
4. Modifiez la constante `nb_cartefille` dans le fichier `main.cpp` si vous utilisez des modules d'extension, il important de préciser les adresses I2C de chaque module (voir parti carte fille).
5. Compilez et téléversez le programme.
6. Il est essentiel que les batteries qui alimente le modules ai toute la même tension d'origine (les 6 emplacements qui n'aliemnte pas le module pevent toute avoir un tension différentes)

---
## Carte Principale

La carte principale fonctionne comme expliqué précédemment au travers de relais statiques (SSR) et du module unique INA237. Lors du démarrage, le programme choisit de manière aléatoire les batteries valides celle qui va alimenter le système afin d'équilibrer la décharge du bloc d'alimentation.

Le déroulement exact d'une session de mesure pour une batterie est le suivant :

1. **Ouverture de tous les relais de mesure (`ssrOff`) :** Le système s'assure d'abord qu'aucune ligne de mesure n'est active pour isoler complètement les batteries et éviter les courts-circuits.
2. **Fermeture du relais de la batterie concernée (`ssrOn`) :** Le relais `CONT_MES` spécifique à la batterie s'active. Après un court délai de stabilisation (500 ms), l'INA237 effectue la mesure de la **tension à vide** ($V_{bus}$).
3. **Mise en charge et mesure de puissance :** Si la batterie est détectée, le système ferme le relais de charge globale (`MES_TOT`, SSR index 8). L'INA237 mesure alors le courant ($A$), la tension de shunt ($mV$) et la **tension en charge**. Le relais de charge est ensuite immédiatement réouvert par sécurité.
4. **Mesure de la température :** Le système active la ligne d'alimentation des capteurs (`3.3V_temp`, SSR index 15) et effectue une boucle de lecture de la thermistance PTC 10 fois d'affilée afin d'en calculer une moyenne stable.

*Note importante : Par sécurité, la batterie qui alimente actuellement le module principal n'est jamais mesurée en même temps.*

---

## Carte Secondaire (Carte Fille)

La carte secondaire se relie à la carte principale via un connecteur à 5 broches (ou pins en bout de carte) qui comprend :
- **GND :** Masse commune.
- **3.3V :** Alimentation des composants.
- **SDA / SCL :** Les deux lignes du bus I2C pour la communication.
- **V_bus :** La ligne analogique partagée qui renvoie la tension de la batterie sélectionnée vers l'`INA237` de la carte principale.


Pour fonctionner de manière autonome sur le bus, chaque carte fille embarque deux circuits intégrés distincts :
1. **Un Expandeur d'E/S (`MCP23017`)** : Pilote les relais statiques (SSR) de la carte.
2. **Un ADC 16 bits (`LTC2497`)** : Convertit avec haute précision la tension de la batterie sélectionnée.

Le système peut accueillir jusqu'à **7 cartes filles** simultanément sur le même bus I2C (limite physique d'adressage binaire du MCP23017, l'adresse `0x20` étant réservée à la carte principale).


### Plan d'Adressage I2C (Configuration Matérielle)

Pour que l'ordinateur central et l'ESP32 puissent interroger chaque carte individuellement, vous devez configurer physiquement leurs adresses via les ponts de soudure prévu à cet effet. 

Le **MCP23017** utilise une logique binaire standard (`0` = GND, `1` = VCC), tandis que le **LTC2497** exploite ses broches à **3 états** (`0` = GND, `1` = VCC, `F` = Flottant / Non connecté) pour générer leir adresses. L'adresse `Ox44` est utilisé par le `INA237`.
Voici une propostion de configuration pour utiliser plusieurs cartes filles :

| Module | Pins Expandeur (A2, A1, A0) | Adresse MCP23017 | Pins ADC (CA2, CA1, CA0) | Adresse LTC2497 |
| :---: | :---: | :---: | :---: | :---: |
| *Master* | `0 0 0` | `0x20` | aucun | aucun |
| **Fille n°1** | `0 0 1` | `0x21` | `0 0 0` | `0x14` |
| **Fille n°2** | `0 1 0` | `0x22` | `0 0 F` | `0x15` |
| **Fille n°3** | `0 1 1` | `0x23` | `0 0 1` | `0x16` |
| **Fille n°4** | `1 0 0` | `0x24` | `0 F 0` | `0x17` |
| **Fille n°5** | `1 0 1` | `0x25` | `0 F F` | `0x18` |
| **Fille n°6** | `1 1 0` | `0x26` | `0 F 1` | `0x19` |
| **Fille n°7** | `1 1 1` | `0x27` | `0 1 0` | `0x1A` |

*(Légende : `0` = relié au GND | `1` = relié au 3.3V | `F` = Flottant / Pin laissée déconnectée)*

#### Règles de mise en œuvre :
 **Déclaration logicielle :** Une fois vos cartes filles adressées selon ce tableau, veillez à reporter le nombre de cartes actives dans la variable `nb_cartefille` du fichier `main.cpp`.