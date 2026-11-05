# EnviroTerminal : un objet connecte sur le Wi-Fi habituel

**Recepteur LoRa ajoute le 25 octobre 2026 :** le meme tableau de bord est
maintenant installe sur la seconde carte COM4. Il affiche les mesures distantes,
recues toutes les deux minutes, et les masque apres trois minutes sans paquet
valide. Configuration Bluetooth : `PROV_Enviro_78b4b4` ; nom local une fois
connecte : `enviro-78b4b4.local`. Voir
[le projet recepteur](firmware/WeatherReceiver/README.md).
La sonde COM3 conserve son site et ajoute l'emission via le profil
`enviroterminal_local_web_meteo_tx`.
Le recepteur est maintenant connecte a la box :
**http://192.168.1.197/**. API et page reelles verifiees avec des mesures LoRa
valides (24,6 degres C et 1007,17 hPa, quatre mesures recues, zero erreur).

L'ESP32 rejoint la box et le telephone reste sur son reseau habituel.
Le profil `enviroterminal_local_web` n'ouvre aucun point d'acces Wi-Fi.
La configuration initiale se fait par Bluetooth avec le protocole officiel
Espressif, puis les mesures se consultent dans le navigateur sur le reseau local.

## Configuration initiale

1. Installer **ESP BLE Provisioning**, l'application Espressif disponible
   sur [Android](https://play.google.com/store/apps/details?id=com.espressif.provble)
   ou [iPhone](https://apps.apple.com/in/app/esp-ble-provisioning/id1473590141).
2. Activer le Bluetooth et ouvrir l'application. Le telephone garde sa
   connexion Wi-Fi habituelle pendant cette operation.
3. Scanner le QR de configuration Bluetooth fourni localement, ou choisir
   `PROV_Enviro_81ca70` et saisir le code de possession fourni avec la carte.
   Ce code est propre a la carte et conserve apres redemarrage.
4. Choisir le Wi-Fi **2,4 GHz de la box** dans la liste de l'application,
   puis saisir le mot de passe. Les identifiants sont transmis a la carte
   par la session Bluetooth securisee ; ne pas les envoyer dans le chat.
5. Une fois la carte connectee, ouvrir **http://enviro-81ca70.local/**.
   Si mDNS n'est pas disponible sur le telephone, utiliser l'adresse IP
   affichee dans le diagnostic serie. L'application ne remplace pas encore
   le tableau de bord : elle sert a configurer la connexion.
6. Ajouter la page a l'ecran d'accueil pour acceder aux mesures ensuite.

Aux demarrages suivants, la carte rejoint automatiquement le Wi-Fi enregistre.
Le nom local reste identique meme si l'adresse IP change. Apres 30 secondes
sans connexion, le service de configuration Bluetooth devient disponible.

Pour changer de box : une fois la carte demarree, maintenir **BOOT pendant
3 secondes**, puis refaire la configuration Bluetooth. Ne pas tenir BOOT
pendant la mise sous tension, car cela lance le mode de televersement.
Apres un mauvais mot de passe de box, la carte attend 8 secondes pour
laisser l'application afficher le refus, puis ferme et recree la session
Bluetooth avec le meme nom et le meme code de possession. Revenir a l'ajout
d'appareil dans l'application et rescanner le QR pour recommencer.
BOOT maintenu 3 secondes relance aussi une session deja active.
Si l'arret du transport reste bloque 10 secondes, la carte memorise une
reprise Bluetooth et redemarre automatiquement, sans changer le code du QR.

Le QR contient le code de possession, pas le mot de passe de la box.
Il est genere localement dans `.tools/private/ble-setup-qr.png` ; les
journaux contenant ce code restent aussi dans `.tools/private/` (ignore par Git).

## Mesures et interface

- Temperature en degres C et pression brute en hPa, sans correction d'altitude.
- Cadence nominale de 2 secondes et courbes des 60 derniers points de la session.
- Valeurs masquees lors d'une erreur capteur ou d'une connexion interrompue.
- Controle SPI du SX127x toutes les 10 secondes. Il reste en veille ;
  aucune liaison radio LoRa n'est encore configuree.
- Page autonome sans service, police ou script externe. Acces HTTP local,
  reserve au reseau domestique de confiance ; aucune ouverture de port Internet.

Brochage : BMP180 SDA=4 / SCL=3 ; LoRa SCK=6, MISO=5, MOSI=7,
NSS=10, RESET=0 et DIO0=1 facultatif ; alimentation 3,3 V. LCD desactive.

## Base commune pour les futurs projets

Chaque firmware compatible doit publier :

- Un identifiant stable : ici `e83dc181ca70`.
- Un nom local propre a la carte : ici `enviro-81ca70.local`.
- Le service de decouverte **`_enviro._tcp.local`**, port 80, avec les
  champs TXT `id`, `api=1`, `path=/api/device` et `project`.
- **GET `/api/device`** : protocole `enviroterminal`, version de l'API,
  identifiant, nom du projet, capacites, nom local et chemin des mesures.
- **GET `/api/status`** : valeurs, age de la mesure, etat des capteurs et du reseau.
  Aucun mot de passe ni code de possession n'est expose dans ces API.

Une future application native pourra decouvrir les appareils compatibles,
les enregistrer et afficher leurs fonctions selon leurs capacites. Elle
pourra reutiliser la configuration Bluetooth Espressif. Cette application
multiappareils n'est pas encore developpee. Une page web ordinaire ne peut
pas effectuer directement une decouverte mDNS complete des appareils.

La decouverte exige un reseau local commun autorisant les communications
entre appareils. Elle ne retrouve pas automatiquement n'importe quel ESP32
sans firmware compatible. Les futurs capteurs LoRa distants devront
passer par un recepteur relie au reseau ; voir [LORA.md](LORA.md).

## Compilation

```powershell
$env:PYTHONPATH = Join-Path $PWD '.tools\python'
$env:PLATFORMIO_CORE_DIR = Join-Path $PWD '.tools\platformio'
python -m platformio run -e enviroterminal_local_web -t upload --upload-port COM3
```

La partition `no_ota.csv` reserve 2 Mio a l'application pour Wi-Fi + BLE.
Pas de mise a jour OTA dans cette version. La zone NVS conserve les
identifiants de connexion. Les identifiants de la premiere version du
formulaire web sont importes une seule fois si necessaire.

Validation : compilation ESP32-C3 reussie (application environ 1,55 Mo),
interface navigateur verifiee avec des donnees de test explicites.
Firmware installe sur COM3 : service Bluetooth demarre, 23 mesures sans
erreur en 45 secondes et quatre controles SPI LoRa reussis. Journal sans
code de possession : `diagnostics/connected-object-ble-startup.log`.
Apres confirmation du mot de passe accepte sur Android, la connexion a la
box et l'acces HTTP reel ont ete verifies le 24 octobre 2026 :
`/api/status` annonce `wifi_connected=true`, BMP180 et SPI LoRa OK.
La carte utilise alors `192.168.1.65` ; le nom stable reste
`enviro-81ca70.local`. La page reelle a ete ouverte et son actualisation
des mesures a ete constatee (23,5 degres C, pression autour de 1001,7 hPa).

Sources : [provisioning Espressif Android](https://github.com/espressif/esp-idf-provisioning-android),
[provisioning Espressif iOS](https://github.com/espressif/esp-idf-provisioning-ios).

Correction de reprise du 24 octobre 2026 : compilation et installation
reussies ; deux fermetures et recreations successives du service BLE
confirmees sur COM3 avec `tools/check_ble_recovery.py`.
Journal sans code de possession : `diagnostics/ble-session-recovery.log`.
Ce test valide le cycle d'arret/reprise du service sur la carte. L'utilisateur
a ensuite confirme le mot de passe accepte et l'acces au tableau de bord
sur le reseau de la box a ete verifie.
Les commandes USB `wifi-setup` et `wifi-status` permettent respectivement
de relancer la configuration et de lire l'etat sans afficher de mot de passe.
