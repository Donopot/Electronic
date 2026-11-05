# Plant Station VEML7700 — vérification du 28 octobre 2026

Ce rapport conserve les preuves de la version 1.0.0. La version actuelle **1.1.0 avec OLED** est décrite dans le [rapport OLED](plant-station-oled-20261028.md).

## État de livraison

Sous-projet autonome [PlantStationVeml7700](../firmware/PlantStationVeml7700/README.md), cible Heltec V3 / ESP32-S3, SDA4/SCL5, UART0 115200. **Profil texte v1.0.0 téléversé avec succès le 28 octobre 2026 à la demande de l'utilisateur.** Capteur acquittant à 0x10, configuration relue et 21 mesures physiques sans erreur pendant 22 secondes. Le programme RX précédent a été sauvegardé ; ses sources et les projets météo C3 restent disponibles.

## Détection USB et carte

Observations réalisées dans cette session le 28/10/2026, vers 08:04 UTC, par l'agent principal (accès Python/pyserial/esptool hors sandbox nécessaire sur ce poste) :

```text
python -m serial.tools.list_ports -v
COM5
  desc: Silicon Labs CP210x USB to UART Bridge (COM5)
  hwid: USB VID:PID=10C4:EA60 SER=0001 LOCATION=1-6
COM1 : port ACPI PNP0501
```

Interrogation esptool **sans écriture flash** : `esptool.py v4.9.0 --chip esp32s3 --port COM5 --baud 115200 chip_id`.

```text
Chip is ESP32-S3 (QFN56) (revision v0.2)
Features: WiFi, BLE, Embedded Flash 8MB (GD)
Crystal is 40MHz
MAC: 3c:0f:02:eb:f5:28
Hard resetting via RTS pin...
```

Cette commande a interrogé puis réinitialisé la puce via RTS ; elle n'a pas remplacé le programme RX. Elle identifie l'ESP32-S3 et sa flash, sans identifier la sous-révision physique Heltec V3.x ni attester la présence du VEML7700. L'énumération USB seule ne prouve pas non plus le fonctionnement du bus capteur.

## Compilation cible

Commande finale depuis la racine PowerShell :

```powershell
$env:PYTHONPATH = (Join-Path $PWD '.tools\python')
$env:PLATFORMIO_CORE_DIR = (Join-Path $PWD '.tools\platformio')
python -m platformio run --project-dir firmware/PlantStationVeml7700 -e heltec_v3_text -e heltec_v3_json
```

Versions réellement résolues :

| Élément | Version |
| --- | --- |
| PlatformIO Core | 6.1.18 (installation locale) |
| Espressif32 | 6.12.0 |
| Cible | heltec_wifi_lora_32_V3 |
| Arduino ESP32 | framework 3.20017.241212+sha.dcc1105b (Arduino 2.0.17) |
| Toolchain Xtensa ESP32-S3 | 8.4.0+2021r2-patch5 |
| esptool | 4.9.0 |
| SparkFun VEML7700 | 2.0.1 |
| SparkFun Toolkit | 1.2.1 (manifest capteur : ≥1.2.0) |
| Wire | 2.0.0 |

Les deux profils finaux après correctifs de revue réussissent, vérification répétée par l'agent principal : texte en 5,824 s et JSON en 5,835 s. Par profil : RAM **18768 / 327680 octets (5,7 %)** ; flash programme **282361 / 3342336 octets (8,4 %)**, partition par défaut de la cible 8 Mo. Le dénominateur flash est la partition application, pas toute la flash physique. Firmware ELF/BIN généré séparément sous `firmware/PlantStationVeml7700/.pio/build/<profil>/`.

La [sortie des builds](plant-station-veml7700-builds-20261028.log) contient versions, tailles et statuts exacts. Cette étape de compilation n'utilisait pas `--target upload` ; le téléversement demandé ensuite est décrit ci-dessous. La construction seule de l'image BIN utilise esptool sans connexion série.

## Exécution des tests logiciels

Commande :

```powershell
python firmware/PlantStationVeml7700/test/run_host_tests.py
```

Compilateur **Zig 0.13.0**, mode C++11, `-O0 -g -Wall -Wextra -Werror=return-type`, cible Windows GNU `x86_64-windows-gnu`. Après correctifs de revue, le script compile puis exécute les tests unitaires et le vrai `setup()/loop()` lié depuis `main.cpp`, dans deux processus (normal et wrap). Sources testées : vrais pilote Plant Station, formateur, scanner, adaptateur sfTkIBus, orchestration, `sfDevVEML7700.cpp` 2.0.1 et `sfToolkit.cpp` 1.2.1. Interfaces Arduino simulées : temps, Wire/registres, Serial capturée, identité ESP et désactivation des logs. Aucun calcul ni formateur n'est réimplémenté et aucune API de test n'a été ajoutée au firmware.

Résultat après revue, confirmé par une exécution finale de l'agent principal : **code de sortie 0**, toutes les assertions C++ réussies et **35 lignes** du vrai formateur parsées avec schéma constant (11 unitaires, 12 par scénario d'orchestration). Les séquences exactes de statuts sont comparées, y compris normalisation NaN/Inf/négatif en `erreur_bus` et métadonnée `scan_ack` en `ok` avec nombres null. La [sortie des tests](plant-station-veml7700-host-tests-20261028.log) conserve les marqueurs PASS et les lignes JSON.

| Matrice de la spécification | Couverture exécutée |
| --- | --- |
| Présent / configuration valide | Configuration 0x1000, PSM=0 ; ALS1000→537,6 lux ; vrai setup/loop avant et à1000ms, boucles répétées et pause sans rafale |
| Obscurité ALS=0 | Statut ok et lux=0 |
| NACK, lecture courte, bus défaillant | Injection NACK2/3, erreur5, requestFrom1/0, read=-1, écriture courte et begin refusé ; aucune ancienne mesure |
| Coupure / configuration perdue | Registre0 revenu à1, PSM changé, coupure simulée après lecture ALS ; reprise à3000ms, Wire.end/begin, reconfiguration puis délai150ms |
| Saturation65535 | Statut saturation et lux absent ; `status()` cohérent, retour ok après baisse. Tests additionnels raw44586 juste sous140klx,44587 hors plage et65534 rejeté |
| JSON valide / erreur | 35 lignes : clés identiques, type et sensor stables, uptime entier64bits, statuts exacts, lux nombre/null, NaN/Inf/négatif et échappement ; vrai ACK du scan avec raw/lux null |
| Débordement millis | Vrai setup/loop en scénario wrap, cadence et uptime étendu, erreur puis reprise sans anciennes données ; tests unitaires des reprises répétées |

Tests additionnels : scanner à112 adresses non réservées, un ACK, scanner désactivé et arrêt immédiat en erreur de bus ; attente de première conversion comptée **après** les transactions lentes de configuration ; tampon de sortie trop petit rejeté ; adresse textuelle globale explicite. `test_short_paths.py` a exécuté le vrai script SCons et vérifié include relatif sur le même lecteur et absolu sur un lecteur différent.

Un avertissement Clang `-Wvla-cxx-extension` provient d'un overload du Toolkit (`uint16_t data16[length]`, non utilisé par le firmware). Aucun changement n'a été appliqué aux sources tierces. La première tentative de test via distribution Zig temporaire `uv --no-cache` a échoué sur des en-têtes Mingw manquants ; l'installation en cache uv et la cible Windows GNU explicite ont résolu ce problème. Une erreur initiale d'include `<initializer_list>` dans le test a également été corrigée avant la passe finale. Les résultats ci-dessus concernent bien l'exécution finale réussie.

## Choix techniques vérifiés

- SparkFun est conservé pour la configuration et `getAmbientLight(uint16_t&)`, avec contrôle du résultat `sfTkError_t`. Le transport Toolkit fourni boucle pour compléter une lecture courte ; l'adaptateur strict impose une requête complète de deux octets et rejette les données partielles.
- `getLux` de SparkFun 2.0.1 conserve un coefficient0,4608 au gain1/8 et100ms. Le calcul local utilise0,5376 et la correction Vishay au-dessus de1000lux linéaires. Un résultat corrigé >140000lux est déclaré saturation/hors plage avec `lux:null`, sans plafonnement numérique.
- Lecture de configuration et PSM avant et après ALS ; reprise sans `delay` de conversion, timeout25ms, scanner borné, publication1000ms, reprise3000ms. Les intervalles utilisent la soustraction non signée ; uptime est étendu sur64bits.
- Profil JSON : aucun log applicatif en texte ; logs du framework désactivés. Préambule ROM possible au reset, documenté pour le parseur série.

## Téléversement et premières mesures physiques

Autorisation utilisateur : « televerse ». Port COM5 et identité confirmés à nouveau : ESP32-S3 QFN56 révision 0.2, flash 8 Mo, MAC `3c:0f:02:eb:f5:28`.

Avant remplacement, lecture complète de **8388608 octets**, terminée sans erreur en 105,4 secondes à 921600 bauds. Sauvegarde privée ignorée par Git : `.tools/private/heltec-before-veml-20261028-094132.bin`. SHA256 : `68E020D37820317AE8E9DFBC441825234BC03B773BEEB0D7785BC31BBA21351A`.

Installation du profil texte :

```powershell
python -m platformio run --project-dir firmware/PlantStationVeml7700 -e heltec_v3_text --target upload --upload-port COM5
```

Résultat **SUCCESS**, durée 10,703 secondes. Les blocs bootloader, table de partitions, boot_app0 et application ont été écrits ; esptool confirme les hachages. La carte redémarre ensuite sur le nouveau firmware. [Journal du téléversement](plant-station-veml7700-upload-20261028.log).

Capture série de **22 secondes** à 115200 bauds, commencée le 28 octobre à **08:43:59 UTC / 09:43:59 à Paris**, avec redémarrage RTS pour observer le scanner. DTR reste relâché.

- ACK observé à **0x10** sur SDA4/SCL5 ; configuration gain 1/8, intégration 100 ms et économie désactivée vérifiée.
- **21 mesures valides**, zéro erreur d'acquisition, de **87,0912 à 90,3168 lux** ; brut ALS de 162 à 168.
- Horodatages de 1151 à 21151 ms, avec **1000 ms** entre chaque mesure ; dernière valeur observée **88,1664 lux**.
- Port série refermé après capture ; firmware texte laissé en fonctionnement sur la carte.

[Journal matériel](plant-station-veml7700-hardware-20261028.log) et [mesures structurées](plant-station-veml7700-hardware-20261028.json).

Ces observations valident le démarrage, les échanges I2C et les acquisitions en lumière ambiante. Elles ne constituent pas un étalonnage par luxmètre ni un essai contrôlé de réponse à une variation de lumière.

## Essais matériels restant à réaliser

Après le téléversement et les premières acquisitions réussis, le [protocole manuel](../firmware/PlantStationVeml7700/README.md#protocole-manuel-apr%C3%A8s-flash-volontaire) reste à poursuivre pour variation contrôlée lumière/obscurité, absence capteur, récupération sans reset sur montage permettant une coupure propre du module, saturation si atteignable et profil JSON sur carte. Ces essais, la calibration par luxmètre, la consommation et la stabilité prolongée ne sont **pas réalisés**. Aucun niveau électrique ni résistance de rappel n'a été mesuré pendant cette séance.

Sources : [Vishay](https://www.vishay.com/docs/84323/designingveml7700.pdf), [SparkFun 2.0.1](https://github.com/sparkfun/SparkFun_VEML7700_Arduino_Library/tree/v2.0.1), [Toolkit](https://github.com/sparkfun/SparkFun_Toolkit), [Heltec V3](https://heltec.org/project/wifi-lora-32-v3/). Aucun commit ni ajout global à l'index n'a été effectué.
