# Deux sondes de terre sur le Heltec

Le profil Heltec `heltec_v3_soil_lora` accepte **SOIL-01** et **SOIL-02**.
Il conserve pour chacune son brut, sa tension, son âge, sa session et son
compteur. Les identifiants restent des étiquettes, sans authentification.

| Carte | Profil XIAO | Premier relevé | Cycles suivants |
| --- | --- | --- | --- |
| SOIL-01 | `xiao_soil_lora_pmos` | après 30 s | 30 min après la fin du cycle précédent |
| SOIL-02 | `xiao_soil_lora_pmos_node2` | après 35 s | 30 min après la fin du cycle précédent |

Le décalage initial sépare les départs si les cartes sont alimentées ensemble,
sans garantie de livraison radio. Pas d'ACK ni de répétition ajoutés. Les deux
profils gardent le [câblage AO3401 et les contrôles](soil-pmos.md). Les autres
profils XIAO restent SOIL-01 par défaut. Ne pas installer SOIL-01 sur la deuxième.

Le format sur l'air reste **PS v1, vingt octets** : l'octet 3 contient 1 ou 2.
L'ancien programme SOIL-01 reste compatible sans reflash. Les autres nœuds
restent rejetés. Défaut, doublon, nouvelle session ou péremption ne modifient
que la sonde concernée ; expiration après **90 min** sans nouveau paquet accepté.

L'OLED affiche simultanément lux, les deux identifiants NON AUTH, leur tension
et âge en minutes (`0m`, `1m`…), puis l'état radio. Selon le nœud : ATTENTE SONDE,
DEFAUT ADS ou MESURE PERIMEE remplacent la tension. Les bruts restent sur USB.
Redémarrer le Heltec efface ses valeurs en mémoire et impose d'attendre les
prochains paquets ; aucun pourcentage d'humidité n'est calculé.

## Installation et vérification

Avec les outils locaux décrits dans le [test radio](soil-lora.md) :

```powershell
python -m platformio run --project-dir firmware/XiaoSoilDiagnostic -e xiao_soil_diagnostic -e xiao_soil_lora_tx -e xiao_soil_lora_pmos -e xiao_soil_lora_pmos_node2
python -m platformio run --project-dir firmware/PlantStationVeml7700 -e heltec_v3_soil_lora
python tools/run_soil_lora_host_tests.py
python tests/test_capture_soil_lora.py
python tests/test_xiao_battery_variant.py
```

Avant installation, antenne raccordée et batterie isolée sur la nouvelle XIAO ;
vérifier son numéro USB pour ne pas remplacer la première. Sauvegarder les
applications disponibles et ne modifier que les applications, sans effacement
global, bootloader ni partitions. Conserver SOIL-01 alimentée pour son prochain cycle.

Les captures comparent par **nœud/session/compteur** : même session et même
compteur entre sondes ne s'apparient pas. Les RX des nœuds présents dans les TX
USB sont comparés strictement ; les autres RX valides figurent dans
`background_rx`, sans fausse perte, doublon ou défaut ADC du nœud suivi.
Paquets malformés, nœuds inconnus, erreurs de capture et rejets sans identité
de nœud restent des erreurs. Les doublons JSON sont des triplets
`[node, session, seq]`. Les journaux bruts sont conservés.

Le minimum CLI reste trois paires **par nœud suivi**, avec les comptes
`valid_matched_by_node`. Pour installer SOIL-02, exiger aussi
`compared_nodes == [2]` et sa bannière USB : un autre profil ne valide pas
l'identité attendue. Un échange initial unique se contrôle
séparément. Une capture courte ne prouve pas une période réelle de trente
minutes. Les tests simulent aussi compteurs identiques, défauts et expirations
indépendants, reprise, rejet SOIL-03 et sécurité GPIO des deux profils P-MOS.
