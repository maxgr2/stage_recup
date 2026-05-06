import socket
import struct

SERVER_IP = "10.240.193.115"
SERVER_PORT = 4210
TAILLE_STRUCT = 6 * 4  # 6 floats × 4 octets

def decoder_payload(raw_bytes: bytes) -> dict | None:
    if len(raw_bytes) != TAILLE_STRUCT:
        print(f"Taille inattendue : {len(raw_bytes)} octets")
        return None

    valeurs = struct.unpack("<6f", raw_bytes)
    return {
        "tensionBus_V":          valeurs[0],
        "courant_A":             valeurs[1],
        "puissance_W":           valeurs[2],
        "tensionShunt_mV":       valeurs[3],
        "temperature_C":         valeurs[4],
        "temperaturebatterie_C": valeurs[5],
    }

def afficher_donnees(data: dict, adresse: str):
    print(f"\n{'─' * 40}")
    print(f"  ESP32 → {adresse}")
    print(f"{'─' * 40}")
    print(f"  Tension bus    : {data['tensionBus_V']:.3f} V")
    print(f"  Courant        : {data['courant_A']:.3f} A")
    print(f"  Puissance      : {data['puissance_W']:.3f} W")
    print(f"  Tension shunt  : {data['tensionShunt_mV']:.3f} mV")
    print(f"  Température    : {data['temperature_C']:.1f} °C")
    print(f"  Temp. batterie : {data['temperaturebatterie_C']:.1f} °C")
    print(f"{'─' * 40}")

def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((SERVER_IP, SERVER_PORT))
    print(f"Serveur UDP en écoute sur le port {SERVER_PORT}... (Ctrl+C pour arrêter)")

    try:
        while True:
            raw_bytes, adresse = sock.recvfrom(1024)
            donnees = decoder_payload(raw_bytes)
            if donnees is not None:
                afficher_donnees(donnees, adresse[0])
    except KeyboardInterrupt:
        print("\nArrêt propre.")
    finally:
        sock.close()

if __name__ == "__main__":
    main()