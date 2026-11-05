# Plant Station — écran OLED et luminosité, 28 octobre 2026

Historique OLED : la version actuelle **1.2.0 avec veille PRG** est décrite dans le [rapport de veille](plant-station-veille-20261028.md).

Version **1.1.0**, profil `heltec_v3_text`, installée sur Heltec COM5 (ESP32-S3 révision0.2, MAC3c:0f:02:eb:f5:28). Le signalement de l'écran noir est corrigé par l'initialisation de l'OLED et l'affichage des lux chaque seconde.

## Configuration

- VEML7700 inchangé : Wire / contrôleur0, GPIO4/5, adresse0x10, alimentation3V3.
- OLED : contrôleur1 indépendant, GPIO17/18, adresse0x3C, reset21, Vext36 actif bas.
- Titre Plant Station, lux à une décimale, uptime64bits ; les erreurs remplacent les anciens lux.
- Une panne OLED laisse les mesures actives ; reprise écran au plus toutes les10s. `PLANT_OLED=0` désactive son initialisation.
- LoRa et Wi-Fi restent non initialisés dans ce diagnostic. Le Heltec reste le hub prévu pour les futures sondes d'humidité LoRa868MHz ; leur réception est à intégrer.

## Vérifications logicielles

Deux profils texte/JSON compilés : RAM18984/327680, flash297101/3342336 octets. Versions : Espressif32 6.12.0, SparkFun VEML7700 2.0.1, Toolkit1.2.1, Adafruit SSD1306 2.5.13, GFX1.13.0 et BusIO1.17.4.

Tests hôte exécutés avec succès : vrai pilote, vrai main et vrai module OLED avec interfaces matérielles simulées. 62 lignes JSON contrôlées, scénarios normal/wrap/OLED absent/mode désactivé, affichage valide et erreurs sans valeur périmée, GPIO36/21 et ordre des délais, reprise après NACK et uptime64bits. Revue indépendante effectuée ; ses cinq constats ont été traités et vérifiés.

[Builds](plant-station-oled-builds-20261028.log) · [Tests](plant-station-oled-tests-20261028.log).

## Téléversement et matériel

BIN texte précédent v1.0.0 conservé dans `.tools/private/plantstation-veml-v1.0.0-pre-oled-20261028.bin`. Sauvegarde complète du firmware RX antérieur également conservée, décrite dans le rapport VEML7700.

Téléversement réussi en12,025s ; hachages des blocs vérifiés par esptool. [Journal](plant-station-oled-upload-20261028.log).

Capture15s commencée à08:56:21 UTC /09:56:21 à Paris :

- OLED acquitté à0x3C et initialisation confirmée à359ms ; capteur acquitté à0x10.
- **14 mesures**, zéro erreur, de **56.9856 à 504.8064 lux**.
- Écart exact1000ms entre mesures, de1397 à14397ms. Dernière valeur : 439.7568 lux.
- Variation lumineuse observée pendant la capture ; sa cause n'a pas été rapportée par l'utilisateur. Aucun étalonnage effectué.

[Journal matériel](plant-station-oled-hardware-20261028.log) · [Données](plant-station-oled-hardware-20261028.json).

L'initialisation matérielle et les acquisitions sont vérifiées. Le rendu a été testé en simulation ; la lisibilité physique de l'écran reste à confirmer visuellement par l'utilisateur. Aucun essai physique de panne/reprise OLED n'a été effectué.
