# Electronic : état vérifié du projet

Date : 27 octobre 2026. Sources : fichiers locaux et essais archivés des 24–26 octobre.
Les adresses IP et ports cités dans les anciens rapports sont historiques.

## Intention reconstituée

Consulter des mesures de température et pression sur le réseau domestique,
avec une sonde pouvant transmettre au récepteur par LoRa. Réutiliser les
fonctions de configuration et de diagnostic pour de futurs objets connectés.

| Ensemble | Fonction existante | Référence |
| --- | --- | --- |
| ESP32-C3 sonde | BMP180 toutes les 2 s ; Wi-Fi/BLE ; émission LoRa selon profil | `firmware/EnviroTerminalDiagnostic/`, `platformio.ini` |
| ESP32-C3 récepteur | Réception 433,92 MHz, validation JSON, tableau de bord local | `firmware/WeatherReceiver/` |
| Pilote SX127x | Copie MIT Arduino-LoRa modifiée localement | `lib/LoRa169/README.md` |
| Heltec V3 | Diagnostic RX 868,1 MHz et veille PRG/RST | `firmware/PlantHubDiagnostic/` |
| Outils | Captures série, observation radio, provisioning et prévisualisation | `tools/` |
| Preuves | Rapports, journaux série et captures JSON | `diagnostics/` |

Le nom historique `LoRa169` ne désigne pas la fréquence actuellement utilisée.
La station météo utilise 433,92 MHz ; le diagnostic Heltec utilise 868,1 MHz.
Ces deux liaisons ne sont pas interopérables avec leurs configurations actuelles.

## Ce que les preuves établissent

- BMP180 : identification et mesures valides documentées après reprise du câblage.
- Météo : Wi-Fi et échanges LoRa observés sur les deux C3 ; après passage à
  8 dBm nominaux, deux paquets successifs à environ 7 m derrière un mur,
  RSSI −119 dBm, sans nouvelle erreur CRC pendant cette observation.
- Interface : masquage des mesures distantes à partir de 180 000 ms ; état
  local et indisponibilité réseau couverts par le test JavaScript existant.
- Heltec : initialisation RX et séquences de veille/reprise documentées.
  Aucun émetteur XIAO n'est encore programmé selon son README.
- Logiciel : les trois compilations et les trois fichiers de tests ont été
  vérifiés de nouveau le 27 octobre, voir le rapport de validation BMAD.

Sources principales : `diagnostics/lora-power-8dbm-20261026.md`,
`diagnostics/heltec-v3-rx-20261026.md`,
`diagnostics/heltec-v3-sleep-20261026.md`.

## Limites ouvertes

1. La fiabilité LoRa sur une longue durée n'est pas mesurée par les deux
   paquets du dernier essai. L'environnement radio et les antennes restent à qualifier.
2. Le LCD conserve un historique d'incidents de câblage et de niveaux logiques.
   Il est désactivé dans le profil météo Wi-Fi/LoRa courant.
3. Le courant de veille Heltec n'est pas mesuré ; aucun chiffre d'autonomie
   ne peut être déduit de la seule séquence logicielle de sommeil.
4. Les tests du parseur JSON exécuté par le récepteur ne sont pas isolés.
5. La future application multiappareils et le produit Plant Station complet
   restent des perspectives ; leurs besoins ne sont pas définis par le code.
6. `WEB.md` mélange un historique sans émission et les profils TX ajoutés ensuite.
   Pour connaître le comportement, consulter le profil compilé et les sources.
7. Git est initialisé sans commit ; il n'existe pas encore de référence versionnée.

## Principes de reprise

Conserver les profils matériels séparés, les paramètres météo communs et la
validation des mesures. Conserver la distinction entre observation, hypothèse
et objectif de campagne. Les secrets BLE et sauvegardes restent dans
`.tools/private/`, ignoré par Git ; ils n'ont pas été lus pour cette analyse.
