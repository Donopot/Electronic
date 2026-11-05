# Premier diagnostic Heltec V3 (Plant Station)

Projet autonome pour Heltec WiFi LoRa 32 V3 EU868, ESP32-S3 / SX1262.
Il ne modifie pas les projets ESP32-C3 existants.

**Installe sur COM5 le 26 octobre 2026.** Initialisation SX1262 et reception
continue confirmees, OLED acquitte a 0x3C, quatre etats successifs sans erreur
pendant une capture de 25 secondes. Aucun paquet recu : le premier emetteur
XIAO n'est pas encore programme. Voir
[le rapport](../../diagnostics/heltec-v3-rx-20261026.md).

Ce firmware teste l'initialisation du SX1262 avec RadioLib, active son TCXO
1,8 V et le commutateur RF DIO2, puis reste en reception. Il n'emet aucun
paquet. L'OLED est utilise temporairement pour le diagnostic. Aucun Wi-Fi
ni capteur n'est configure a ce stade.

- Frequence : 868,1 MHz ; BW125 kHz ; SF7 ; CR4/5 ; sync privee 0x12.
- CRC active, en-tete explicite, preambule de 8 symboles.
- SPI : SCK=9, MISO=11, MOSI=10, NSS=8, RESET=12, BUSY=13, DIO1=14.
- OLED : SDA=17, SCL=18, RESET=21 ; Vext=36 actif bas.
- Journal : UART0 via CP2102, 115200 bauds, USB CDC natif desactive.

`RADIO OK / ECOUTE` atteste l'initialisation et l'entree en reception,
pas la portee ni une liaison complete. Sans emetteur compatible, zero
paquet est attendu. Les trames radio eventuelles sont brutes : elles ne
constituent pas encore des mesures authentifiees du projet.

## Boutons et economie de batterie (v2)

Version v2 installee et testee sur COM5 le 26 octobre 2026 : appui court,
deux cycles de veille et reprise de l'ecoute apres RST verifies par le
circuit DTR/RTS du CP2102. Carte laissee en veille apres l'essai. Voir
[le rapport de veille](../../diagnostics/heltec-v3-sleep-20261026.md).

- Maintenir **PRG pendant 3 secondes**. L'ecran demande de relacher PRG.
- Relacher PRG : SX1262 en sommeil, OLED coupe via Vext, LED eteinte,
  puis ESP32-S3 en veille profonde sans reveil automatique.
- Un appui court sur PRG ne change rien.
- Appuyer sur **RST**, PRG relache, pour redemarrer et reprendre l'ecoute.
- PRG seul ne reveille pas la carte. Ce mode garde une consommation
  residuelle ; ce n'est pas une coupure physique de la batterie et son
  courant n'a pas encore ete mesure.

Les niveaux de commande de Vext et des sorties radio sont maintenus
pendant la veille. Si la commande de sommeil du SX1262 echoue, son RESET
est maintenu bas. Les maintiens GPIO sont liberes au demarrage suivant.

Test materiel reproductible via le circuit de programmation USB (il
actionne GPIO0/PRG et EN/RST, et redemarre donc la carte) :

```powershell
python tools/check_heltec_sleep.py --port COM5 --output diagnostics/heltec-sleep-retest.log --leave-asleep
```

Executer depuis Electronic avec `PYTHONPATH` configure comme ci-dessous.
Sans `--leave-asleep`, le test termine en reception.

## Compilation et installation

Depuis la racine Electronic, avec les outils locaux deja installes :

```powershell
$env:PYTHONPATH = Join-Path $PWD '.tools\python'
$env:PLATFORMIO_CORE_DIR = Join-Path $PWD '.tools\platformio'
python -m platformio run --project-dir firmware/PlantHubDiagnostic
python -m platformio run --project-dir firmware/PlantHubDiagnostic --target upload --upload-port COM5
```

Verifier le port reel avant installation. Sauvegarder la flash d'origine
avant le premier televersement ; les sauvegardes restent dans `.tools/private/`
(ignore par Git). La validation sur carte est consignée dans `diagnostics/`.

## References

- [Carte et ressources constructeur](https://heltec.org/project/wifi-lora-32-v3/)
- Brochage : variante locale Arduino `heltec_wifi_lora_32_V3/pins_arduino.h`.
- [RadioLib 7.8.1](https://github.com/jgromes/RadioLib/releases/tag/7.8.1)
