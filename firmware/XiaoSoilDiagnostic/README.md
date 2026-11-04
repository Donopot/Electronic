# Diagnostic USB XIAO et ADS1115

Le profil optionnel `xiao_soil_lora_tx` ajoute maintenant le transport SOIL-01
vers le Heltec, sans changer les lignes `[MESURE]` du diagnostic ci-dessous.
Voir [profils, protocole, branchements et capture LoRa](../../docs/soil-lora.md).
Le profil par defaut `xiao_soil_diagnostic` conserve le fonctionnement USB seul.

Le profil `xiao_soil_lora_pmos` ajoute la commande AO3401 sur D0, l'acquisition
commutée et les contrôles USB OFF/ON/AUTO. Voir le [test P-MOS](../../docs/soil-pmos.md)
pour le câblage avec rappel 10 kΩ, les limites et la validation au multimètre.
Le premier relevé automatique arrive après 30 s ; les suivants sont espacés
de 30 minutes après la fin du cycle précédent. Le Heltec associé utilise une
péremption de 90 minutes. Cette cadence n'ajoute pas de nouvelle veille matérielle.
Les paragraphes ci-dessous décrivent les profils historiques sans commutation.

Les trois profils utilisent une [variante locale](variants/README.md) pour
maintenir P0.14 / READ_BAT_ENABLE à LOW dès `initVariant()`, tout en conservant
le réglage de recharge faible courant annoncé à environ 50 mA. Le bootloader
et la recharge matérielle ne sont pas validés par cette correction. Pour le
premier essai batterie, débrancher l'USB avant de raccorder la cellule protégée
aux pastilles BAT+/BAT−, après contrôle de sa polarité. Le P-MOS reste débranché.

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

Apres l'echange sur les masses communes, le nouveau test du meme jour trouve
0x48 et mesure une mediane de 2,695625 V dans l'air contre 1,060375 V dans
la terre humide : 59 conversions sans erreur. La reponse physique est observee ;
l'etalonnage du pot reste a faire. Voir
[le rapport](../../diagnostics/humidite-controle-gnd-20261103-160424.md).

## References

- [Brochage Seeed XIAO](https://wiki.seeedstudio.com/XIAO_BLE/).
- [Coeur Arduino Seeed](https://github.com/Seeed-Studio/Adafruit_nRF52_Arduino).
- [Fiche technique TI ADS1115](https://www.ti.com/lit/ds/symlink/ads1115.pdf).
- [Chargeur UF2 nRF52 Meshtastic](https://meshtastic.org/docs/getting-started/flashing-firmware/nrf52/drag-n-drop/).
