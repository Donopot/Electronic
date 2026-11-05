# Tableau de bord du concentrateur (interface à onglets)

Le concentrateur Heltec V3 héberge son propre tableau de bord : aucune compte,
aucun service externe, aucune requête sortante. La page est servie depuis la
flash du Heltec et interroge son API locale toutes les cinq secondes.

Deux profils la fournissent :

| Profil | Capteur de lumière | Usage |
| --- | --- | --- |
| `heltec_v3_plant_dashboard` | VEML7700 présent | concentrateur historique, lux affiché |
| `heltec_v3_soil_dashboard` | **absent** | concentrateur de sondes seul : aucun scan I2C, `lux` toujours `null` |

Le profil `heltec_v3_soil_lora` (sans tableau de bord) reste inchangé.

## Écran OLED (3 écrans, 1 seul QR)

PRG court fait défiler **trois** écrans : visage, détails, QR.

| Écran | Contenu |
| --- | --- |
| Visage | robot selon l’humeur, états courts des quatre sondes (`OK`, `ETAL.`, `ATT.`, `ADC!`, `PERIM.`, `SECHE`), ligne basse `N/4 fraiches PRG:>` (ou le lux si le capteur est présent) |
| Détails | quatre blocs `SOIL-0X 2.701V B--` / état + âge en minutes |
| QR | **un seul** QR, seul sur l’écran, centré, au plus grand format possible |

Contenu du QR unique, par priorité :

1. **Appairage Android** — BLE actif **et** assistant demandé explicitement
   (PRG 8 s) : JSON
   `{"ver":"v1","name":"PLANT-XXXX","transport":"ble","security":1,"pop":"…"}`.
2. **Wi-Fi du kit** (fenêtre de configuration, **réseau ouvert**) : `WIFI:T:nopass;S:PLANTES-XXXX;;`
   — le scan rejoint le réseau sans mot de passe, et le portail captif ouvre
   ensuite l’assistant (ou `/dashboard`).
3. **Accès local** (connecté au Wi-Fi domestique) : `HTTP://<ip>/` — ouvre le
   tableau de bord à onglets depuis un téléphone.
4. Sinon : `WI-FI ABSENT` / `Reconnexion en cours` / `PRG 8s : configurer`,
   sans QR.

L’écran QR n’affiche **que le symbole** : aucun texte, aucune colonne latérale.
Le QR est centré et utilise la plus grande échelle qui tient dans 128 × 64 px
(2 px par module dès que possible, marge réduite à 1 module si nécessaire), sans
quoi un téléphone ne le résout pas. Le nom et la clé du kit ne sont plus écrits
à côté : ils sont dans le QR lui-même, et le nom apparaît de toute façon dans la
liste des réseaux du téléphone.

Règle concrète : au premier démarrage sans Wi-Fi enregistré, le QR est celui du
**Wi-Fi du kit** (utile avec n’importe quel téléphone) ; le QR **Android**
n’apparaît que si l’assistant a été ouvert volontairement par PRG 8 s. Il n’y a
donc jamais deux QR simultanés ni d’écran « assistant » séparé. Sans capteur de
lumière (`PLANT_LUX=0`), la ligne basse du visage compte les sondes exploitables
à la place du lux.

## Connexion du kit (mode hybride)

Le profil `heltec_v3_soil_dashboard` compile `PLANT_OPEN_AP=1` : le Wi-Fi du kit
n’a **pas de mot de passe**, et il n’est ouvert qu’**à la demande**.

1. **Première mise sous tension** (aucun réseau enregistré) : le réseau
   `PLANTES-XXXX` apparaît, sans cadenas. Un appui dessus suffit — aucune clé à
   saisir, aucun QR obligatoire. Le portail captif ouvre l’assistant sur
   `http://192.168.4.1/`, qui propose aussi « Voir le tableau de bord tout de
   suite » (`/dashboard`), sans rien configurer.
2. **Choisir le Wi-Fi de la maison** (2,4 GHz) dans l’assistant. Le Heltec teste
   la connexion, l’enregistre, puis **referme son propre Wi-Fi** : le téléphone
   revient tout seul sur le réseau domestique, Internet conservé.
3. **Ensuite, plus rien à faire** : le Heltec rejoint son Wi-Fi au démarrage, et
   le tableau de bord répond sur `http://<ip>/` ou `http://plant-xxxxxx.local/`
   depuis n’importe quel appareil de la maison. Ajouter la page à l’écran
   d’accueil du téléphone la transforme en icône.
4. **Changer de box ou déménager** : un appui **PRG 8 s** rouvre le Wi-Fi du kit
   pendant **15 minutes**, puis il se referme seul (et ne se rouvre pas tout
   seul : PRG ou redémarrage requis).

Le QR de l’OLED suit cet état : `WIFI:T:nopass;S:PLANTES-XXXX;;` quand le kit
diffuse son réseau, `HTTP://<ip>/` quand il est sur le Wi-Fi de la maison.

Conséquences assumées :

- le Wi-Fi du kit est ouvert : qui est à portée pendant la fenêtre peut lire le
  tableau de bord (tensions, historique) et modifier la configuration. Aucune
  donnée sensible, mais ce n’est pas un réseau privé. Pour un AP chiffré :
  `PLANT_OPEN_AP=0` et recompiler ;
- l’AP n’est **pas** permanent : c’est ce qui évite que le téléphone saute sur le
  kit et perde Internet à chaque passage.

Le Bluetooth (BLE) ne sert qu’à l’appairage de l’application Android : afficher
le tableau de bord en Bluetooth exigerait un service GATT dédié **et** une
application native ou une page HTTPS tierce (Web Bluetooth), ce qui n’est pas
implémenté. Le QR d’appairage Android n’apparaît donc que sur demande explicite
(PRG 8 s).

## Onglets

1. **Vue d’ensemble** — résumé, quatre cartes (état, tension, âge, RSSI/SNR,
   mini-courbe 24 h) et tableau des derniers paquets. Après un redémarrage,
   un bandeau rappelle la dernière mesure persistée de chaque sonde.
2. **Sondes** — tableau complet : tension, valeur brute, adresse ADS, session,
   séquence, RSSI, SNR, âge, date de la dernière mesure, puis le contenu exact
   de la dernière mesure persistée en NVS.
3. **Historique** — quatre courbes comparées (24 h / 7 j / 30 j) lues dans le
   journal flash, légende par sonde, export CSV. Un trou de plus de 40 minutes,
   un défaut ADC ou un redémarrage coupe la courbe.
4. **Radio** — paramètres SX1262 réellement compilés (868,1 MHz, BW 125 kHz,
   SF7, CR 4/5, 8 dBm, CRC activé), trames acceptées/rejetées et motifs de
   rejet comptés par le concentrateur.
5. **Système** — mémoire libre et minimum depuis le boot, état du journal
   flash (trames non enregistrées, écritures interrompues), Wi-Fi (signal,
   canal, adresse, mDNS), carte (puce, révision, flash, version), horloge et
   synchronisation manuelle.
6. **Réglages** — noms des quatre sondes, enregistrés en NVS, et aide PRG.

## API locale (version 2)

`GET /api/status` — un seul document, réservé à l’hôte LAN ou au nom mDNS.

| Bloc | Contenu |
| --- | --- |
| racine | `api_version`, `source`, `mood`, `dry_mask`, `warning_mask`, `radio_ready`, `uptime_ms`, `utc_ms`, `lux`, `lux_available`, `history_state`, `history_dropped`, `history_corrupt`, `names_persistence`, `clock_persistence` |
| `plants[]` | `id`, `name`, `state`, `label`, `age_ms`, `microvolts`, `adc_status`, `raw`, `ads_address`, `session`, `sequence`, `rssi_dbm`, `snr_db`, `packets_accepted`, `packets_rejected`, `battery_percent` (toujours `null`), `last_known{present,utc_ms,microvolts,adc_status,session,sequence}` |
| `radio` | `ready`, `frequency_mhz`, `bandwidth_khz`, `spreading_factor`, `coding_rate`, `power_dbm`, `crc`, `accepted`, `rejected`, `reject_reasons{length,header,node,session,status,address,raw,duplicate,old_sequence}` — `null` si la radio n’est pas compilée |
| `wifi` | `connected`, `rssi_dbm`, `channel`, `ip`, `hostname`, `mdns` |
| `system` | `chip`, `revision`, `flash_bytes`, `heap_free`, `heap_min`, `boot`, `node_count`, `version` |

`GET /api/history?node=N&days=D&after=C`

- `N` : `1` à `4` pour une sonde, **`0` pour les quatre** (un seul curseur,
  chaque enregistrement porte son `node`).
- `D` : `1`, `7` ou `30`. `C` : curseur opaque rendu par `next`.
- 96 enregistrements au maximum par page ; `more` indique s’il en reste.
- Chaque enregistrement : `node`, `utc_ms` (ou `null` si l’horloge n’est pas
  connue), `boot`, `uptime_ms`, `microvolts` (ou `null` en défaut ADC),
  `adc_status`.

`POST /api/time` (`utc_ms`) et `POST /api/plants` (`id`, `name`) restent
protégés par l’en-tête `X-Plant-Request: 1`, le type de contenu, l’origine et
l’hôte. Aucun identifiant Wi-Fi n’est exposé.

## RSSI et SNR

PS v1 ne transporte ni RSSI, ni SNR, ni ACK. Ces deux valeurs sont mesurées par
le SX1262 du concentrateur au moment de la réception, conservées en RAM par
sonde, et **jamais écrites dans le journal** : elles ne viennent pas de la
sonde et ne sont pas comparables d’une antenne à l’autre. Une ligne
`[SOIL_SIGNAL]` les journalise aussi sur l’USB.

## Persistance

| Donnée | Support | Durée |
| --- | --- | --- |
| Historique des mesures | journal flash (`spiffs`, anneau de secteurs CRC) | 30 jours glissants |
| Dernière mesure par sonde | NVS `plant-last` (une écriture par minute au plus) | jusqu’au prochain paquet |
| Noms des sondes | NVS `plant-names` | permanente |
| Horloge | ancrage CRC dans le journal | par démarrage |

La dernière mesure NVS survit au redémarrage et s’affiche comme « dernière
connue » tant qu’aucune trame n’a été acceptée depuis le boot. Le journal, lui,
reste la source des courbes.

## Compilation et installation

Depuis la racine du dépôt :

```powershell
python -m platformio run --project-dir firmware/PlantStationVeml7700 -e heltec_v3_soil_dashboard
python .tools/platformio/packages/tool-esptoolpy/esptool.py --chip esp32s3 --port COM5 --baud 460800 --before default_reset --after hard_reset write_flash 0x10000 firmware/PlantStationVeml7700/.pio/build/heltec_v3_soil_dashboard/firmware.bin
```

Écrire uniquement l’application à `0x10000` conserve bootloader, table de
partitions et journal. Sauvegarder la flash avant la première installation
d’un profil différent.

## Limites assumées

- Aucun pourcentage d’humidité : une tension seule n’en est pas un.
- Aucun ACK ni répétition : un paquet perdu en l’air n’est compté nulle part.
- `NON AUTH` : identifiants et CRC n’authentifient pas l’émetteur.
- Les valeurs d’un nœud expiré restent affichées comme périmées, jamais
  effacées.
- Le profil sans capteur de lumière ne publie ni lux ni scan I2C : le capteur
  doit être physiquement retiré, sinon il reste simplement ignoré.
