# SOIL-01 : préparation du premier essai batterie

## Matériel et limites

- Batterie photographiée : Li-ion 3,7 V, 1000 mAh. L'utilisateur confirme un
  modèle protégé et la disponibilité d'un multimètre. Avec les pointes
  rouge/noire placées sur les fils correspondants, il relève **3,29 V**,
  sans signe négatif signalé. La batterie est encore séparée de la carte.
- Photo du dos XIAO `IMG_20261103_172711.jpg` : USB en haut, deux rectangles
  à droite près de BAT ; BAT− supérieur et BAT+ inférieur, conformes au
  [brochage Seeed](https://files.seeedstudio.com/wiki/XIAO-BLE/XIAO_nRF52840_back_pinout.png).
- Pendant la préparation et le contrôle radio : batterie débranchée, USB seul,
  XIAO sur Wio avec antenne. P-MOS toujours débranché.

## Correction installée

Le cœur Seeed 1.1.1 mettait P0.14 / READ_BAT_ENABLE à HIGH. Une variante locale
sélectionnée pour les deux profils précharge LOW avant `pinMode(OUTPUT)`.
La table des broches et les autres initialisations restent identiques ;
P0.13 / commande de courant de charge reste en entrée sans rappel, réglage
faible courant annoncé à environ 50 mA. Le paquet partagé n'est pas modifié.
Voir la [FAQ constructeur](https://wiki.seeedstudio.com/XIAO_BLE/#q3-what-are-the-considerations-when-using-xiao-nrf52840-sense-for-battery-charging).

Le bootloader et la période avant l'application ne sont ni modifiés ni
audités. Cette correction et la capture USB ne valident pas la recharge,
le courant réel, la température, la consommation ou l'autonomie. Premier
essai batterie prévu USB débranché ; aucune recharge/DFU batterie connectée
n'est validée par ce rapport.

## Vérification logicielle

- Deux profils compilés : `xiao_soil_diagnostic` et `xiao_soil_lora_tx`, succès.
  Profil radio : application 89 744 octets, RAM 8 448 octets.
- `python -m unittest discover -s tests -p test_xiao_battery_variant.py -v` :
  trois tests passent. Le vrai `initVariant()` est exécuté avec une trace GPIO ;
  les deux ELF sont inspectés avec `nm` et `objdump`. Un unique symbole fort
  et la séquence LOW/OUTPUT/courant faible sont confirmés dans les deux profils.
- Revue indépendante : quatre améliorations des tests et de leurs prérequis
  traitées puis revérifiées ; aucun constat restant.
- Sauvegarde de l'application précédente dans
  `.tools/private/before-xiao-battery-20261103/`.
- Installation USB DFU COM7 réussie, application seulement : taille SoftDevice
  et bootloader toutes deux nulles dans le transfert. Retour sur COM8.
- SHA-256 ELF installé :
  `16de1246ebeadd9e33aa241c7712e4519b6ee16cf7adfee7390e27df7d878945`.
- SHA-256 ZIP DFU :
  `e5fb98c1eede5390351270995a97780b428111bacdc8cc4640f13d64e031ba4d`.

## Contrôle radio après installation

Capture de 72,078 s, XIAO COM8 et Heltec COM5, session 4279149232 :

| Séquence | Adresse | Brut TX = RX | Tension TX = RX |
| --- | --- | --- | --- |
| 2 | 0x48 | 21487 | 2,685875 V |
| 3 | 0x48 | 21489 | 2,686125 V |

Deux correspondances valides, sans perte, doublon, divergence ni erreur de
capture. Le critère annoncé pour cette correction (deux émissions) est atteint.
Le champ `accepted` de l'utilitaire général reste faux : son seuil par défaut
exige trois correspondances. Ce seuil n'a pas été changé ; il ne s'agit pas
d'une erreur de mesure ou de radio.

Preuves locales : `xiao-batterie-builds-20261103.log`,
`xiao-batterie-init-binaire-20261103.log`,
`xiao-batterie-installation-20261103.log` et
`xiao-batterie-usb-controle-20261103{.json,-tx.log,-rx.log}`.

## Suite matérielle

Polarité contrôlée par l'utilisateur avec une lecture de 3,29 V. Garder
l'USB débranché pour raccorder les pastilles via un connecteur détachable
et des fils isolés. Souder le câble côté carte batterie déconnectée, puis
raccorder celle-ci après contrôle des soudures.
Le fonctionnement et la réception sur batterie restent à vérifier après
ce raccordement. Aucune humidité relative calibrée n'est fournie.
