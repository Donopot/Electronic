# Diagnostic USB XIAO et ADS1115

Projet autonome pour XIAO nRF52840 et sonde capacitive analogique. Le kit
Wio-SX1262 peut rester assemble, mais ce diagnostic n'initialise pas la radio.
Il remplace l'application installee sur le XIAO pendant le test.

| Depart | Arrivee |
| --- | --- |
| XIAO 3V3 | ADS1115 VDD et sonde VCC compatible 3,3 V |
| XIAO GND | ADS1115 GND, ADDR et sonde GND |
| XIAO D6, P1.11 | ADS1115 SDA |
| XIAO D7, P1.12 | ADS1115 SCL |
| Sonde AOUT | ADS1115 A0 |

Premier essai sur USB, batterie et P-MOS debranches comme convenu. D0 reste
une entree sans tirage. Les resistances de rappel I2C doivent aller vers 3,3 V.
Le bus logiciel fonctionne lentement et limite chaque attente SCL a 2 ms ;
il ne force jamais un niveau haut. Le logiciel controle le mapping physique
de D6/D7 fourni par le coeur Seeed avant de toucher ces lignes.

Le scan est repete tous les dix essais. Un unique candidat entre 0x48 et 0x4B
est configure pour lire A0 ; une adresse differente de 0x48 est signalee.
Si plusieurs candidats repondent, aucune configuration n'est envoyee. Lecture
en single-shot a 128 SPS, plage +/-4,096 V, pas de 125 microvolts. La plage
du convertisseur n'autorise pas une tension superieure a VDD sur son entree.
Les mesures hors de la plage nominale 0-3,3 V sont signalees, sans mesurer
VDD lui-meme. Jusqu'a huit codes negatifs pres de zero sont distingues comme
un offset possible, pas comme une surtension prouvee. Aucun pourcentage n'est calcule.
Un ACK ou un registre compatible ne constitue pas une identification certaine
de la puce : les ADS1015 et ADS1115 n'ont pas de registre d'identification.

## Compilation et capture

Depuis la racine du projet, avec les outils Python locaux :

```powershell
$env:PYTHONPATH = Join-Path $PWD '.tools\xiao-diagnostic-python'
$env:PLATFORMIO_CORE_DIR = Join-Path $PWD '.tools\platformio'
python -m platformio run --project-dir firmware/XiaoSoilDiagnostic
python tools/capture_xiao_soil.py --port COM6 --seconds 25 --output diagnostics/xiao-ads1115-live.log
```

Verifier le port reel apres installation ; il peut changer. Le chargeur UF2
est accessible par double appui RESET. Avant remplacement, garder une copie
de CURRENT.UF2 et INFO_UF2.TXT, ainsi qu'un export de configuration Meshtastic
dans `.tools/private/`. La sauvegarde UF2 du chargeur n'inclut pas necessairement
tous les parametres. Ne pas mettre a jour le chargeur pour ce test.

Une tension A0 stable valide la lecture du convertisseur, pas le fonctionnement
physique de la sonde : comparer ensuite les valeurs dans l'air et dans un
substrat humide, sans mouiller l'electronique situee au-dessus de la sonde.

Au premier scan materiel du 3 novembre 2026, l'unique reponse est 0x49.
Ce n'est pas l'adresse attendue avec ADDR a GND ; controler ce raccordement
hors tension. Il n'est pas necessaire de deplacer les fils pour lire 0x49.

## References

- [Brochage Seeed XIAO](https://wiki.seeedstudio.com/XIAO_BLE/).
- [Coeur Arduino Seeed](https://github.com/Seeed-Studio/Adafruit_nRF52_Arduino).
- [Fiche technique TI ADS1115](https://www.ti.com/lit/ds/symlink/ads1115.pdf).
- [Chargeur UF2 nRF52 Meshtastic](https://meshtastic.org/docs/getting-started/flashing-firmware/nrf52/drag-n-drop/).
