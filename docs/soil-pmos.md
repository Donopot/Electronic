# SOIL-01 : test de la commande AO3401

## Montage utilisé

AO3401 SOT-23 vu du dessus, marquage visible et patte seule en haut :
3 = Drain, 1 = Grille en bas à gauche, 2 = Source en bas à droite.
Le [brochage constructeur](https://www.aosmd.com/sites/default/files/res/datasheets/AO3401.pdf)
doit être comparé au composant avant soudure.

| Liaison | Connexion |
| --- | --- |
| AO3401 2 / S | XIAO 3V3 |
| AO3401 3 / D | Sonde VCC, ancien fil direct 3V3 retiré |
| AO3401 1 / G | XIAO D0 via 220 Ω |
| Rappel G-S | 10 kΩ, valeur confirmée par l'utilisateur |
| ADS1115 | VDD 3V3, GND commun, ADDR GND, SDA D6, SCL D7 |
| Sonde GND / AOUT | GND commun / ADS1115 A0 |

Le bouton du Wio partage D0 : ne pas l'utiliser pendant ce contrôle. Le
logiciel n'écrit jamais HIGH sur D0 et n'active aucun rappel interne.
LOW préchargé puis OUTPUT allume ; INPUT relâche la grille et éteint par
le rappel externe. Un niveau de grille lu par logiciel ne prouve pas la
coupure physique : mesurer VCC/GND de la sonde.

## Profil de test

`firmware/XiaoSoilDiagnostic/platformio.ini` ajoute `xiao_soil_lora_pmos`.
Les profils USB seul et radio sans P-MOS restent disponibles. La variante
batterie corrigée est commune aux cinq profils.

Au démarrage, D0 est relâchée et le premier cycle attend 30 s. En mode AUTO,
le programme alimente la sonde, attend 1 s, prend cinq conversions et leur
médiane, puis relâche D0 avant de journaliser et d'émettre. La prochaine
acquisition attend **30 minutes** après la fin du traitement précédent. Une acquisition
échouée coupe aussi la sonde et transmet un défaut, sans reprendre une ancienne
mesure. Un appui bouton ou une grille maintenue basse peut empêcher la coupure
physique malgré l'état logiciel ; le contrôle au multimètre reste requis.

Ce profil valide la commutation, **pas la veille longue ni une autonomie**.
Le Heltec conserve la mesure comme courante pendant **90 minutes** sans
nouveau paquet accepté, puis affiche « PERIMEE ».

SOIL-02 utilise le même montage avec le profil `xiao_soil_lora_pmos_node2`,
premier cycle après 35 s puis 30 min. SOIL-03 utilise `xiao_soil_lora_pmos_node3`, premier cycle à 40 s. Voir [les trois sondes](soil-three-nodes.md).

## Commandes USB

Port XIAO à 115200 bauds, lignes terminées par CR ou LF :

- `OFF` : grille relâchée ; aucune acquisition ni émission jusqu'à une autre
  commande. Le Heltec peut donc signaler des données anciennes après 90 min.
- `ON` : maintien allumé pour le contrôle au multimètre, limité à 120 s puis
  OFF. Débrancher physiquement l'USB coupe aussi ce mode ; fermer seulement le
  terminal, avec l'USB toujours configuré, laisse le temps de mesurer. Ce mode
  sert au contrôle de VCC : il n'effectue ni acquisition ni émission radio.
  L'échéance est vérifiée à chaque tour, avec une pause de 10 ms entre tours.
- `AUTO` : coupe d'abord, puis reprend le cycle, avec première mesure immédiate
  et respect du garde radio minimal de 30 min. Des commandes AUTO répétées
  peuvent relancer une acquisition manuelle, sans accélérer les émissions.

Le traitement des commandes lit au plus 64 octets par tour. Une ligne trop
longue ou contenant un octet autre qu'une lettre majuscule est abandonnée
entièrement (hors délimiteurs CR/LF). Les lignes USB restent bornées à 4 ms,
même si le terminal ne lit plus. Une commande reçue pendant une acquisition
ou initialisation radio sera traitée au retour de ce traitement ; la sonde
est déjà coupée avant l'initialisation radio.

## Contrôle matériel

Garder la batterie déconnectée pendant le flash et le contrôle USB initial.
Vérifier successivement OFF puis ON entre **VCC et GND de la sonde**. OFF doit
être proche de 0 V et ON proche de la sortie 3V3 mesurée. Une tension notable
en OFF impose de rechercher pont, inversion S/D, bypass ou réalimentation.
Après validation, envoyer AUTO et comparer au moins trois paquets USB TX et
Heltec RX. La mesure brute ne représente toujours pas un pourcentage d'eau.

## Vérification logicielle

Depuis la racine, avec Python, uv, Zig 0.13.0 et paquets PlatformIO locaux :

```powershell
python tools/run_soil_lora_host_tests.py
$env:PYTHONPATH = Join-Path $PWD '.tools/xiao-diagnostic-python'
$env:PLATFORMIO_CORE_DIR = Join-Path $PWD '.tools/platformio'
python -m platformio run --project-dir firmware/XiaoSoilDiagnostic -e xiao_soil_diagnostic -e xiao_soil_lora_tx -e xiao_soil_lora_pmos -e xiao_soil_lora_pmos_node2 -e xiao_soil_lora_pmos_node3
python tests/test_xiao_battery_variant.py
```

Le lanceur hôte vérifie le premier cycle après 30 s, l'absence d'acquisition
intermédiaire, la frontière de 30 min, les retards sans rattrapage et le
débordement de l'horloge par simulation du temps. Il vérifie aussi le maintien
de la mesure sur le Heltec à 30 min et sa péremption à 90 min.
Il couvre le contrôleur réel (médiane et chaque erreur de lecture),
le GPIO partagé, le parser, les commandes manuelles, USB absent/bloqué, les
timeouts et le débordement de millis, plus les scénarios historiques TX/RX.
