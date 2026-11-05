# Ajout du XL1278-SMT — premier diagnostic SPI

**Puissance actuelle : 8 dBm nominaux sur la sonde**, installes le26octobre,
registres PA=0x86 et DAC=0x84 verifies, sans changement de frequence ni
de cadence. Sur batterie au meme emplacement a environ 7 m derriere le
mur : deux paquets consecutifs a -119 dBm, SNR +6,50 / +6,75 dB,
sans nouvelle erreur CRC ; reference precedente a 2 dBm : -127 / -4,25.
Les anciennes captures a 2 dBm restent historiques.
Voir [diagnostics/lora-power-8dbm-20261026.md](diagnostics/lora-power-8dbm-20261026.md).

**Comparaison precedente du 26 octobre, a 2 dBm :** meme batterie, reception a proximite
a -108 dBm / +9,75 dB, puis a environ 7 m derriere un mur a -127 dBm / -4,25 dB.
Un paquet arrive dans ce dernier placement, mais la liaison reste fragile.
Voir [diagnostics/lora-loss-20261026.md](diagnostics/lora-loss-20261026.md).

**Retest precedent du 26 octobre : reception revenue mais intermittente.**
Deux nouvelles mesures valides, derniere valeur : 23,9 degres C / 1008,80 hPa,
RSSI=-106 dBm, SNR=+9,75 dB ; un autre envoi manque, sans erreur SPI observee.
Voir [diagnostics/lora-retest-20261026.md](diagnostics/lora-retest-20261026.md).

**Reglage actuel : 433,92 MHz sur les deux cartes.** L'utilisateur a corrige
la bande du module, precedemment indiquee a 169 MHz. La valeur partagee
se trouve dans `firmware/WeatherRadioConfig.h`. L'historique a 169 MHz est
conserve ci-dessous ; la soudure MISO de la sonde a aussi ete reparee.
Les deux cartes ont ete reprogrammees a 433,92 MHz et les premiers paquets
meteo sont a nouveau recus et publies sur http://192.168.1.197/.
Detail du test : [diagnostics/lora-433mhz-20261025.md](diagnostics/lora-433mhz-20261025.md).
Une perte ulterieure de reception est ensuite constatee : les emissions
locales se terminent mais aucun paquet complet n'arrive. Le nouveau
diagnostic, avec essais de redemarrage RX et compteurs IRQ, est dans
[diagnostics/lora-outage-433mhz-20261025.md](diagnostics/lora-outage-433mhz-20261025.md).
Lors de l'essai du25octobre a un metre, la sonde presente une nouvelle perte SPI :
version0x00/0x00 meme apres reprise du bus et reset radio ; les emissions
sont suspendues. Aucun firmware n'a ete modifie pendant cet essai.

La photo fournie identifie un **XL1278-SMT**, avec puce marquee SX1278 et
reperes de variantes **433M / 169M**. L'annonce Amazon fournie mentionne aussi
868 MHz : cette indication ne suffit pas pour parametrer la radio, car Semtech
specifie le SX1278 de 137 a 525 MHz. La variante RF et l'antenne doivent etre
confirmees sur le materiel recu avant un essai radio.

## Cablage propose, USB debranche

Utiliser les **noms graves au dos du module**, pas la position apparente des
pastilles : les vues avant et arriere de la fiche sont en miroir.

| XL1278-SMT | ESP32-C3 SuperMini |
| --- | --- |
| VCC | 3V3, alimentation 3,3 V |
| GND | GND commun avec ESP32 et BMP180 |
| SCK | GPIO6 |
| MISO | GPIO5 |
| MOSI | GPIO7 |
| NSS | GPIO10 |
| REST / RESET | GPIO0 |
| DIO0 | GPIO1, facultatif pour ce diagnostic SPI |

DIO1, DIO2, DIO3, DIO4 et DIO5 ne sont pas utilises par ce premier test.
Le BMP180 conserve VIN=3V3, GND commun, SDA=GPIO4 et SCL=GPIO3.
Ce profil desactive le LCD et n'initialise ni Wi-Fi ni Bluetooth.

Le circuit SX1278 utilise des niveaux de 3,3 V ; ne pas relier VCC ou ses
signaux a 5 V. Le rail 3V3 doit rester stable. Lors d'un futur essai d'emission,
verifier sa capacite a fournir les pointes de courant et raccorder une antenne
adaptee a la bande du module. Aucune emission n'est programmee ici.

## Ce que verifie le firmware

1. SPI explicite a 1 MHz, mode 0, sur GPIO6/5/7/10 ; reset sur GPIO0.
2. Deux lectures du registre `RegVersion`, adresse 0x42, attendu 0x12.
3. Mise en sommeil par `RegOpMode=0x00`, puis relecture du mode et de la version.
4. Controle toutes les 10 secondes, avec nouvelle tentative apres erreur.
5. Mesures BMP180 toutes les 2 secondes, avec les diagnostics serie existants.

Le test du registre d'identification valide la communication avec la famille
SX127x ; il ne distingue pas a lui seul SX1276 et SX1278 et ne valide ni bande,
antenne, portee, emission ou reception. Aucune frequence n'est configuree.
Les erreurs SPI du module radio n'arretent pas les mesures BMP180. Un controle
de compilation interdit le partage accidentel de broches entre SPI et BMP180.

## Compilation et essai

```powershell
pio run -e enviroterminal_bmp_lora_spi
# Apres cablage confirme :
pio run -e enviroterminal_bmp_lora_spi --target upload --upload-port COM3
pio device monitor --port COM3 --baud 115200
```

Dans l'installation locale de ce projet :

```powershell
$env:PYTHONPATH = Join-Path $PWD '.tools\python'
$env:PLATFORMIO_CORE_DIR = Join-Path $PWD '.tools\platformio'
python -m platformio run -e enviroterminal_bmp_lora_spi
```

Le profil du capteur seul reste disponible avec
`-e enviroterminal_sensor_gpio4_3` : ses broches radio ne sont pas initialisees.

## Sources

- [Annonce fournie, avec photos du brochage](https://www.amazon.fr/dp/B0CLXZB2V4)
- [Semtech : SX1278, plage 137-525 MHz](https://www.semtech.com/products/wireless-rf/lora-connect/sx1278)
- [Code du pilote Arduino-LoRa : SPI, RegVersion et RegOpMode](https://github.com/sandeepmistry/arduino-LoRa/blob/master/src/LoRa.cpp)

Etat : les profils `enviroterminal_bmp_lora_spi` et
`enviroterminal_sensor_gpio4_3` ont compile avec succes pour ESP32-C3.
Le diagnostic a ete televerse avec succes sur COM3 le 24 octobre 2026.
La capture de 25 secondes confirme RegVersion=0x12, l'ecriture et la lecture
du mode veille, puis deux controles SPI reussis a 10 secondes d'intervalle.
La communication SPI du module est validee ; aucune liaison radio n'a ete testee.

Le BMP180 etait volontairement debranche, comme confirme par l'utilisateur :
les 14 messages d'erreur I2C et l'absence de mesures sont donc attendus dans
ce montage. Le code de sortie non nul de l'outil de capture concerne ces
erreurs BMP180, pas un echec du diagnostic SPI.
Journal : [capture du premier test](diagnostics/bmp180-lora-spi-first-test.log).

Apres reconnexion du BMP180 sur SDA=GPIO4/SCL=GPIO3, le test combine du
24 octobre 2026 est reussi : capteur detecte a 0x77, ID=0x55, calibration lue,
13 mesures en 25,1 secondes, aucune erreur et intervalles de 2000 ms.
Temperature : 25,9 a 26,5 degres C ; pression : 1003,85 a 1004,00 hPa.
Le module LoRa repond avec RegVersion=0x12 et deux controles SPI reussis
pendant la meme capture, toujours en veille et sans emission radio.
Journal : [test combine BMP180 et LoRa](diagnostics/bmp180-lora-combined-test.log).

## Second noeud : recepteur 169 MHz

Le 25 octobre 2026, l'utilisateur a confirme une seconde ESP32-C3 avec le meme
module XL1278-SMT, variante **169 MHz**. La nouvelle carte est identifiee sur
**COM4**, MAC e8:3d:c1:78:b4:b4, flash 4 Mo.
Son cablage differe de celui de la sonde : MISO=5, MOSI=6, SCK=7, NSS=8,
DIO0=9, RESET=10. Le projet dedie et l'etat du test sont decrits dans
[firmware/WeatherReceiver/README.md](firmware/WeatherReceiver/README.md).

Compilation et televersement reussis. Le premier demarrage restait en mode
telechargement. Apres debranchement de DIO0/GPIO9 hors tension par l'utilisateur,
le recepteur demarre : trois controles SPI reussis et aucune erreur en
25 secondes (`diagnostics/lora-receiver-com4-dio0-disconnected.log`).
Laisser DIO0 debranche : ce firmware interroge la radio par SPI.
Aucun paquet n'est encore recu ; la liaison radio reste a tester.

Les deux cartes ont ensuite ete programmees pour **169437500 Hz**, SF7,
BW31,25kHz, CR4/5, sync 0x12, CRC. La sonde emet la premiere mesure valide
puis au plus une toutes les deux minutes, a 2 dBm. Configuration commune :
`firmware/WeatherRadioConfig.h`.

La sonde COM3 contient `enviroterminal_local_web_meteo_tx`, qui conserve son
tableau de bord existant ; l'API reelle confirme les mesures et le Wi-Fi.
Le recepteur COM4 contient maintenant le tableau de bord
alimente par les mesures LoRa, avec configuration Wi-Fi par Bluetooth
(`PROV_Enviro_78b4b4`, nom local `enviro-78b4b4.local`). Compilation et
televersement des deux profils reussis. Lors de ce premier essai, la connexion
du recepteur a la box restait a effectuer par Bluetooth.

Diagnostic initial avant reprise : le recepteur est en mode LoRa LF 0x8E, mais la sonde
ne conserve pas le bit LoRa de RegOpMode : ecritures/relectures 00/00,
08/08, 88/08, repetees apres reset avec SPI a 100 kHz et delais explicites.
L'emission etait donc inhibee ; aucun paquet n'avait ete valide. L'API de la sonde
annoncait alors `radio_spi_ok=false`, tout en conservant le BMP180 et le
Wi-Fi fonctionnels. Verifier la reference de la puce et le montage de la sonde.
Voir les journaux `diagnostics/lora-link-com3.log` et `lora-link-com4.log`.
Le pilote local `lib/LoRa169` conserve le bit basse frequence dans tous les
modes et configure SPI avant NSS bas ; les fichiers et la licence MIT sont
conserves dans le depot.

Nouvel essai du 25 octobre, vers 16:59 UTC : le recepteur compte maintenant
un paquet et l'emetteur est initialement pret. Le contenu du premier paquet
n'a pas ete observe. A la tentative suivante (JSON seq=1, uptime_ms=210723),
la communication SPI de la sonde est perdue : version relue 0xF6 et mode
0x7B aux nouvelles tentatives, compteur recepteur toujours a un.
La liaison est intermittente ; verifier le montage et l'alimentation de la
radio de la sonde. Aucun changement de firmware n'a ete fait pendant ce retest.

Retest suivant du 25 octobre, capture de 18:00:46 UTC pendant 30 secondes :
COM3 lit maintenant RegVersion=0x00 puis 0xFF au lieu de 0x12 ; les
relectures des modes alternent aussi 0x00/0xFF. Aucune emission validee.
COM4 reste en ecoute et son compteur reste a un ancien paquet, datant
de plus d'une heure. Le BMP180 et le Wi-Fi de la sonde fonctionnent
(environ 25,0 degres C, 1006,5 hPa, zero mesure echouee dans l'API).
Verifier d'abord l'alimentation directement entre VCC et GND du XL1278,
puis ses connexions SPI et RESET. Aucun firmware modifie pendant cet essai.

**Reception meteo confirmee au nouveau retest, capture de 18:30:05 UTC :**
COM3 envoie le JSON `meteo-1`, seq=1, uptime_ms=120718, 24,8 degres C,
1007,16 hPa, puis confirme la fin d'emission. COM4 recoit exactement le
meme JSON (95 octets, paquet compte n°3, RSSI=-123 dBm, SNR=0,75 dB) et
publie une mesure meteo valide sur le tableau de bord. Les deux premiers
paquets comptes n'avaient pas ete captures en entier. Aucun firmware n'a
ete modifie pendant ce retest ; la cause precise de la reprise n'est pas
etablie. Cet essai valide un echange complet, pas une portee ni une fiabilite
de longue duree. Journaux : `diagnostics/lora-link-com3.log` et
`diagnostics/lora-link-com4.log`.

Le recepteur a ensuite rejoint la box par configuration Bluetooth et publie
son site sur **http://192.168.1.197/**. API HTTP reelles verifiees :
`measurement_source=lora`, `sample_count=4`, `sensor_ok=true`, SPI et Wi-Fi
OK, zero mesure echouee ; derniere temperature 24,6 degres C, pression
1007,17 hPa, RSSI=-124 dBm et SNR=0,5 dB. La page reelle a ete inspectee
dans le navigateur et affiche ces valeurs avec `Sonde recue`.
L'etat observe est conserve dans `diagnostics/receiver-dashboard-live-status.json`.

## Nouvelle panne signalee ensuite

Le recepteur reste en ligne sur 192.168.1.197, mais annonce zero mesure
depuis son dernier demarrage. La sonde, rebranchee sur COM3, initialise
correctement le modem LoRa et confirme TxDone pour la premiere mesure.
Pourtant aucune mesure n'arrive au recepteur. Vers 67 secondes de
fonctionnement dans la capture de demarrage, COM3 disparait et ne revient
pas pendant l'observation. Aucune cause d'alimentation precise n'est
demontree. Batterie via prise USB indiquee par l'utilisateur.
Le diagnostic detaille et les preuves sont dans
[diagnostics/lora-stop-20261025.md](diagnostics/lora-stop-20261025.md).

Contrainte confirmee : **conserver Wi-Fi et LoRa sur la sonde**. Une correction
de reprise SPI complete et de controle de la configuration toutes les dix
secondes est compilee pour `enviroterminal_local_web_meteo_tx` et ses dix
assertions de validation passent. Elle a ensuite ete installee sur COM3,
ecritures verifiees. La capture de 150 secondes ne releve aucune coupure USB,
mais confirme RegVersion=0x00/0x00 a chaque tentative ; aucune emission.
Mesures utilisateur sur le module : VCC, RESET et NSS a 3,28 V par rapport
a GND au repos. Le prochain controle est la continuite des liaisons SPI,
RESET et masse, hors tension. La variante sans Wi-Fi n'a pas ete installee.

L'utilisateur a ensuite trouve MISO sans continuite et repris sa soudure.
Sur les 150 secondes suivantes, RegVersion=0x12/0x12 reste coherent et
deux emissions se terminent sans erreur SPI. Le recepteur reste toutefois
a zero paquet. Le diagnostic `/api/radio` a ete installe sur COM4 :
la lecture du 25 octobre a 20:41:23 UTC confirme mode LoRa LF RX 0x8E,
frequence 169437500 Hz, SF7/BW31250/CR4/5/CRC et sync0x12 attendus.
La liaison complete reste a retester ; la reprise MISO a corrige le defaut
SPI observe, sans prouver a elle seule le retablissement de la radio.
