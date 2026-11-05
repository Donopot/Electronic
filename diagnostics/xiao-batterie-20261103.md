# SOIL-01 : alimentation batterie et premier essai

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

## Premier contrôle après soudure directe

L'utilisateur ne dispose pas de connecteur et a soudé directement les fils
sur BAT−/BAT+, après les consignes de travail USB débranché, un fil à la
fois, autre extrémité isolée. Il confirme « soudé ».

- XIAO absent de l'inventaire USB avant et après l'écoute ; Heltec sur COM5.
- Première ouverture avec DTR actif : appui PRG/BOOT puis mode download du
  Heltec. Cette capture a été interrompue et **n'est pas une preuve radio**.
  Journal `xiao-batterie-radio-20261103-165202Z.log`, bilan d'abandon associé.
- Heltec remis en démarrage normal avec DTR et RTS inactifs, sans flash.
  Réception 868,1 MHz confirmée par `state=listening`, mesures VEML7700 actives.
- Seconde écoute **110,047 s : aucune trame SOIL reçue**, aucune erreur de
  capture. Preuves : `xiao-batterie-radio-20261103-165404Z.log` et `.json`.
- **Le fonctionnement du XIAO sur batterie n'est donc pas encore validé.**
  Cette absence ne permet pas d'attribuer la panne à la batterie seule.
- Contrôles physiques demandés : XIAO bien réemboîtée sur Wio avec antenne ;
  tension entre 3V3 et GND, batterie raccordée et USB du XIAO débranché.
  L'utilisateur confirme les cartes assemblées et l'antenne raccordée, puis
  précise **0,8 V entre 3V3 et GND**. La sortie d'alimentation est anormalement
  basse ; le diagnostic reste matériel, sans modification du firmware.
- Contrôle d'échauffement demandé, réponse encore inconnue. Une mauvaise
  soudure ou un pont d'étain sont des hypothèses, pas des défauts démontrés.
  Photo des deux soudures et contrôle BAT+/BAT− nécessaires pour distinguer
  perte de contact, entrée batterie qui s'effondre et problème en aval.

## Contrôle réussi après reprise de soudure

L'utilisateur indique avoir corrigé la soudure et retrouvé « la bonne tension ».
La valeur exacte de cette nouvelle mesure au multimètre n'est pas précisée.
Une nouvelle écoute a été lancée, sans modifier ni reflasher les firmwares.

- XIAO absent de l'inventaire USB avant et après ; Heltec sur COM5, lignes
  DTR/RTS inactives, sans réinitialisation pendant la capture.
- Capture du 3 novembre 2026 à 18:08:20, heure de Paris, durée **82,406 s**.
- **Trois mesures valides consécutives**, même session **4236450037**, adresse
  **0x48**, sans erreur de capture ni défaut ADC signalé.

| Séquence | Temps depuis démarrage XIAO | Brut reçu | Tension de sonde reçue |
| --- | --- | --- | --- |
| 5 | 133,392 s | 21189 | 2,648625 V |
| 6 | 164,031 s | 21174 | 2,646750 V |
| 7 | 194,669 s | 21162 | 2,645250 V |

Intervalles de réception 30,641 puis 30,640 s. Le Heltec a aussi produit
81 mesures lux valides pendant cette capture. Le fonctionnement de la
mesure et de la transmission **sur batterie seule est confirmé à court terme**.
L'écoute RX seule ne mesure pas un taux de perte global ; elle ne valide ni
la recharge, ni l'autonomie, ni la température ou la tenue des soudures.

Preuves : `xiao-batterie-apres-soudure-20261103-170819Z.log` et `.json`
(`accepted: true`). Le P-MOS est toujours absent : il n'est pas nécessaire
pour ce fonctionnement, mais reste prévu au plan pour couper la sonde entre
les relevés. L'économie d'énergie complète et les mesures de consommation
restent à réaliser.
