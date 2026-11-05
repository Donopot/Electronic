# Retest du 26 octobre 2026

Les cartes sont identifiees sur USB : sonde COM3 e8:3d:c1:81:ca:70,
recepteur COM4 e8:3d:c1:78:b4:b4. Aucun redemarrage ni televersement
pendant ce retest ; les reglages communs restent433920000Hz, SF7,
BW31250, CR4/5, sync12, CRC, IQ normal et intervalle120s.

## Etat avant capture

L'API du recepteur confirme 24 mesures valides, dont la derniere date
de moins d'une minute lors du premier controle :23,2 degres C,1008,69hPa,
RSSI-106dBm, SNR+9,50dB. Mode0x8E, version0x12 et frequence relue
0x6C7AE1 corrects ; zero erreur CRC dans cette session. La reception
a donc repris avant notre intervention. La cause de cette reprise et
les eventuelles interventions materielles depuis le test precedent
ne sont pas etablies. L'ancienne adresse Wi-Fi de la sonde192.168.1.65
ne repond pas, mais les deux USB sont presents.

## Observations serie et HTTP

Capture simultanee de 150 s a partir de 07:45:17 UTC : la sonde mesure
normalement. Elle emet seq24, uptime2885293ms,23,6 degres C /1008,71hPa,
puis confirme TxDone. Aucun nouveau paquet ni en-tete n'est observe au
recepteur pour cet envoi ; son age de mesure depasse temporairement180s.
Aucune erreur SPI n'est relevee dans cette capture.

Une seconde capture commence a07:48:08UTC. Entre les deux captures,
une nouvelle mesure a ete recue et validee : compteur meteo24->25,
paquets bruts25->26, temperature23,8 degres C, pression1008,73hPa,
RSSI-106dBm, SNR+9,75dB, decalage estime par le modem-1822Hz.
L'API indique une reception vers07:48:00UTC. Le JSON serie complet de
ce paquet n'a pas ete capture pendant l'intervalle entre les captures ;
la reception et sa publication sont confirmees par l'increment des
compteurs et les nouvelles mesures HTTP, pas par une comparaison exacte
des deux journaux pour ce paquet. Les erreurs CRC restent a zero.

Le releve final a 07:50:26 UTC confirme encore une nouvelle reception
vers 07:50:00 UTC : 26 mesures meteo valides, 27 paquets bruts,
23,9 degres C et 1008,80 hPa, RSSI=-106 dBm, SNR=+9,75 dB,
zero erreur CRC. Les deux nouvelles mesures validees depuis le releve
initial sont donc espacees de deux minutes. Les deux captures serie,
de 150 s puis 80 s, se terminent sans erreur SPI ni interruption USB.
Les JSON exacts de ces deux receptions ne sont pas dans les captures
serie ; les nouvelles mesures et compteurs sont confirmes par l'API.

Conclusion : la communication radio a repris, mais un envoi capture
a ete manque. La liaison reste intermittente ; la perte SPI0x00 du test
precedent n'est pas observee pendant les nouvelles captures. Ce retest
ne prouve pas une resolution durable ni une cause materielle precise.

Preuves : lora-retest-20261026-initial.json,
lora-retest-20261026-after-reception.json, lora-retest-20261026-final.json,
et sections du26octobre07:45:17/07:48:08UTC de lora-link-com3.log
et lora-link-com4.log.
