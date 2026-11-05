# Heltec V3 : veille par PRG, 26 octobre 2026

## Fonction installee

Diagnostic RX v2 installe sur COM5. Maintenir PRG pendant 3 secondes,
puis le relacher, met le SX1262 en sommeil et coupe l'OLED via Vext,
eteint la LED et place l'ESP32-S3 en veille profonde. Aucun reveil
automatique n'est configure. RST, PRG relache, redemarre la carte et
retablit l'ecoute LoRa a 868,1 MHz.

Un appui court ne change pas l'ecoute. La detection est filtree sur 40 ms.
Les broches NSS, RESET radio, RESET OLED, LED et Vext sont maintenues
pendant la veille, puis liberees au demarrage. Si la commande sommeil
radio echoue, RESET radio est maintenu bas pour eviter une ecoute active.

## Validation sur carte

Compilation reussie : RAM 21068 octets, programme 350829 octets.
Televersement reussi avec verification des hachages esptool.

Test commence a 15:24:00 UTC, soit 16:24:00 a Paris, sur COM5 / 115200 bauds.
Le script `tools/check_heltec_sleep.py` actionne les lignes DTR/RTS du
CP2102 pour commander les memes signaux GPIO0/PRG et EN/RST que les boutons.
Il ne s'agit pas d'un appui manuel sur leurs contacts.

Resultats du [journal complet](heltec-v3-sleep-20261026.log) :

1. Demarrage v2, OLED detecte et SX1262 en reception avec code 0.
2. Appui PRG court de 0,3 seconde detecte : pas de veille et ecoute maintenue.
3. Appui PRG de 3,4 secondes : seuil long detecte, attente du relachement.
4. Au relachement : sommeil SX1262 code 0 et annonce d'entree en veille.
5. Aucune sortie serie ni redemarrage pendant les 8 secondes suivantes.
6. Signal RST : nouveau demarrage v2, OLED initialise et ecoute retablie.
7. Second cycle de veille reussi, suivi de 8 secondes sans sortie serie.

Test termine avec succes. **Carte laissee en veille**, port USB ferme.
Pour reprendre, appuyer sur RST sans maintenir PRG.

## Limites

Le courant sur batterie et l'etat physique de l'affichage n'ont pas ete
mesures par un instrument pendant cet essai. Le test confirme les commandes
de sommeil radio, l'execution de la sequence de veille et la reprise apres
reset. La veille conserve une consommation residuelle ; ce n'est pas une
deconnexion physique de la batterie.

Le projet de reception reste autonome dans `firmware/PlantHubDiagnostic/`.
La sauvegarde d'usine du premier essai est conservee dans `.tools/private/`.
L'autre carte COM4 n'a pas ete reprogrammee.
