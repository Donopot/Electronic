# EnviroTerminal V1 — BMP180, LoRa SPI et site local

## Organisation du projet avec BMAD

La méthode BMAD 6.12.4 est installée pour Codex. Le [guide BMAD](BMAD.md)
relie l'analyse de l'existant, les exigences, l'architecture, le backlog et
les résultats de vérification. Le prochain travail recommandé est la story
1.2 : rendre la validation logicielle reproductible avec une commande.

**Puissance TX installee le 26 octobre : 8 dBm nominaux (environ 6,3 mW),**
contre 2 dBm auparavant. Registres de puissance relus et paquets recus.
Comparaison sur batterie au meme emplacement a environ 7 m derriere un mur :
deux paquets consecutifs recus a -119 dBm, SNR +6,50 / +6,75 dB, aucune
nouvelle erreur CRC. Reference a 2 dBm : -127 dBm / -4,25 dB.
Wi-Fi et LoRa conserves. Voir
[diagnostics/lora-power-8dbm-20261026.md](diagnostics/lora-power-8dbm-20261026.md).

**Diagnostic precedent du 26 octobre, a 2 dBm : liaison degradee au placement eloigne.**
Sur la meme batterie, RSSI=-108 dBm / SNR=+9,75 dB a proximite, puis
RSSI=-127 dBm / SNR=-4,25 dB apres retour a environ 7 m derriere un mur.
Le dernier paquet est valide (22,7 degres C / 1008,53 hPa), mais les pertes
precedentes imposent de verifier les antennes et leur raccordement.
Aucun firmware modifie. Voir
[diagnostics/lora-loss-20261026.md](diagnostics/lora-loss-20261026.md).

**Recepteur LoRa 433 MHz sur COM4** : canal commun 433,92 MHz ; projet et cablage dans
[firmware/WeatherReceiver/README.md](firmware/WeatherReceiver/README.md).
Tableau de bord installe sur le recepteur ; demarrage et ecoute LoRa LF valides
avec DIO0 debranche. Son Wi-Fi se configure par Bluetooth.
Apres correction de la bande a 433,92 MHz sur les deux cartes, reception
meteo retablie le 25 octobre : 22,9 degres C et 1008,24 hPa publies sur
le tableau de bord. La sonde conserve Wi-Fi et LoRa simultanement.
Voir le journal et l'historique du diagnostic dans [LORA.md](LORA.md).
Le recepteur est connecte a la box : **http://192.168.1.197/** ; API et page
web disponibles. L'ancienne adresse de la sonde **http://192.168.1.65/**
ne repond pas au retest du26octobre ; la reception LoRa est independante.

Firmware Arduino pour ESP32-C3 SuperMini : mesures BMP180 et affichage LCD I²C
16×2, actualisés toutes les 2 secondes. Le Wi-Fi est active uniquement par
le profil optionnel `enviroterminal_local_web`, presente dans [WEB.md](WEB.md).
Ce profil heberge le tableau de bord sur le Wi-Fi de la box, configure
la connexion par Bluetooth et conserve le LCD desactive. Le telephone
reste sur son reseau habituel. Une API et un service de decouverte
communs preparent les futurs projets ; voir [WEB.md](WEB.md).
Le BMP180 utilise `Wire` sans bibliothèque capteur externe ; le LCD utilise
**hd44780 de Bill Perry, version 1.3.2** (adaptateurs PCF8574/PCF8574A/MCP23008).

**Montage actuel : BMP180, SDA=GPIO4 / SCL=GPIO3, et SX127x en SPI**.
Le test combine a confirme 13 mesures sans erreur en 25 secondes et les
controles SPI reussis : `diagnostics/bmp180-lora-combined-test.log`.
Le profil web est ensuite installe sur COM3 ; il attend la configuration
du Wi-Fi domestique par l'utilisateur. Voir [WEB.md](WEB.md).

Validation precedente du BMP180 seul, SDA=GPIO4 / SCL=GPIO3, avec
`enviroterminal_sensor_gpio4_3`. Le 24 octobre 2026, après vérification des
connexions/soudures, le scan a trouvé 0x77, l'identifiant était 0x55 et 13 mesures
ont été reçues en 25 secondes sans erreur, à intervalles de 2000 ms.
Dernière mesure : 27,0 °C et 1005,75 hPa. Journal :
`diagnostics/bmp180-gpio4-3-solder-recheck.log`. Le LCD n'est pas testé dans cet essai.

Radio **XL1278-SMT / SX1278**, avec profil optionnel
`enviroterminal_bmp_lora_spi`. Voir [LORA.md](LORA.md) pour les connexions et le
diagnostic SPI sans emission. Ce profil conserve le BMP180 sur GPIO4/GPIO3 et
desactive le LCD. Les autres profils laissent les broches radio au repos.

## Câblage

Effectuer le câblage carte hors tension, puis alimenter la SuperMini par USB-C.

| BMP180 | ESP32-C3 SuperMini |
| --- | --- |
| VCC / VIN | 3V3 (3,3 V) |
| GND | GND |
| SDA | GPIO4 |
| SCL | GPIO3 |

Les lignes SDA et SCL nécessitent des résistances de tirage vers **3,3 V**.
Elles sont souvent présentes sur le module BMP180. Si elles sont absentes,
ajouter par exemple une résistance de 4,7 kΩ entre chaque ligne et 3V3.

### LCD 16×2 I²C

| LCD | Raccordement |
| --- | --- |
| Alimentation | 5 V, selon le module de l'utilisateur |
| GND | Masse commune avec l'ESP32 |
| SDA | GPIO3, avec niveaux compatibles 3,3 V côté ESP32 |
| SCL | GPIO2, avec niveaux compatibles 3,3 V côté ESP32 |

L'alimentation 5 V du LCD ne garantit pas des signaux SDA/SCL compatibles
avec l'ESP32-C3. Si les tirages du module sont à 5 V sans adaptation intégrée,
intercaler un **convertisseur I²C bidirectionnel** : LV=3V3, HV=5 V, GND commun,
LV1=GPIO3/SDA, HV1=SDA LCD, LV2=GPIO2/SCL, HV2=SCL LCD.
Un raccordement direct suppose une compatibilité 3,3 V attestée du module.
**Mesures ensuite rapportées par l'utilisateur : environ 4,7 V sur VCC, SDA et
SCL du LCD, par rapport à GND.** Les lignes I²C de ce raccordement ne sont donc
pas adaptées aux GPIO 3,3 V de l'ESP32-C3. Débrancher l'alimentation et adapter
les niveaux avant de reprendre les essais LCD. Cette mesure ne prouve pas à
elle seule qu'un GPIO a été endommagé ni que la tension explique tous les NACK.
Voir les [caractéristiques électriques Espressif](https://documentation.espressif.com/esp32-c3_datasheet_en.html).

Le C3 possède [un seul contrôleur I²C matériel](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/api-reference/peripherals/i2c.html).
Dans la variante complète, le firmware le réaffecte successivement au BMP180 sur GPIO4/3, puis au LCD sur
GPIO3/2, et le restitue au capteur après chaque affichage, y compris en cas
d'erreur LCD. Les anciennes broches sont libérées en entrée lors de chaque
changement pour ne pas reproduire les signaux sur l'autre paire de fils.
Aucun recâblage du BMP180 n'est requis.

### Identification par la photo du module

La photo fournie par l'utilisateur correspond visuellement au circuit **1602IIC**
avec interface I²C intégrée, premier modèle illustré par Freenove dans sa fiche
`Power Supply Specifications.pdf`. Le constructeur spécifie une alimentation
**5 V** pour le LCD1602 et indique qu'une alimentation 3,3 V réduit le contraste.
Son tableau accepte des signaux SDA/SCL de 3,3 à 5 V côté LCD ; cela ne rend pas
les GPIO de l'ESP32-C3 tolérants à 5 V.

Dans l'orientation de la photo reçue (connecteur à gauche), de haut en bas :
**4/SCL jaune**, **3/SDA orange**, **2/VCC vert/turquoise**, **1/GND bleu**,
d'après la numérotation visible et le schéma Freenove du même module.
Les extrémités côté ESP32 ne sont pas visibles et restent à vérifier.
Le marquage de la puce est illisible sur cette photo ; l'adresse doit être
confirmée par acquittement I²C. Freenove indique 0x27 pour 8574T ou puce vierge,
et 0x3F pour 8574AT, à l'état d'origine des ponts d'adresse.

Sources constructeur téléchargées dans `.tools/freenove-source/` : `power.pdf`
et `addresses.pdf`, depuis le dépôt officiel Freenove/Freenove_LCD_Module.

## Téléversement avec Arduino IDE

1. Installer le paquet **esp32 by Espressif Systems** dans le gestionnaire de cartes.
2. Ouvrir `firmware/EnviroTerminalDiagnostic/EnviroTerminalDiagnostic.ino`.
   Conserver **tous les fichiers du dossier du sketch** ensemble, notamment
   `Bmp180Compensation.h`, `I2cBusRouter.h`, `Lcd1602.h` et `Lcd1602.cpp`.
   Installer **hd44780 by Bill Perry** dans le gestionnaire de bibliothèques.
3. Choisir **ESP32C3 Dev Module**, mémoire flash **4 MB** pour une SuperMini
   standard, et **USB CDC On Boot → Enabled**.
4. Sélectionner le port USB de la carte et téléverser.
5. Ouvrir le moniteur série à **115200 bauds**, puis appuyer sur **RESET**
   pour voir aussi le scan du démarrage.

Si le téléversement ne démarre pas : maintenir **BOOT**, appuyer puis relâcher
**RESET**, relâcher **BOOT**, sélectionner le port apparu et relancer.
Après le premier téléversement, appuyer sur **RESET** si nécessaire.
Utiliser un câble USB-C capable de transférer des données.

Le réglage USB CDC permet d'utiliser `Serial` via l'USB natif du C3 ; voir
la [documentation Espressif](https://docs.espressif.com/projects/arduino-esp32/en/latest/tutorials/cdc_dfu_flash.html).

## Téléversement avec PlatformIO

Le fichier `platformio.ini` fixe la plateforme Espressif32 à `6.12.0`
(Arduino-ESP32 2.0.17). Le profil générique ESP32-C3-DevKitM-1 fournit la cible
C3 / 4 MB ; le code utilise GPIO4 et GPIO3 pour le BMP180 et active la sortie USB CDC.
La dépendance LCD est installée automatiquement par PlatformIO.

```powershell
pio run
pio run --target upload
pio device monitor --baud 115200
```

Si plusieurs ports sont présents, ajouter `--upload-port COMx` à la commande
de téléversement et `--port COMx` au moniteur, avec le port réel de la carte.

Pour la variante **capteur seul**, qui n'accède pas aux GPIO2/3 :

```powershell
pio run -e enviroterminal_sensor_only
pio run -e enviroterminal_sensor_only --target upload --upload-port COM3
```

Configuration de comparaison BMP180 seul sur SDA=GPIO4 / SCL=GPIO1 :

```powershell
pio run -e enviroterminal_sensor_gpio4_1
# Après déplacement physique de SCL vers GPIO1 :
pio run -e enviroterminal_sensor_gpio4_1 --target upload --upload-port COM3
```

Les broches BMP180 sont définies dans `I2cBusRouter.h` et peuvent être surchargées
par les options de compilation `ENVIRO_BMP_SDA` / `ENVIRO_BMP_SCL`.

## Comportement et validation matérielle

- Initialisation I²C à **100 kHz** sur SDA=4 et SCL=3, avec délai de transaction
  limité à **50 ms**.
- Scan des adresses non réservées **0x08 à 0x77**, avec liste des périphériques
  détectés et signalement explicite de l'absence de **0x77**.
  Après trois erreurs de bus consécutives (hors NACK d'adresse inutilisée), le
  scan est interrompu et déclaré incomplet. Les tentatives directes à 0x77 et
  le test LCD peuvent alors continuer sans attendre tout le scan défaillant.
- Vérification du registre d'identification **0xD0 = 0x55**, puis lecture des
  22 octets de calibration usine. L'identifiant est partagé avec le BMP085 ;
  il atteste la compatibilité du capteur, pas son modèle exact.
- Conversion de température puis de pression, suréchantillonnage **OSS=3**.
  Chaque conversion attend la fin réelle du bit SCO avec une limite de temps.
- Première mesure après l'initialisation, puis lancement d'une mesure toutes
  les **2 secondes**. Le temps des conversions peut légèrement faire varier
  l'heure d'affichage. La pression affichée est la pression absolue locale,
  sans correction au niveau de la mer.
- NACK, délais dépassés, lectures incomplètes, mauvais identifiant, calibration
  invalide et mesures hors plage donnent un message `[ERREUR]`. Une mesure
  échouée ne réaffiche aucune ancienne valeur. La détection/calibration est
  retentée automatiquement aux cycles suivants ; après un échec de mesure,
  le contrôleur I²C est aussi réinitialisé. Le scan complet a lieu au démarrage.
- Au premier affichage, test prioritaire de l'adaptateur LCD à **0x27**, selon
  la demande de l'utilisateur. Si cette adresse n'est pas acquittée, recherche
  des autres adresses usuelles dans 0x20..0x27 et 0x38..0x3F. En cas de délai
  dépassé ou d'erreur de bus, la recherche s'arrête pour ce cycle.
  L'adaptateur détecté est autoconfiguré avec `hd44780_I2Cexp`.
  L'adresse prioritaire est définie par `LCD_ADDRESS` dans `Lcd1602.cpp`.
  Chaque ligne est réécrite sur 16 caractères
  sans effacer l'écran entier à chaque mesure. En cas de problème BMP180,
  l'écran affiche `ERREUR BMP180` / `Mesure invalide`.
- Un échec LCD est signalé en série et retenté après 10 secondes. Les mesures
  du BMP180 continuent. Un message `[LCD]` confirme les écritures acceptées ;
  la lisibilité et le contraste restent à vérifier visuellement sur l'écran.

Affichage prévu sur le LCD :

```text
Temp:  24.3 C
P: 1009.02 hPa
```

Exemple **illustratif**, ne provenant pas d'un essai sur votre matériel :

```text
BMP180 : SDA=GPIO4, SCL=GPIO3, 100 kHz, timeout=50 ms.
[SCAN] Recherche I2C de 0x08 a 0x77...
[SCAN] Peripherique a 0x77 (adresse BMP180 attendue)
[SCAN] Termine : 1 peripherique(s), 0 erreur(s) de bus.
[OK] Capteur compatible BMP180, ID=0x55 ; calibration lue.
[OK] t=3200 ms | Temperature : 22.4 °C | Pression : 1008.32 hPa
[OK] t=5200 ms | Temperature : 22.4 °C | Pression : 1008.30 hPa
```

Pour valider le montage, vérifier la présence de 0x77, l'identifiant attendu,
puis des valeurs cohérentes pendant au moins une minute, sans erreur.
Pour tester le cas « capteur absent », couper l'alimentation, débrancher le
module puis redémarrer : le scan et les tentatives suivantes doivent signaler
l'échec. Recâbler hors tension et redémarrer pour reprendre les mesures.

| Message | Vérification à effectuer |
| --- | --- |
| Aucun capteur à 0x77 / NACK | Alimentation 3,3 V, masse commune, GPIO4/SDA et GPIO3/SCL |
| Délai dépassé / erreur de bus | Courts-circuits, ligne maintenue à zéro, tirages, connexions |
| Lecture incomplète | Stabilité du câblage et de l'alimentation, longueur des fils |
| Identifiant différent de 0x55 | Référence réelle du capteur à 0x77 |
| Calibration/calcul invalide ou hors plage | Relancer hors tension et vérifier le capteur ; ne pas considérer les données comme valides |
| LCD 0x27 : NACK / délai dépassé | SDA=GPIO3, SCL=GPIO2, masse commune, alimentation, niveaux logiques et adresse du module |
| LCD : autoconfiguration impossible | Adaptateur compatible PCF8574/PCF8574A/MCP23008 et connexions de l'écran |
| `[LCD]` en série mais écran vide ou rectangles | Ajuster doucement le potentiomètre de contraste du module ; vérifier le rétroéclairage et les soudures |

## Vérifications logicielles

`tests/compensation_test.cpp` contient des assertions évaluées à la compilation :
le vecteur Bosch (UT=27898, UP=23843, OSS=0) doit produire exactement
**15,0 °C** et **699,64 hPa**. Il vérifie aussi le rejet d'une calibration nulle,
d'une division par zéro, d'un OSS invalide et de données brutes incohérentes.
Le [code SparkFun](https://github.com/sparkfun/BMP180_Breakout_Arduino_Library/blob/master/src/SFE_BMP180.cpp)
publie également les coefficients de cet exemple Bosch.

Pour l'installation locale de vérification conservée dans `.tools` :

```powershell
$env:PYTHONPATH = Join-Path $PWD '.tools\python'
$env:PLATFORMIO_CORE_DIR = Join-Path $PWD '.tools\platformio'
python -m platformio run
$compiler = '.tools\platformio\packages\toolchain-riscv32-esp\bin\riscv32-esp-elf-g++.exe'
& $compiler -std=gnu++14 -Wall -Wextra -Werror -c tests/compensation_test.cpp -o .pio/compensation_test.o
```

Compilation ESP32-C3 réussie avec la configuration fournie ; les huit assertions
de compensation passent avec le compilateur de cette cible.
Téléversement effectué le 24 octobre 2026 sur COM3 : ESP32-C3 révision 0.4,
flash 4 MB, écriture vérifiée par esptool.

**Test matériel réussi après vérification du câblage de l'alimentation VIN** :
la capture du 24 octobre 2026 à 08:33 UTC sur COM3 contient **15 mesures en
30,1 secondes, sans erreur**, avec température **24,1 à 24,3 °C**, pression
**1008,93 à 1009,05 hPa** et intervalle exact de **2000 ms** entre les mesures
d'après les horodatages du firmware. Journal : `diagnostics/bmp180-com3-retry-vin.log`.
Les mesures confirment le fonctionnement des lectures et de la compensation ;
elles ne constituent pas une vérification métrologique avec un instrument étalon.

Les premières captures avaient montré des délais I²C dépassés (code 5) et
un NACK (code 2) à 0x77 avant la reprise du câblage. Elles restent conservées
dans `diagnostics/` comme historique du diagnostic.

La version BMP180 + LCD compile pour ESP32-C3, ainsi que la variante capteur seul.
Le premier essai après ajout du LCD (`diagnostics/bmp180-lcd1602-com3.log`)
n'a détecté aucun adaptateur LCD et aucun BMP180. L'utilisateur a précisé ensuite
que le BMP180 était débranché et que le montage complet restait à effectuer.
Ce journal ne valide donc pas l'intégration matérielle complète ; l'essai est
à reprendre une fois les deux modules raccordés selon les tableaux ci-dessus.

Après signalement du montage complet, `diagnostics/bmp180-lcd1602-complete.log`
montre toujours des NACK à 0x77 et aucun adaptateur LCD détecté. Un test temporaire
de la variante capteur seul, sans changement de paire I²C, donne également des
NACK à 0x77 (`diagnostics/bmp180-isolation-after-lcd.log`). Cette vérification ne
permet pas encore de valider le montage ; contrôler les connexions, les masses
communes et l'alimentation des deux modules avant de reprendre l'essai.

À la demande de l'utilisateur, l'adresse LCD a ensuite été fixée à **0x3F** et
le firmware téléversé avec succès. Le test ciblé sur SDA=GPIO3 / SCL=GPIO2
retourne un **délai I²C dépassé (code 5)** ; le BMP180 reste sans acquittement
à 0x77 (NACK, code 2). La capture de 25,2 secondes ne contient aucune mesure
valide : `diagnostics/lcd1602-address-0x3f.log`. L'affichage LCD reste non validé.

Un diagnostic indépendant (`tools/i2c-wiring-probe`) a ensuite testé les paires
SDA/SCL 3/2, 2/3, 4/5 et 5/4 aux adresses 0x3F et 0x77. Aucun acquittement n'a
été observé. Sur SDA=3/SCL=2, 0x3F donne un délai dépassé ; les essais BMP180
sur 4/5 et 5/4 donnent des NACK. Les entrées étaient lues hautes au repos avec
les tirages internes activés, ce qui ne mesure pas leur tension et ne prouve pas
que les modules sont raccordés. Journal : `diagnostics/i2c-pins-orientation.log`.
Le bilan automatique de cette capture indique zéro mesure car cet outil ne
produit que des sondages d'adresses ; il faut lire les lignes `[PROBE]`.

Dernier changement demandé : **BMP180 SDA=GPIO4, SCL=GPIO1** (le câblage
initial était GPIO4/5). La version correspondante a été compilée et téléversée.
Sur ce nouveau câblage, le scan rencontre trois délais dépassés et s'interrompt
correctement ; la lecture logique à cet instant est SDA=1 et SCL=0. Les lectures
directes du BMP180 expirent également. Le LCD à 0x3F donne des NACK.
Journal : `diagnostics/bmp180-gpio4-1-short-scan.log`. Ce résultat demande de
vérifier la connexion et les soudures de SCL/GPIO1 ; il ne détermine pas à lui
seul l'origine matérielle ou périphérique de l'état bas observé.

**État du dernier essai après vérification de 3V3 : BMP180 fonctionnel sur
GPIO4/GPIO1, LCD toujours non détecté sur GPIO3/GPIO2.** Une première capture
a reçu une mesure puis subi une interruption USB. Après reconnexion, 15 mesures
valides ont été reçues à 2000 ms d'intervalle ; les deux erreurs relevées étaient
des NACK du LCD (`diagnostics/bmp180-lcd-after-3v3-reconnect.log`).
La recherche des autres adresses LCD usuelles a ensuite été rétablie, en gardant
0x3F prioritaire, et le firmware téléversé. La capture suivante contient 12 mesures
BMP180 en 25,1 s, température 24,5..25,9 °C, pression 1006,76..1006,96 hPa,
intervalle 2000 ms ; les trois erreurs concernent uniquement l'absence du LCD
dans 0x20..0x27 / 0x38..0x3F (`diagnostics/bmp180-working-lcd-address-scan.log`).
L'intégration LCD reste à valider après contrôle de ses connexions et soudures.

Dernier essai demandé : **0x27 prioritaire pour le LCD**, firmware compilé et
téléversé. Le module ne répond pas à 0x27, ni aux autres adresses usuelles
recherchées ensuite. Le BMP180 continue à fournir 12 mesures valides en 25,2 s,
température 24,3..24,4 °C et pression 1006,67..1006,75 hPa, toutes les 2000 ms.
Les trois erreurs concernent l'absence du LCD. Journal :
`diagnostics/lcd1602-address-0x27.log`.

À la suite des liens Freenove et de la capture de l'exemple `LiquidCrystal_I2C`,
un projet de comparaison LCD seul a été préparé dans
`tools/lcd1602-freenove-test/`. Il fixe SDA=GPIO3/SCL=GPIO2, sonde 0x3F puis
0x27 avant d'appeler `init()`, `backlight()` et `print()`. Ce projet se compile
séparément. Après passage du VCC LCD sur 3V3, l'utilisateur a confirmé **3,32 V
sur SDA et SCL**. Le test LCD seul sur GPIO3/GPIO2 a alors été téléversé : les
deux adresses donnent des NACK au redémarrage et l'utilisateur ne voit aucun
texte. Journal : `diagnostics/lcd-only-at-3v32-startup.log`.
Une variante `lcd_test_known_pins` sur GPIO4/GPIO1 a ensuite été testée avec
scan complet, sans périphérique détecté. L'utilisateur a précisé ensuite que
SCL était physiquement sur GPIO3. La variante `lcd_test_gpio4_3`, SDA=GPIO4 et
SCL=GPIO3, a donc été compilée, téléversée et testée : NACK à 0x3F et à 0x27,
puis scan complet 0x08..0x77 avec zéro périphérique et zéro erreur de bus.
À cette étape, la carte contenait le test LCD seul sur GPIO4/GPIO3 et le BMP180
était temporairement débranché. Journal : `diagnostics/lcd-only-gpio4-3-at-3v3.log`.

L'utilisateur a ensuite rebranché **le BMP180 seul**, SDA=GPIO4 / SCL=GPIO3.
La variante capteur seul a été adaptée, compilée et téléversée. Le scan complet
ne détecte aucun périphérique et les tentatives à 0x77 donnent des NACK :
zéro mesure, 14 erreurs en 25 secondes (`diagnostics/bmp180-only-gpio4-3.log`).
Après retour de SCL sur GPIO1 confirmé par l'utilisateur, la variante de
référence `enviroterminal_sensor_gpio4_1` a été téléversée. Le scan ne détecte
toujours aucun périphérique ; les lectures à 0x77 donnent des NACK, zéro mesure
et 14 erreurs en 25 secondes (`diagnostics/bmp180-return-gpio4-1.log`).
La carte était alors en diagnostic BMP180 seul sur SDA=GPIO4/SCL=GPIO1.
Ces résultats ne permettent pas de conclure à un défaut propre à GPIO3.
Vérifier maintenant la tension directement entre VIN et GND sur le BMP180,
puis la continuité des connexions et soudures hors tension.
Le schéma Freenove ESP32-WROVER montre un raccordement direct avec LCD alimenté
en 5 V ; cette illustration ne remplace pas les caractéristiques électriques
du C3 ni les tensions effectivement rapportées sur le montage de l'utilisateur.

Ces assertions vérifient le calcul sur la cible ESP32-C3. La validation des
échanges I²C, des mesures réelles et du montage nécessite l'exécution sur la carte.

Référence des appels et délais I²C :
[documentation Arduino-ESP32 de Espressif](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/i2c.html).
