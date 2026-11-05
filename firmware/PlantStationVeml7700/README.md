# Plant Station — diagnostic VEML7700 / Heltec V3

Le nouveau profil **`heltec_v3_plant_dashboard`** prépare le robot OLED, les quatre
plantes, leur batterie non communiquée, le Wi-Fi domestique avec provisioning BLE
et un historique local de trente jours. Voir le [tableau de bord local](../../docs/plant-dashboard.md)
pour l’utilisation, l’API, le stockage et les limites de vérification.

Le profil optionnel **`heltec_v3_soil_lora`** recoit maintenant **SOIL-01 et
SOIL-02, SOIL-03 et SOIL-04** et affiche simultanement leurs tensions et ages avec les lux.
Leurs etats et peremptions de 90 minutes sont independants. Voir [quatre sondes](../../docs/soil-four-nodes.md).
Les paragraphes historiques ci-dessous decrivent les profils lux texte/JSON.

Les bruts de chaque sonde restent disponibles sur l'UART ; l'écran montre
leurs tensions et âges, et conserve les contrôles PRG.
Voir [profils, protocole, branchements et capture LoRa](../../docs/soil-lora.md).
Les descriptions sans radio ci-dessous concernent les profils historiques
`heltec_v3_text` et `heltec_v3_json`, dont le schema JSON reste identique.

Firmware autonome pour valider la luminosité du futur hub Plant Station. Il lit un VEML7700 en I2C, affiche les lux sur l'OLED du Heltec et publie les mesures sur l'UART0 à 115200 bauds via le pont USB CP2102. Dans les profils lux texte/JSON, Wi-Fi et LoRa ne sont pas initialisés. Le diagnostic RX [PlantHubDiagnostic](../PlantHubDiagnostic/README.md) et les projets météo ESP32-C3 restent distincts.

**Version 1.2.0 avec OLED et veille PRG installée sur COM5 le 28 octobre 2026.** Appui court, appui long puis relâchement, sommeil du capteur, silence en veille et reprise OLED/lux après RST vérifiés sur carte. Voir les [preuves de veille](../../diagnostics/plant-station-veille-20261028.md).

## Câblage conservé

| Module VEML7700 | Heltec V3 |
| --- | --- |
| VIN | 3V3 |
| GND | GND commun |
| SDA | GPIO4 |
| SCL | GPIO5 |
| 3Vo | Libre |

Brancher hors tension. Les rappels I2C doivent être à 3,3 V ; vérifier ceux déjà présents sur le module. Ne pas relier 3Vo à VIN. L'adresse attendue est `0x10`. Un ACK et une configuration relue indiquent un composant compatible, sans certifier son identité physique.

La cible est `heltec_wifi_lora_32_V3` : ESP32-S3, flash 8 Mo. Le composant présent a été interrogé sur COM5 : QFN56 révision 0.2, MAC `3c:0f:02:eb:f5:28`. Cette révision de puce **n'identifie pas la sous-révision physique V3.x** de la carte. L'OLED utilise un second contrôleur I2C indépendant sur GPIO17/18, adresse 0x3C, RESET=21 et Vext=36 actif bas. Les broches I2C par défaut 41/42 ne sont pas utilisées.

## Installer et compiler

Depuis la racine du dépôt, sous PowerShell, avec Python disponible :

```powershell
python -m venv .tools/venv-plant-veml
.\.tools\venv-plant-veml\Scripts\python.exe -m pip install platformio==6.1.18
.\.tools\venv-plant-veml\Scripts\python.exe -m platformio run --project-dir firmware/PlantStationVeml7700 -e heltec_v3_text -e heltec_v3_json
```

L'installation propre télécharge les outils Espressif. Pour réutiliser l'installation déjà présente dans ce dépôt :

```powershell
$env:PYTHONPATH = (Join-Path $PWD '.tools\python')
$env:PLATFORMIO_CORE_DIR = (Join-Path $PWD '.tools\platformio')
python -m platformio run --project-dir firmware/PlantStationVeml7700 -e heltec_v3_text -e heltec_v3_json
```

Dans l'environnement Codex de cette livraison, l'accès aux paquets existants exige une exécution hors sandbox approuvée. Cela n'est pas une exigence du firmware sur un poste normal.

Les versions fixées dans `platformio.ini` sont Espressif32 **6.12.0**, SparkFun VEML7700 **2.0.1** et SparkFun Toolkit **1.2.1**, Adafruit SSD1306 **2.5.13**, GFX **1.13.0** et BusIO **1.17.4**. Le profil par défaut `heltec_v3_text` produit du texte ; `heltec_v3_json` produit du JSON. Les fichiers générés se trouvent dans `.pio/build/<profil>/` sous ce sous-projet. `short_paths.py` reprend le raccourcissement des includes adapté aux chemins Windows.

## Réinstaller et surveiller

Le profil texte de ce diagnostic est actuellement installé. Pour le réinstaller ou ouvrir le moniteur série :

```powershell
python -m platformio device list
python -m platformio run --project-dir firmware/PlantStationVeml7700 -e heltec_v3_text --target upload --upload-port COM5
python -m platformio device monitor --project-dir firmware/PlantStationVeml7700 -e heltec_v3_text --port COM5 --baud 115200
```

Pour le JSON, remplacer `heltec_v3_text` par `heltec_v3_json` dans upload et monitor. Les commandes supposent le même Python/PlatformIO que dans l'étape précédente ; pour la venv, remplacer `python` par son exécutable. Fermer les autres moniteurs avant l'upload. Si la liaison échoue, vérifier le port, le câble de données et le pilote CP210x ; en dernier recours suivre la procédure BOOT/RESET Heltec.

Le 28 octobre 2026, COM5 est le pont **Silicon Labs CP210x**, VID:PID `10C4:EA60`, numéro de série `0001`. COM1 est un port ACPI, pas cette carte. Le numéro COM peut changer : le vérifier avant chaque flash. `ARDUINO_USB_CDC_ON_BOOT=0` conserve l'UART0 (TX43/RX44), les lignes DTR/RTS du moniteur sont désactivées pour éviter un reset involontaire.

## Configuration et sorties

`include/config.h` centralise GPIO4/5, adresse `0x10`, I2C 100 kHz, version `1.2.0`, première conversion et paramètres compilables :

| Option de compilation | Défaut | Bornes / effet |
| --- | --- | --- |
| `PLANT_INTERVAL_MS` | 1000 | 150 à 2147483647 ms ; cadence de publication |
| `PLANT_RETRY_MS` | 3000 | 150 à 2147483647 ms ; intervalle minimal après échec |
| `PLANT_TIMEOUT_MS` | 25 | 1 à 100 ms ; timeout d'une transaction Wire |
| `PLANT_OLED` | 1 | 0/1 ; affichage OLED local |
| `PLANT_SCAN` | 1 | 0/1 ; scanner au démarrage |
| `PLANT_JSON` | 0 / 1 selon profil | 0/1 ; sortie applicative texte / JSON |

Ajouter par exemple `-DPLANT_INTERVAL_MS=2000` et `-DPLANT_SCAN=0` aux `build_flags` du profil choisi. Éviter les doubles définitions. Les valeurs invalides échouent à compilation. Gain **1/8**, intégration **100 ms**, interruptions et économie d'énergie sont fixés et vérifiés par relecture ; les changer exige d'adapter calcul, attente et tests.

Exemple de mesure **simulée**, pas d'un relevé matériel :

```json
{"type":"plant_station_veml7700","sensor":"VEML7700","event":"mesure","version":"1.2.0","uptime_ms":1000,"status":"ok","raw":1000,"lux":537.6000,"address":16,"message":"Luminosite ambiante"}
```

En erreur, le même schéma est conservé : `lux:null`, `raw:null` et `status` explicite. En saturation ou hors plage nominale, le brut est gardé pour le diagnostic et `lux:null`. `type`, `sensor`, `event`, `version`, `status`, `message` sont des chaînes ; `uptime_ms` est un entier en millisecondes depuis ce démarrage, étendu sur 64 bits ; `address` est l'adresse décimale (16 = 0x10), ou -1 pour un événement global. `raw` et `lux` sont des nombres ou `null`. Il ne faut pas confondre l'uptime avec une date civile.

Les événements de démarrage, configuration et scanner passent également par le formateur JSON. Les journaux applicatifs restent JSON, y compris en panne de bus ; les logs du framework sont désactivés. Au reset, la ROM ESP32 peut produire un préambule texte **avant** l'exécution du firmware : un lecteur JSON doit l'ignorer et traiter ensuite les lignes commençant par `{`. Une ligne tronquée par un lecteur série doit être écartée. En mode texte, les logs présentent version, uptime, événement, statut, brut et lux.

Seul l'événement `mesure` peut contenir des valeurs numériques dans `raw` et `lux`. Un `scan_ack` réussi garde `status:ok` et `raw:null`, `lux:null`. En texte, une adresse globale est affichée `adresse=globale`.

| `status` | Sens et action |
| --- | --- |
| `ok` | Mesure disponible ; zéro est valide dans l'obscurité |
| `conversion_en_cours` | Attendre la première conversion après configuration |
| `i2c_nack` | Pas d'ACK ; vérifier alimentation, masse, SDA/SCL et module |
| `lecture_courte` | Deux octets non reçus intégralement ; vérifier connexions |
| `erreur_bus` | Timeout, initialisation ou transport invalide ; vérifier lignes et rappels |
| `configuration_perdue` | Configuration relue différente ; coupure/réinitialisation possible |
| `saturation` | ALS = 65535 ou estimation corrigée > 140000 lux ; lux indisponible |

Pendant une lecture ESP32, Wire peut renvoyer zéro octet aussi bien après un NACK que sur timeout ; ces deux cas deviennent alors `erreur_bus`, avec la même reprise sûre. `i2c_nack` correspond aux codes NACK explicites retournés pendant la phase d'écriture/adressage.

## Veille par bouton PRG

Depuis la version **1.2.0**, maintenir **PRG pendant 3 secondes**, attendre « Relacher PRG », puis relâcher : VEML7700 en sommeil, OLED coupé, LED éteinte et ESP32-S3 en veille profonde. Un appui court ne change rien. Appuyer sur **RST**, PRG relâché, pour reprendre les mesures et l'écran. PRG seul ne réveille pas la carte ; aucun réveil périodique n'est configuré. Le bouton doit avoir été relâché après le démarrage avant d'accepter un appui long.

Le VEML7700 reçoit ALS_SD=1 avec vérification par relecture. L'absence du capteur ne bloque pas la veille. La radio est maintenue en RESET sans l'initialiser, et les sorties d'extinction sont tenues pendant le sommeil puis libérées au redémarrage. Ce mode n'est pas une coupure physique de la batterie ; la consommation réelle n'a pas été mesurée.

Test reproductible par DTR/RTS du CP2102 (actionne réellement PRG/RST, termine avec la carte active) :

```powershell
python tools/check_plant_station_sleep.py --port COM5 --output diagnostics/plant-station-sleep-retest.log
```

Les outils Python locaux nécessitent le même PYTHONPATH que la compilation décrite plus haut.

## Affichage OLED et rôle du hub

Depuis la version 1.1.0, l'écran affiche **PLANT STATION**, la luminosité avec une décimale et l'uptime. Une erreur capteur remplace le nombre par un message, sans conserver d'ancien lux. Le second bus permet au pilote VEML7700 de se réinitialiser sans réaffecter les broches OLED. Si l'écran disparaît, les mesures série continuent et une reprise de l'écran est tentée au plus toutes les 10 secondes. `-DPLANT_OLED=0` désactive complètement son alimentation/initialisation logicielle.

Le Heltec reste le **hub prévu pour les futures sondes d'humidité LoRa 868 MHz** du plan Plant Station. Cette version est un diagnostic lumière + écran : la réception des sondes et leur télémétrie restent à intégrer. Elle ne remplace pas le plan d'architecture du hub.

## Fonctionnement interne

`main.cpp` orchestre la série, le scanner et la cadence ; `Diagnostics.*` formate les sorties et limite le scanner à 112 adresses non réservées (0x08–0x77), une par tour de boucle. Un défaut de bus arrête le scan. Un ACK à 0x10 ne suffit pas à identifier un VEML7700.

`VEML7700Sensor.*` gère configuration, attente, acquisition et reprise. Chaque échec invalide immédiatement la mesure ; aucune valeur précédente n'est conservée. Une nouvelle tentative réinitialise Wire, réapplique les paramètres, les relit, puis attend **150 ms après la fin des transactions** avant de lire. Ce délai couvre l'intégration de 100 ms avec tolérance de 30 % et le démarrage. La configuration et l'économie d'énergie sont contrôlées avant et après la lecture ALS. La boucle n'utilise pas de `delay()` ni de boucle d'attente de conversion. Les transactions restent synchrones, bornées par leur timeout ; les tentatives sont espacées, sans rafale de rattrapage.

`StrictWireTransport.h` réutilise le vrai pilote SparkFun `sfDevVEML7700` et ses retours `sfTkError_t`, mais fournit son propre `sfTkIBus`. La version 1.2.1 du transport Toolkit peut compléter une lecture courte par plusieurs `requestFrom`, ce qui ne satisfait pas le rejet strict d'une conversion incomplète. Ici une seule demande de deux octets est acceptée ; taille, disponibilité, octets et ACK sont contrôlés. Écritures avec STOP, lecture avec repeated START puis STOP ; les erreurs sont typées. Le registre d'économie d'énergie non exposé par le pilote SparkFun est écrit et relu directement.

Le calcul local suit la note Vishay : `lux_lin = ALS × 0,5376` pour le gain 1/8 et 100 ms. Au-dessus de 1000 lux linéaires :

```text
lux = 6,0135e-13 × L^4 - 9,3924e-9 × L^3 + 8,1488e-5 × L^2 + 1,0023 × L
```

Le `getLux` de SparkFun 2.0.1 utilise encore 0,4608 pour ce réglage ; il n'est donc pas appelé. Le réglage fixe offre une résolution linéaire de 0,5376 lux/count et un plafond brut proche de **35 232 lux linéaires**. Le polynôme peut produire des extrapolations nettement supérieures à la plage nominale d'environ 140 klux : elles sont rejetées avec `status:saturation`, brut conservé et `lux:null`, sans tronquer la mesure à 140000. Avec ces coefficients, 44586 counts donnent environ 139996 lux, tandis que 44587 counts sont hors plage. `65535` est systématiquement signalé comme saturé. Les valeurs retenues restent des estimations non étalonnées ; le montage, le spectre lumineux, la protection optique et la lumière directe influencent le résultat. Une gestion automatique de gain/intégration et une qualification avec luxmètre sont des suites possibles, hors de ce diagnostic.

## Tests logiciels exécutables

Après compilation du profil texte, avec Python et [uv](https://docs.astral.sh/uv/getting-started/installation/) disponibles :

```powershell
python firmware/PlantStationVeml7700/test/run_host_tests.py
```

Le script utilise Zig **0.13.0** via le paquet Python épinglé, construit des exécutables natifs Windows GNU, puis **les exécute**. Il compile les vrais `VEML7700Sensor.cpp`, `Diagnostics.cpp`, `main.cpp`, le transport strict et les sources réelles SparkFun/Toolkit installées par PlatformIO. Les interfaces matérielles Arduino (temps, Wire, Serial capturée et identité ESP) sont simulées, sans API de test ajoutée au firmware. Il injecte ACK/NACK, lectures courtes, `read=-1`, panne de bus, configuration perdue, coupure pendant acquisition, reprise et temps aux bornes du débordement. Il vérifie obscurité, correction, saturation et cohérence de `status()`, scanner, délai après configuration lente et rejets des nombres invalides.

Le vrai `setup()/loop()` est exécuté dans quatre processus : démarrage normal, juste avant le débordement, OLED absent et OLED désactivé à compilation : transition du scan, événements de démarrage, publication avant/à échéance, boucles répétées, absence de rafale après pause et récupération sont contrôlées. Python parse **62 lignes** du vrai formateur (11 unitaires, 13 pour chacun des trois scénarios avec support OLED, 12 pour le mode sans OLED), compare les statuts attendus exactement et vérifie uptime, schéma et nombres/null. Les tests PRG couvrent appui court/long, rebonds, relâchement, bouton tenu au démarrage et débordement temporel ; le vrai main est exercé jusqu’au sommeil avec capteur présent puis absent. Les séquences GPIO, arrêt OLED et flush série sont vérifiés. Les tests OLED contrôlent le rendu, le remplacement des lux par une erreur, l'uptime 64 bits, la reprise après NACK et la séquence d'alimentation/reset. Un petit test Python exécute aussi le vrai `short_paths.py` avec des includes sur lecteurs C: et D:. Un échec retourne un code non nul. Ce script est distinct de `pio test`.

## Protocole manuel après flash volontaire

1. Hors tension, vérifier les cinq lignes de câblage ci-dessus ; alimenter en USB et ouvrir le moniteur à 115200. Observer version et puce, un ACK compatible à 0x10 si le scan est actif, puis la configuration vérifiée et les mesures espacées d'environ une seconde.
2. Couvrir le capteur avec un cache opaque puis l'exposer à une lampe et à la lumière ambiante. Attendre plusieurs lignes par condition. Vérifier variation cohérente, lux finis ≥ 0 et zéro valide si ALS vaut 0 ; ne pas exiger zéro dans une pièce simplement sombre.
3. Pour tester l'absence, débrancher USB, enlever le capteur et redémarrer. Attendre des erreurs explicites, `lux:null` en JSON et des reprises espacées. Rebrancher hors tension permet un essai de démarrage, mais ne prouve pas encore une récupération sans reset du hub.
4. Pour la reprise sans redémarrage, préparer un montage d'essai permettant de couper puis restaurer proprement l'alimentation du module sans retour de courant par SDA/SCL (commutation/isolation appropriée). Garder le hub sous tension. Vérifier erreur, reconfiguration périodique et attente d'une nouvelle conversion avant le retour des lux. Ne pas manipuler des Dupont alimentés au risque de court-circuit.
5. Exposer progressivement à une lumière plus forte, sans chauffer le composant. Si ALS atteint 65535, attendre `saturation` avec `lux:null`, puis retour à `ok` quand on réduit la lumière. Sans saturation réellement atteinte, noter l'essai « non atteint » ; le test logiciel couvre cette borne.
6. Tester PRG : un appui court conserve les mesures. Maintenir 3 secondes, attendre la demande de relâchement puis relâcher ; vérifier extinction écran et silence série. PRG seul ne réveille pas. Appuyer sur RST avec PRG relâché et vérifier retour écran et nouvelles mesures.
7. Refaire le protocole avec le profil JSON ; parser chaque ligne applicative avec un lecteur JSON et vérifier les champs/timestamps. Noter durée, emplacement, conditions, erreurs et résultat dans un journal séparé.

Les premières mesures en lumière ambiante sont validées sur le matériel. Les essais de variation contrôlée de lumière, reconnexion, saturation et stabilité prolongée restent **à réaliser**. Le débordement de `millis()` est testé par injection de temps, sans prétendre avoir laissé tourner la carte 49 jours.

## Suites possibles

- Réception des futures sondes d’humidité LoRa 868 MHz et affichage par pot.
- Gain/intégration automatiques et calibration avec luxmètre.
- Télémétrie future LoRa ou Wi-Fi pour le hub Plant Station.
- Mesure de consommation et optimisation de l’alimentation après la veille déjà implémentée.

Ces fonctions ne sont pas implémentées dans ce diagnostic.

## Références primaires

- [Vishay — Designing the VEML7700 Into an Application](https://www.vishay.com/docs/84323/designingveml7700.pdf) : résolution, tolérance d'intégration, économie d'énergie et correction.
- [SparkFun — bibliothèque VEML7700 2.0.1](https://github.com/sparkfun/SparkFun_VEML7700_Arduino_Library/tree/v2.0.1) : configuration et lecture ALS avec statut.
- [SparkFun — Toolkit](https://github.com/sparkfun/SparkFun_Toolkit) : manifeste VEML7700 exige Toolkit ≥1.2.0 ; 1.2.1 épinglée ici.
- [Heltec — WiFi LoRa 32 V3](https://heltec.org/project/wifi-lora-32-v3/) et [documents matériels](https://s.heltec.cn/download/WiFi_LoRa_32_V3/) : carte, schémas et brochage.
- [PlatformIO — cible Heltec V3](https://docs.platformio.org/en/latest/boards/espressif32/heltec_wifi_lora_32_V3.html).
- [Plan Plant Station local](../../docs/plant-station/plan-realisation-balcon-v1.md) : rôle futur du hub et étapes plantes.
