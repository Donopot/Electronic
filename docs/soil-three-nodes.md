# Trois sondes de terre sur le Heltec

Le profil `heltec_v3_soil_lora` reçoit **SOIL-01, SOIL-02 et SOIL-03**,
avec valeurs, sessions, compteurs et péremptions indépendants. Les identifiants
et le CRC n'authentifient pas les émetteurs ; NON AUTH reste affiché.

| Carte | Profil XIAO | Premier cycle | Cycles suivants |
| --- | --- | --- | --- |
| SOIL-01 | `xiao_soil_lora_pmos` | 30 s | 30 min après la fin du cycle précédent |
| SOIL-02 | `xiao_soil_lora_pmos_node2` | 35 s | 30 min après la fin du cycle précédent |
| SOIL-03 | `xiao_soil_lora_pmos_node3` | 40 s | 30 min après la fin du cycle précédent |

Le décalage initial ne garantit pas la livraison. Le format reste **PS v1,
vingt octets**, avec l'identifiant 1, 2 ou 3 dans l'octet 3 ; les autres sont
rejetés. Les anciens programmes SOIL-01 et SOIL-02 restent compatibles sans
reflash. Le [câblage AO3401 et les contrôles](soil-pmos.md) sont identiques.
Un défaut, doublon ou redémarrage ne modifie que la sonde concernée. Une mesure
devient périmée après **90 min** sans nouveau paquet accepté pour ce nœud.

## Écran compact

Sept lignes sur 128×64 : titre/PRG, lux, NON AUTH, SOIL-01, SOIL-02, SOIL-03,
état radio. Chaque sonde affiche par exemple `SOIL-03 2.698 V 0m`. Les tensions
sont arrondies à **trois décimales** sur l'écran ; brut et tension complète
restent sur USB. ATTENTE, DEFAUT ADS et PERIMEE remplacent seulement la valeur
du nœud concerné. Redémarrer le hub efface sa mémoire et impose d'attendre
les prochains paquets ; aucun pourcentage d'humidité n'est calculé.

## Compiler et vérifier

Avec les outils locaux indiqués dans le [guide radio](soil-lora.md) :

```powershell
python -m platformio run --project-dir firmware/XiaoSoilDiagnostic -e xiao_soil_diagnostic -e xiao_soil_lora_tx -e xiao_soil_lora_pmos -e xiao_soil_lora_pmos_node2 -e xiao_soil_lora_pmos_node3
python -m platformio run --project-dir firmware/PlantStationVeml7700 -e heltec_v3_text -e heltec_v3_json -e heltec_v3_soil_lora
python tools/run_soil_lora_host_tests.py
python tests/test_capture_soil_lora.py
python tests/test_xiao_battery_variant.py
```

La vérification batterie inspecte les **cinq ELF XIAO**, à compiler avant elle.
Installer uniquement la nouvelle carte et le Heltec, après identification USB,
sauvegarde des applications disponibles, antennes raccordées et batterie de
la nouvelle XIAO isolée. Aucun bootloader ni partition ne doit être remplacé.

Les captures comparent par nœud/session/compteur et exigent un minimum de paires
**pour chaque nœud suivi**. Le trafic valide des autres nœuds est conservé dans
`background_rx`. Pour installer SOIL-03, exiger `compared_nodes == [3]`, la
bannière SOIL-03 et son état AUTO. Les rejets non attribuables restent des erreurs.
Les options CLI restent inchangées ; un contrôle court d'une paire initiale
ne prouve pas trois cycles ni une période réelle complète de trente minutes.

Les tests couvrent compatibilité des anciennes trames, troisième nœud,
défauts/reprises indépendants, géométrie de l'écran, trafic de fond 1/2,
identifiants inconnus et contrôle GPIO/temps des trois profils P-MOS.
