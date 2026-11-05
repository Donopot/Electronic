# Quatre sondes de terre sur le Heltec

Le profil `heltec_v3_soil_lora` reçoit SOIL-01 à SOIL-04, avec valeurs,
sessions, compteurs et péremptions indépendants. NON AUTH reste affiché :
identifiants et CRC ne constituent pas une authentification.

| Carte | Profil XIAO | Premier cycle | Cycles suivants |
| --- | --- | --- | --- |
| SOIL-01 | `xiao_soil_lora_pmos` | 30 s | 30 min après la fin du cycle précédent |
| SOIL-02 | `xiao_soil_lora_pmos_node2` | 35 s | 30 min après la fin du cycle précédent |
| SOIL-03 | `xiao_soil_lora_pmos_node3` | 40 s | 30 min après la fin du cycle précédent |
| SOIL-04 | `xiao_soil_lora_pmos_node4` | 45 s | 30 min après la fin du cycle précédent |

PS v1 reste une trame de vingt octets ; l'octet 3 contient 1 à 4, autres
identifiants rejetés. Les anciens programmes des trois premières sondes
restent compatibles sans reflash. Le [montage AO3401](soil-pmos.md) ne change
pas. Défaut, doublon et nouvelle session ne modifient que le nœud concerné ;
expiration individuelle après **90 min** sans nouveau paquet accepté.
Le décalage initial ne garantit pas la livraison ; pas d'ACK ni de répétition.

## Écran

Huit lignes de huit pixels sur 128×64 : titre/PRG, lux, NON AUTH, SOIL-01,
SOIL-02, SOIL-03, SOIL-04, radio. Par exemple `SOIL-04 2.698 V 0m` ; tension
arrondie à trois décimales et âge en minutes. ATTENTE, DEFAUT ADS ou PERIMEE
remplacent la valeur du seul nœud concerné. Les bruts et tensions complètes
restent sur USB. Redémarrer le hub impose d'attendre les prochains paquets ;
aucun pourcentage d'humidité n'est calculé.

## Compilation et installation

Depuis la racine du dépôt, avec Python 3.12 et les paquets locaux installés.
Utiliser la même installation PlatformIO pour les deux projets :

```powershell
$env:PYTHONUTF8 = '1'
$env:PYTHONPATH = Join-Path $PWD '.tools/xiao-diagnostic-python'
$env:PLATFORMIO_CORE_DIR = Join-Path $PWD '.tools/platformio'
python -m platformio run --project-dir firmware/XiaoSoilDiagnostic -e xiao_soil_diagnostic -e xiao_soil_lora_tx -e xiao_soil_lora_pmos -e xiao_soil_lora_pmos_node2 -e xiao_soil_lora_pmos_node3 -e xiao_soil_lora_pmos_node4
python -m platformio run --project-dir firmware/PlantStationVeml7700 -e heltec_v3_text -e heltec_v3_json -e heltec_v3_soil_lora -e heltec_v3_soil_dashboard
python tools/run_soil_lora_host_tests.py
python tests/test_capture_soil_lora.py
python tests/test_xiao_battery_variant.py
```

Le contrôle batterie inspecte les six ELF XIAO, à compiler avant lui.
Installer seulement la nouvelle carte et le Heltec, après identification USB,
sauvegarde des applications disponibles, antennes raccordées et batterie
de la nouvelle XIAO isolée. Conserver bootloader, SoftDevice et partitions.

Pour cette quatrième carte, le numéro USB est `0690EC20846A82A8`.
Après passage en bootloader, vérifier ce numéro et VID/PID `2886:0045` ;
`COM16` ci-dessous est un exemple et doit être remplacé par le port vérifié.
Sélectionner explicitement le paquet du profil **node4**, car le profil par
défaut du projet est le diagnostic :

```powershell
python .tools/platformio/packages/tool-adafruit-nrfutil/adafruit-nrfutil.py dfu serial -pkg firmware/XiaoSoilDiagnostic/.pio/build/xiao_soil_lora_pmos_node4/firmware.zip -p COM16 -b 115200 --singlebank
```

Pour le Heltec identifié, écrire seulement l'application à `0x10000` :

```powershell
$env:PYTHONPATH = Join-Path $PWD '.tools/python'
python .tools/platformio/packages/tool-esptoolpy/esptool.py --chip esp32s3 --port COM5 --baud 460800 --before default_reset --after hard_reset write_flash 0x10000 firmware/PlantStationVeml7700/.pio/build/heltec_v3_soil_lora/firmware.bin
```

Débrancher l'USB de la XIAO avant de reconnecter sa batterie.

Les captures comparent par nœud/session/compteur, avec minimum par nœud et
trafic des autres sondes conservé dans `background_rx`. Exiger
`compared_nodes == [4]`, la bannière SOIL-04 et AUTO pour valider cette carte.
Un contrôle court de démarrage ne prouve pas trois périodes réelles de
trente minutes. Les tests couvrent les quatre identifiants, défaut/reprise,
compteurs, péremption exacte, limites de l'écran et sécurité GPIO des quatre
profils P-MOS ; ils ne remplacent pas le contrôle des raccordements.

Pour reproduire un contrôle court, démarrer la capture aussitôt après DFU,
avec le port d'application de cette XIAO identifié par son numéro USB.
Le CLI exige trois paires par nœud : un essai de 105 s avec une seule paire
se termine donc normalement avec le code 2. Employer un préfixe inédit :

```powershell
$env:PYTHONPATH = Join-Path $PWD '.tools/xiao-diagnostic-python'
python tools/capture_soil_lora.py --tx-port COM17 --rx-port COM5 --seconds 105 --output-prefix diagnostics/soil04-demarrage-exemple
@'
import json, sys
from pathlib import Path
sys.path.insert(0, str(Path('tools').resolve()))
from capture_soil_lora import compare
p = Path('diagnostics/soil04-demarrage-exemple')
saved = json.loads(Path(str(p)+'.json').read_text(encoding='utf-8'))
r = compare(*[Path(str(p)+s).read_text(encoding='utf-8').splitlines()
              for s in ('-tx.log', '-rx.log')],
            minimum=1, duration=saved['duration_s'])
assert saved['completed'] and not saved['capture_errors']
assert r['accepted'] and r['compared_nodes'] == [4], r
print(json.dumps(r, indent=2))
'@ | python -
```

Vérifier aussi la bannière USB : `[CONFIG] node=SOIL-04`,
`premier apres 45 s` et `[SOIL_POWER] mode=AUTO gate=1 node=SOIL-04`.
Un second lecteur peut la retrouver à la réouverture, sans purger le tampon.
Pour une validation de cadence réelle, conserver le minimum de trois paires
par nœud et la durée de 5 500 s du CLI ; le contrôle court n'en tient pas lieu.
