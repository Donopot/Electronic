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
