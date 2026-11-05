# Recepteur meteo LoRa 433 MHz

**Dernier essai avec la sonde a 8 dBm :** sur batterie au meme emplacement
a environ 7 m derriere un mur, deux paquets successifs valides a -119 dBm,
SNR +6,50 / +6,75 dB, sans nouvelle erreur CRC. Reference a 2 dBm :
-127 dBm / -4,25 dB. Voir
[rapport puissance et comparaison](../../diagnostics/lora-power-8dbm-20261026.md).

**Essai precedent du 26 octobre, a 2 dBm :** la sonde emet sur batterie ; le signal
chute d'environ 19 dB entre la proximite et le placement a environ 7 m
derriere un mur, sur la meme alimentation. Derniere mesure recue :
22,7 degres C / 1008,53 hPa, RSSI=-127 dBm, SNR=-4,25 dB.
Les pertes restent a resoudre. Voir
[rapport de comparaison](../../diagnostics/lora-loss-20261026.md).

**Retest du 26 octobre : reception revenue, liaison encore intermittente.**
Deux nouvelles mesures valides, derniere valeur : 23,9 degres C / 1008,80 hPa,
RSSI=-106 dBm, SNR=+9,75 dB ; un autre envoi est manque, sans erreur SPI observee.
Voir [rapport du retest](../../diagnostics/lora-retest-20261026.md).
Les autres receptions decrites ensuite constituent l'historique precedent.

Projet dedie a la seconde ESP32-C3 SuperMini, avec le meme XL1278-SMT que
la sonde. L'utilisateur a corrige la bande du module : **433 MHz**.
Le canal commun est **433920000 Hz**, avec largeur de bande **31250 Hz**,
defini dans `firmware/WeatherRadioConfig.h`. Les observations a 169 MHz
ci-dessous constituent l'historique anterieur a cette correction.

**Essai a 433,92 MHz : reception retablie.** Les deux cartes sont
reprogrammees ; COM4 valide les JSON de la sonde et les publie sur
http://192.168.1.197/. Premiere mesure observee apres le redemarrage de
test : 22,9 degres C et 1008,24 hPa. Le Wi-Fi de la sonde reste connecte
sur http://192.168.1.65/ pendant l'emission LoRa.

## Tableau de bord sur le recepteur

**Reception meteo confirmee au nouvel essai du 25 octobre, capture de
18:30:05 UTC :** paquet JSON `meteo-1`, `seq=1`, `uptime_ms=120718`,
temperature **24,8 degres C**, pression **1007,16 hPa**, identique a la
charge utile de COM3. L'emetteur confirme TxDone ; COM4 compte son troisieme
paquet, de 95 octets, RSSI **-123 dBm**, SNR **0,75 dB**, et annonce
`Mesure LoRa valide publiee sur le tableau de bord.`
Les premiers deux paquets comptes n'avaient pas ete captures en entier.
Aucun firmware n'a ete modifie pendant ce retest. Cette observation confirme
un echange complet ; elle ne constitue pas encore un test de portee ou de
fiabilite sur une longue duree. Voir les deux journaux `diagnostics/lora-link-*.log`.

Le recepteur a ensuite ete configure par Bluetooth et a rejoint la box a
**192.168.1.197**. Les API `/api/status` et `/api/device` ont ete verifiees
sur cette adresse : source `lora`, quatre mesures valides, zero erreur,
SPI et Wi-Fi OK. Derniere mesure observee : **24,6 degres C, 1007,17 hPa**,
RSSI=-124 dBm, SNR=0,5 dB. La page reelle a ete ouverte et inspectee :
elle affiche `Sonde recue`, `Mesure recue par LoRa` et ces valeurs.
Etat enregistre dans `diagnostics/receiver-dashboard-live-status.json`.

Le recepteur sur COM4 heberge maintenant le tableau de bord Wi-Fi. Il valide les
paquets JSON v1 de `meteo-1`, leurs champs numeriques et les plages du BMP180
(-40..85 degres C et 300..1100 hPa), puis publie temperature, pression et signal
LoRa dans `/api/status`. Il n'utilise aucun capteur BMP180 local.
L'interface affiche les dernieres mesures et leur age ; apres **180 secondes**
sans nouvelle mesure valide, l'API renvoie des valeurs nulles et la page les masque.
Le nom local est **enviro-78b4b4.local**. Une fois connecte a la box :
`http://enviro-78b4b4.local/` (ou l'IP annoncee sur USB).

Configurer le Wi-Fi avec ESP BLE Provisioning, appareil **PROV_Enviro_78b4b4**.
Le QR est dans `.tools/private/receiver-ble-setup-qr.png`, ignore par Git.
Le code de possession et le mot de passe Wi-Fi ne sont pas publies dans l'API.
BOOT maintenu trois secondes ou la commande USB `wifi-setup` relance la
configuration Bluetooth. DIO0 doit rester debranche pour liberer GPIO9/BOOT.

La compilation et le televersement de cette version ont reussi le 25 octobre
2026. Le recepteur est en ecoute LoRa LF (`RegOpMode=0x8E`), SPI stable.
La connexion a la box par QR et l'acces HTTP reel sont maintenant confirmes.
Le comportement de l'interface a aussi ete
verifie par `tests/dashboard_state_test.cjs` avec des donnees de test.

**Premier blocage observe :** l'emetteur COM3 lit `RegVersion=0x12`,
mais ses essais repetes de changement de mode donnent `00/00`, `08/08`,
`88/08` (valeur ecrite / relue). Le bit LoRa 0x80 n'est pas conserve,
meme apres sommeil FSK explicite, delais et SPI ramene a 100 kHz.
Le firmware refuse alors l'emission et retente dix secondes plus tard.
Ce resultat ne suffit pas a conclure a une panne ou a identifier la puce :
verifier visuellement la reference du module/puce et son montage sur la sonde.
La sonde conserve des mesures BMP180 valides et sa connexion Wi-Fi.

**Nouvel essai du 25 octobre, captures de 16:59 UTC :** sans changement de
firmware pendant ce retest, l'API de l'emetteur annonce d'abord la radio prete
et le recepteur compte un paquet recu. Le contenu de ce premier paquet n'a
pas ete capture et sa validation meteo n'est donc pas confirmee.
A l'envoi suivant (`seq=1`, `uptime_ms=210723`, 24,4 degres C, 1005,21 hPa),
la sonde signale une perte de SPI. Les nouvelles tentatives lisent ensuite
`RegVersion=0xF6` et `RegOpMode=0x7B`, tandis que le compteur du recepteur
reste a un paquet. La radio n'est donc pas stable. Il faut verifier les
connexions, soudures, alimentation et RESET du module sur la sonde avant
d'attribuer le defaut au seul changement de mode LoRa.

| Module XL1278-SMT | ESP32-C3 |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| SCK | GPIO7 |
| MISO | GPIO5 |
| MOSI | GPIO6 |
| NSS | GPIO8 |
| REST / RESET | GPIO10 |
| DIO0 | Laisser debranche (lecture des IRQ en SPI) |

**Essai du 25 octobre 2026 :** ce cablage a ete confirme par l'utilisateur,
le firmware a compile et a ete televerse sur COM4 (ESP32-C3, MAC
e8:3d:c1:78:b4:b4, flash 4 Mo), avec verification des ecritures.
La capture qui suit montre uniquement `boot:0x7 (DOWNLOAD(USB/UART0/1))`
puis `waiting for download`. Lors de ce premier essai, le programme recepteur
n'avait donc pas demarre.
Journal : `diagnostics/lora-receiver-com4-169mhz.log` depuis la racine du depot.

GPIO9 est une broche de selection du mode de demarrage : si elle est maintenue
basse au reset, l'ESP32-C3 entre dans le chargeur serie. Le raccordement DIO0
etait une cause probable. L'utilisateur a ensuite debranche uniquement
DIO0 de GPIO9 hors tension et rebranche l'USB sans maintenir BOOT.
Le firmware existant fonctionne sans ce fil, car il interroge la radio en SPI.
Voir [Espressif : choix du mode de demarrage](https://docs.espressif.com/projects/esptool/en/latest/esp32c3/advanced-topics/boot-mode-selection.html).

**Reprise reussie :** la capture de 25 secondes apres cette modification montre
le programme en execution et trois messages `[RX] SPI OK ; aucun paquet recu
pour le moment.`, sans erreur. Le demarrage et la communication SPI sont
valides. Aucun paquet n'etait recu pendant ce premier essai sans emetteur configure.
Journal : `diagnostics/lora-receiver-com4-dio0-disconnected.log`.

Le recepteur n'utilise pas de BMP180 ni de LCD. Le firmware
n'appelle aucune fonction d'emission. Utiliser une antenne adaptee a 433 MHz.
Effectuer les connexions hors tension.

Reglages communs a reproduire sur la sonde : SF7, largeur de bande 31,25 kHz,
codage 4/5, preambule 8 symboles, sync word 0x12, CRC active, entete explicite,
IQ normal. Le recepteur affiche aussi le contenu des paquets, le RSSI et le SNR
sur le port serie a 115200 bauds. Les octets non imprimables sont echappes en
hexadecimal. Le filtre de format et de nom de sonde n'est pas une authentification.

Compilation depuis la racine du depot :

```powershell
pio run -d firmware/WeatherReceiver
pio run -d firmware/WeatherReceiver -t upload --upload-port COM4
pio device monitor --port COM4 --baud 115200
```

La frequence commune est definie dans `firmware/WeatherRadioConfig.h` et peut
etre surchargee par WEATHER_FREQUENCY_HZ dans les options de compilation.
Une detection RegVersion=0x12 ne suffit pas a valider la liaison radio.

## Firmware emetteur de la sonde

Le profil racine `enviroterminal_meteo_tx` conserve le BMP180 sur SDA=4/SCL=3
et le brochage radio d'origine : SCK=6, MISO=5, MOSI=7, NSS=10, RESET=0.
DIO0 n'est pas requis. Ce profil est distinct de celui installe sur COM4.
Il envoie la premiere mesure valide, puis au plus une mesure toutes les deux minutes,
avec une puissance configuree de 8 dBm nominaux et les memes parametres radio.
La consigne est definie dans `WeatherRadioConfig.h`, puis verifiee sur la
sonde par relecture des registres PA/DAC. Les anciennes captures a2dBm
ci-dessous restent historiques.
Les mesures locales restent cadencees a deux secondes. L'emission est
asynchrone et surveillee avec une limite de deux secondes, sans acquittement.
Une mesure invalide n'est jamais envoyee. Aucun identifiant Wi-Fi n'est efface.

Charge utile JSON v1, par exemple (valeurs illustratives) :

```json
{"v":1,"node":"meteo-1","seq":0,"uptime_ms":3200,"temperature_c":24.3,"pressure_hpa":1008.92}
```

Le profil installe sur la sonde COM3 (MAC e8:3d:c1:81:ca:70) est
**enviroterminal_local_web_meteo_tx**, qui conserve son tableau de bord Wi-Fi
et ajoute l'emission LoRa. Le profil `enviroterminal_meteo_tx` fonctionne
sans Wi-Fi. Une antenne adaptee doit etre raccordee avant toute emission.
Compilation et televersement sur COM3 reussis le 25 octobre 2026.
Lors du blocage initial, l'API reelle de la sonde confirmait BMP180 OK, Wi-Fi connecte et
`radio_tx_enabled=true`, mais `radio_spi_ok=false` car l'initialisation radio
complete n'etait pas validee. Observation a ce stade : 23,7 degres C et 1004,76 hPa.
Les journaux `diagnostics/lora-link-com3.log` et
`diagnostics/lora-link-com4.log` conservent le test simultane des deux cartes.

```powershell
pio run -e enviroterminal_meteo_tx
pio run -e enviroterminal_local_web_meteo_tx -t upload --upload-port COM3
```

Le canal commun est 433,92 MHz. L'intervalle de 120 s laisse moins
de 1 % d'occupation pour la taille bornee des paquets et les reglages choisis.

Verification du comportement de l'interface : `node tests/dashboard_state_test.cjs`
(mesures distantes, seuil exact de peremption, mode local et perte reseau).
`short_paths.py` garde les arguments d'inclusion courts pour la compilation
sous Windows ; il ne modifie pas le firmware.

Pilote : copie locale MIT d'[Arduino-LoRa 0.8.0](https://github.com/sandeepmistry/arduino-LoRa/tree/0.8.0)
dans `lib/LoRa169`, avec mode LF conserve et ordre NSS/transaction SPI corrige.
Les controles de l'application verifient les registres avant toute emission.

Diagnostic radio du recepteur : **http://192.168.1.197/api/radio**.
Cette route en lecture seule expose le mode, la frequence relue, les
parametres du modem, les compteurs materiels et le RSSI instantane du canal.
Ce dernier n'est pas le RSSI d'un paquet ; le signal du dernier paquet
valide reste dans `/api/status`. Au test du 25 octobre apres reparation
MISO de la sonde, le recepteur confirme la configuration attendue a
169437500 Hz, mais aucun paquet a l'instant du premier releve.

Le diagnostic expose aussi `rx_done_irq_count`, `crc_error_irq_count` et
`rx_timeout_irq_count`, tires de la lecture IRQ deja faite par le pilote,
avant acquittement. Les timeouts RX sont normaux entre deux emissions.
Le dernier evenement RxDone (meme si CRC incorrect) conserve RSSI, SNR,
decalage de frequence estime par le modem, flags et age. Ces champs valent
null tant qu'aucun RxDone n'a ete observe ; une erreur CRC n'est jamais
publiee comme mesure meteo. Les reglages radio et la cadence sont inchanges.
