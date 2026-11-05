# Augmentation de puissance, 26 octobre 2026

L'utilisateur demande si la puissance d'emission peut etre augmentee,
apres un diagnostic de reception fragile a environ 7 m derriere un mur.
Consigne choisie : 8 dBm, environ 6,3 mW nominaux conduits, contre
2 dBm / 1,6 mW auparavant. Ecart programme : +6 dB, soit environ x4 en
puissance ; cela ne promet pas une portee multipliee par quatre.

## Changement et validation

- TX_POWER_DBM et TX_PA_CONFIG sont centralises dans WeatherRadioConfig.h.
- Le firmware emetteur configure cette puissance et relit RegPaConfig
  (0x09) et RegPaDac (0x4D), au demarrage et pendant le controle de sante
  au repos. Une perte de ces registres declenche la reprise radio existante.
- Frequence 433920000 Hz, BW31250, SF7, CR4/5, sync12, preambule8, CRC,
  IQ normal et intervalle120s conserves. Wi-Fi/BLE conserves.
- Dix-neuf assertions de validation passent avec -Wall -Wextra -Werror,
  dont PA_BOOST8dBm accepte, ancien2dBm rejete et PA_DAC haute puissance
  20dBm rejete. Les deux profils Wi-Fi+LoRa TX et RX compilent.
- Sonde COM3 identifiee par MAC e8:3d:c1:81:ca:70 avant televersement.
  Installation reussie, ecritures verifiees : 1566826 octets flash,
  63220 octets RAM. Le recepteur n'est pas reprogramme.
- Capture du demarrage a 10:38:16 UTC : version0x12/0x12,
  puissance demandee8dBm, PA=0x86, DAC=0x84 relus. TxDone et reception
  du meme JSON seq0, uptime545ms,23,2 degres C /1008,45hPa,92octets.
  RSSI=-99dBm, SNR=+9,5dB ; mesure valide publiee sur le tableau de bord.
  Le paquet seq0 precedent (-100dBm,+9,25dB) provient du demarrage apres
  televersement, avant notre reset manuel destine a observer les registres.

Les receptions USB ne suffisent pas a mesurer le gain a l'emplacement
eloigne. L'utilisateur est invite a remettre la sonde sur la meme batterie,
au meme endroit a environ7m derriere le mur et avec la meme orientation
d'antenne ; cette confirmation et les nouveaux paquets sont necessaires
pour attribuer une comparaison a ce placement.

L'envoi periodique seq=1, uptime_ms=120594, est ensuite capture et valide
sur le recepteur : 23,0 degres C / 1008,57 hPa, 95 octets,
RSSI=-100 dBm, SNR=+10 dB. Les erreurs CRC restent a quatre, comme avant
le changement ; aucune nouvelle erreur CRC pendant ces receptions.
La capture du recepteur de 240 s se termine sans erreur SPI observee.
Etat conserve dans lora-power-8dbm-latest.json.

Au dernier inventaire de 10:42:01 UTC, COM3 est toujours present et
l'utilisateur n'a pas encore confirme le retour sur batterie au placement
a 7 m. L'installation et les emissions a 8 dBm sont validees ; le gain
de reception au meme emplacement eloigne reste non mesure.

## Confirmation de placement et comparaison a 8 dBm

L'utilisateur confirme ensuite la sonde sur batterie au meme emplacement
a environ 7 m derriere le mur, avec la meme orientation d'antenne.
Au releve de 11:24:43 UTC, seul le recepteur est en USB (COM4) :
88 mesures meteo valides, derniere mesure agee de 76 s,
22,3 degres C / 1008,20 hPa, RSSI=-121 dBm, SNR=+5,75 dB,
decalage estime=-1990 Hz. Les erreurs CRC restent a quatre.
La configuration RX est toujours correcte a 433,92 MHz.
Preuve : lora-power-8dbm-far-initial.json.

Reference precedente sur batterie, emplacement eloigne declare identique,
a 2 dBm : RSSI=-127 dBm, SNR=-4,25 dB (JSON seq2 et releve conserve
dans lora-loss-20261026-battery-far-final.json). Le gain de niveau
observe sur ces paquets est d'environ 6 dB, coherent avec la difference
de consigne ; l'environnement n'est pas une chambre de mesure RF et
le SNR peut aussi varier avec les interferences.
Une capture de 210 s est lancee pour observer deux receptions successives
dans la situation maintenant confirmee, sans nouveau changement de firmware.

Deux paquets consecutifs sont ensuite captures et publies, a environ 120 s
d'intervalle, avec la sonde sur batterie dans ce placement :

- seq=4, uptime_ms=483845, 22,2 degres C / 1008,05 hPa,
  RSSI=-119 dBm, SNR=+6,50 dB, compteur brut=93.
- seq=5, uptime_ms=603917, 22,3 degres C / 1008,02 hPa,
  RSSI=-119 dBm, SNR=+6,75 dB, compteur brut=94.

Les erreurs CRC restent a quatre pendant ces deux receptions ; aucune
nouvelle erreur CRC observee. Compteur meteo final=90. La frequence et
les registres RX restent corrects. Preuves : lora-power-8dbm-far-mid.json,
lora-power-8dbm-far-final.json et section du 26 octobre vers 11:25 UTC
dans lora-link-com4.log. Aucun reset ni changement logiciel pendant
cette comparaison sur batterie.

Conclusion de ce test : receptions ameliorees au placement declare
identique. Les niveaux observes a 8 dBm vont de -121 a -119 dBm, contre
-127 dBm sur le paquet de reference a 2 dBm. Cela ne mesure pas la
puissance RF reelle, la portee maximale ni une fiabilite de longue duree.

## Limites de la consigne

La puissance programmee n'est pas une mesure de puissance rayonnee :
le gain d'antenne et les pertes comptent egalement. La decision Arcep
2019-0300, annexe page18/42, indique10mW PAR et10% d'occupation pour
433,05..434,04MHz. La cadence de120s et les paquets bornes conservent
une occupation inferieure a1% pour les parametres inchanges.
Aucune mesure de conformite RF ou de puissance conduite n'est effectuee.
Source : https://www.arcep.fr/uploads/tx_gsavis/19-0300.pdf

Preuves : lora-power-8dbm-before.json, lora-power-8dbm-usb.json,
transmitter-startup-recovery.log (section10:38:16UTC) et
lora-link-com4.log (capture commencee vers10:37UTC).
