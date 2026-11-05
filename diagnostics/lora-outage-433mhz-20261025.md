# Nouvelle perte de mesures a 433,92 MHz

L'utilisateur signale l'arret des mesures apres un echange precedemment
valide a 433,92 MHz. Il confirme les deux cartes toujours dans la meme piece.

## Etat initial, 21:03..21:05 UTC

- Deux identites USB presentes : sonde e8:3d:c1:81:ca:70 sur COM3,
  recepteur e8:3d:c1:78:b4:b4 sur COM4.
- Recepteur HTTP 192.168.1.197 joignable, Wi-Fi connecte, ancien compteur4.
  Derniere mesure agee de 647603ms au premier releve, donc valeurs
  masquees apres le seuil normal de 180000ms.
- Registres relus : version0x12, mode0x8E, modem42/74, sync12,
  FRF0x6C7AE1 (consigne433920000Hz), IQ27/1D. Configuration attendue valide.
- Dernier paquet ancien : RSSI=-127dBm, SNR=-2,75dB. Le RSSI du canal
  courant est environ -125..-127dBm, a distinguer du signal d'un paquet.
- Sonde inaccessible a son ancienne adresse Wi-Fi 192.168.1.65, mais
  les mesures BMP180 continuent toutes les deux secondes sur COM3.
- Capture simultanee sans redemarrage a partir de 21:03:46 UTC : seq1,
  uptime120728ms, 22,4 degres C / 1008,17hPa, suivie de TxDone.
  Aucun nouveau paquet au recepteur ; compteurs materiels inchanges.
  TxDone confirme le modem local, pas la puissance RF a la sortie d'antenne.

## Redemarrage cible du recepteur, firmware conserve

Apres la premiere capture de 150s, le recepteur seul est redemarre par
RTS/DTR. Deux emissions de la sonde sont ensuite confirmees (seq2,
uptime240775ms, puis seq3, uptime360822ms). Le recepteur retrouve le Wi-Fi
et la configuration attendue ; il detecte deux en-tetes valides mais
aucun paquet complet valide. Aucun retablissement par ce redemarrage.
Aucune erreur SPI ni coupure USB relevee pendant les captures.

## Instrumentation des erreurs de reception

Le pilote local expose desormais le snapshot IRQ de parsePacket avant
acquittement ; aucun nouvel acquittement ni commande de reception n'est
ajoute. Le recepteur compte RxDone, erreurs CRC, timeouts et conserve les
metadonnees du dernier RxDone (RSSI/SNR/estimation du decalage de frequence).
Les timeouts sont normaux pendant l'attente entre emissions.

Firmware recepteur compile et installe sur COM4, ecritures verifiees :
1553322 octets flash, 62956 octets RAM. Frequence, BW31,25kHz, SF7, CR4/5,
sync, CRC, IQ, brochage et Wi-Fi inchanges. Le profil emetteur Wi-Fi+LoRa
compile egalement avec la bibliotheque modifiee (1566672 flash,63220 RAM),
mais n'est pas reteleverse sur la sonde pendant ce diagnostic.

Preuves : lora-outage-433mhz-initial.json,
lora-outage-433mhz-after-tx.json, lora-outage-433mhz-after-rx-reset.json,
et sections du 25 octobre a partir de 21:03:46 UTC dans lora-link-com3.log
et lora-link-com4.log. Aucun reglage RF change pendant ces essais.

## Resultat avec instrumentation

Les emissions seq4 (uptime480869ms, 23,2 degres C / 1008,11hPa) et seq5
(uptime600916ms, 23,2 / 1008,13) se terminent sur COM3, sans erreur SPI.
Apres l'installation des compteurs, aucun RxDone, aucune erreur CRC et
aucun en-tete valide ne sont observes au recepteur sur ces emissions.
Les timeouts RX continuent normalement. L'estimation de decalage de
frequence reste donc indisponible (null), faute de RxDone ; aucune mesure
du decalage de frequence reel n'est obtenue par ce test.

Le niveau du canal est echantillonne via HTTP toutes les 200ms pendant
une emission : 598 releves valides, zero erreur HTTP, -132..-124dBm,
sans hausse nette observable liee
a l'emission. Cette observation ne mesure ni la puissance RF de la sonde,
ni la sensibilite reelle du recepteur, ni la frequence physique du quartz.
Preuves : lora-radio-channel-433mhz.jsonl et lora-outage-433mhz-final.json.

Conclusion : absence de mesure expliquee par la perte de paquets sur la
liaison radio, avant le tableau de bord. Le capteur fonctionne, TxDone est
confirme et les registres du recepteur sont coherents. Le redemarrage RX
seul ne retablit pas la liaison. Ces preuves ne permettent pas de designer
un module defectueux ni d'attribuer la cause au Wi-Fi ou a la batterie.
Le signal des dernieres receptions reussies etait deja tres faible.
Controles physiques utiles : position/connexion des antennes 433MHz,
soudures RF et masses, tensions 3V3 directement sur chacun des modules
pendant l'emission/reception. Un essai a environ un metre, antennes
verticales et degagees, a ete demande a l'utilisateur ; resultat non confirme.

## Nouvel essai a un metre, capture du 25 octobre a 22:14:06 UTC

L'utilisateur confirme avoir place les cartes a environ un metre, antennes
verticales et degagees, avec USB et antennes raccordes. Les identites COM3
et COM4 sont verifiees a nouveau ; les deux cartes sont presentes.

Le nouvel etat initial du recepteur conserve une configuration correcte
a 433,92 MHz. Il a compte entre-temps un paquet brut, mais aucune mesure
meteo valide. Deux RxDone et une erreur CRC sont historiques : le dernier
evenement date deja de 2828590ms avant ce releve (-128dBm, SNR-10,5dB,
decalage estime -1927Hz). Ces valeurs ne mesurent pas le nouvel essai.
Etat initial conserve dans lora-one-metre-initial.json.

Des le debut de la capture, COM3 signale une configuration radio perdue :
version0x00, mode0x00, modem00/00, sync00, FRF000000. La reprise complete
du controleur SPI et le reset du module donnent encore version0x00/0x00.
Les tentatives automatiques toutes les dix secondes echouent ; le BMP180
continue de mesurer environ24,4 degres C et1008,8hPa. Les emissions sont
inhibees par le firmware lorsque l'identification est invalide.

Il s'agit d'un blocage SPI local actuellement observe sur la sonde ;
la distance, la frequence ou le Wi-Fi n'expliquent pas une identification
nulle. La cause exacte reste a mesurer : VCC/GND, continuite des liaisons
SPI et RESET, ou module lui-meme. MISO a deja ete repare, sans preuve
que cette liaison soit seule responsable cette fois. Aucun firmware
n'est modifie ni televerse pendant ce nouvel essai. Cela explique
l'absence d'emission pendant cette capture, sans demontrer retrospectivement
la cause des pertes precedentes ou les defauts RF eventuels restants.

Une mesure actuelle VCC/GND directement sur le module emetteur, USB
branche, est demandee a l'utilisateur. Les precedents3,28V ne remplacent
pas une mesure dans l'etat de panne actuel. Observation du canal dans
lora-one-metre-channel.jsonl ; journaux serie dans lora-link-com3.log
et lora-link-com4.log, section22:14:06UTC.

Resultat final de cet essai de150s : quatorze identifications SPI
invalides (0x00/0x00), zero emission terminee, aucune nouvelle reception
ni nouvelle erreur CRC au recepteur. Le BMP180 reste actif et les USB
ne sont pas interrompus. Le releve final est conserve dans
lora-one-metre-final.json. L'echantillonnage HTTP donne746 lectures,
zero erreur reseau et un niveau de canal -132..-124dBm ; faute d'emission
pendant cet essai, ce releve ne teste pas la qualite du trajet radio a un metre.
