# Projet salle blanche

## Introduction:
Ce projet a été réalisé dans le cadre d'un stage de 4ème année d'école d'ingénieurs (niveau Master 1) à l'École de Technologie Supérieure (ÉTS) de Montréal, par un étudiant de Polytech Grenoble.

L'objectif de ce système est de monitorer en continu les salles blanches de l'ÉTS, en suivant l'évolution de la concentration des particules de poussière, ainsi que la température et l'humidité ambiante.

L'architecture centralisée repose sur un serveur Raspberry Pi 4B+ et des microcontrôleurs ESP32 (DevKit V1). Les données récoltées alimentent une page web interne permettant la visualisation des mesures en temps réel.



## Présentation Hardware
Le déploiement standard comprend 4 modules par salle blanche :

 - 3 Modules "Esclaves" : Dédiés à l'acquisition des mesures environnementales et à la transmission sans fil.

- 1 Module "Maître" : Effectue les mêmes mesures que les esclaves, mais agit également comme une passerelle (Gateway). Il centralise les données de tous les modules de la salle, les affiche sur un écran local, et les transmet au serveur Raspberry Pi.

### Capteurs utilisés

 - Sensirion SPS30 : Capteur optique permettant de mesurer le nombre et la taille des particules de poussière (PM) dans l'air.

- Sensirion SHT40 : Capteur haute précision pour mesurer la température et l'humidité relative.

Note : Les fichiers de conception des PCB permettant de relier proprement tous ces composants sont disponibles dans ce dépôt.

## Protocole de communication
L'architecture réseau est conçue pour être robuste et isolée :

 ->  Réseau Local Capteurs (ESP-NOW) : Les modules esclaves communiquent avec le module maître via ESP-NOW. Ce protocole bas niveau spécifique à Espressif permet des transferts de données quasi instantanés avec une très faible consommation énergétique.

-> Pont vers le Serveur (Wi-Fi) : L'ESP32 maître utilise une connexion Wi-Fi standard pour transférer les données agrégées vers la Raspberry Pi.

-> Accès Utilisateur (Ethernet) : La Raspberry Pi est directement reliée au réseau interne de l'école via son port Ethernet pour héberger et servir la page web de monitoring en toute sécurité.

## Partie logiciel
L'ensemble du code embarqué a été développé en C++ via PlatformIO sur l'éditeur VSCode.


Une libraire complète pour utiliser les capteurs de sensirion développer par mes soins est disponible dans les fichier I2C.cpp et I2C.h, la libraire de l'écran et celle du frabicant DFRobot. 

# Notice d'utilisation: 