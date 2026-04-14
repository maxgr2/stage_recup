/**
 * @file ble_helpers.h
 * @author Nicolas Boulanger
 * @author Maxime Goncalves
 * @author Dominique Richer
 * @author Emilie Croteau
 * @date 2025-07-28
 * @brief Contient les fonctions pour interagir le module bluetooth.
 *        Gestion de la connection et de génération du message d'advertisement.
 * @defgroup BLE_HELPERS BLE helper functions
 * @copyright
 */
#ifndef BLE_HELPERS_H
#define BLE_HELPERS_H

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

/***********************************************************************
 * @brief Fonction initialiser les parametres BLE
 * @param None
 * @return None
 ***********************************************************************/
void ble_init(void);

/***********************************************************************
 * @brief Fonction pour vérifier le status de connexion et redémarrer 
 *        l'advertisement
 * @param None
 * @return None
 ***********************************************************************/
void ble_check_connection();

/***********************************************************************
 * @brief Fontion pour généré le json a envoyer dans l'advertisement.
 * @param pCharacteristic BLECharacteristic Ptr vers characteristique BLE 
 * @return None
 ***********************************************************************/
void jsonAdd(BLECharacteristic *pCharacteristic);

#endif //BLE_HELPERS_H
