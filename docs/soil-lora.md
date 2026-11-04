# SOIL-01 : test XIAO → Heltec, LoRa 868 MHz

Le hub accepte aussi **SOIL-02, SOIL-03 et SOIL-04**. Voir [quatre sondes](soil-four-nodes.md) pour
le profil dédié, l'affichage et la réception indépendante des quatre nœuds.

Ce test transporte la mesure brute ADS1115 de la sonde capacitive SOIL-01
vers l'OLED du Heltec. La tension est derivee du meme entier aux deux bouts :
`microvolts = brut × 125`. Il ne produit ni pourcentage d'humidite ni conseil
d'arrosage. Les antennes 868 MHz doivent etre raccordees avant alimentation.
Les identifiants de noeud, session, compteur et CRC ne fournissent **aucune
authentification**. Les journaux et l'OLED le signalent.

## Profils et compilation

Les profils historiques `xiao_soil_diagnostic`, `heltec_v3_text` et
`heltec_v3_json` restent disponibles sans initialisation radio. Les nouveaux
profils sont `xiao_soil_lora_tx` et `heltec_v3_soil_lora` ; RadioLib est figee
a **7.8.1**. Les versions des coeurs et des autres bibliotheques restent celles
des projets existants. La version du diagnostic lux et son schema JSON
restent `1.2.0` ; le profil radio utilise les lignes texte `SOIL_*` en plus
des diagnostics lux habituels.

Depuis la racine du depot, avec les paquets locaux deja installes :

```powershell
$env:PYTHONPATH = Join-Path $PWD '.tools/xiao-diagnostic-python'
$env:PLATFORMIO_CORE_DIR = Join-Path $PWD '.tools/platformio'
python -m platformio run --project-dir firmware/XiaoSoilDiagnostic -e xiao_soil_diagnostic -e xiao_soil_lora_tx
$env:PYTHONPATH = Join-Path $PWD '.tools/python'
python -m platformio run --project-dir firmware/PlantStationVeml7700 -e heltec_v3_text -e heltec_v3_json -e heltec_v3_soil_lora
python firmware/PlantStationVeml7700/test/run_host_tests.py
python tools/run_soil_lora_host_tests.py
python tests/test_capture_soil_lora.py
```

Les commandes ci-dessus compilent sans flasher. Avant installation, conserver
l'application et la configuration des deux cartes ; ne pas effacer la flash
ni changer le chargeur. Les ports COM peuvent changer apres installation.
Le profil XIAO lit et emet aussi sans terminal USB ouvert. D0 reste une
entree sans tirage ; la logique du bouton/P-MOS n'est pas commandee.
Dans le profil TX, chaque ligne USB est assemblee avant emission. Le chemin
CDC non bloquant attend au plus 4 ms pour une ligne entiere de 256 octets
maximum ; une ligne perdue lorsque DTR est actif est annoncee ensuite par
`[SOIL_LOG] dropped_lines=...`. Un terminal deconnecte ne cree pas de perte
de preuve. Un hote qui garde DTR actif sans lire ne bloque pas les mesures
ni les emissions LoRa. Le profil USB historique garde son journal habituel.

## Branchements

Conserver ADS1115 et sonde sous 3,3 V, avec masses communes : XIAO D6/P1.11
vers SDA, D7/P1.12 vers SCL, AOUT vers A0. Un unique candidat ADS entre
`0x48` et `0x4B` est requis. Ne pas toucher au strap ADDR sous tension.
Le bus logiciel existant garde ses attentes SCL bornees a 2 ms et ne force
jamais un niveau haut. Le scan reste repete tous les dix essais.

| SX1262 du kit XIAO | XIAO |
| --- | --- |
| SCK / MISO / MOSI | D8 / D9 / D10 |
| NSS / DIO1 / RESET / BUSY | D4 / D1 / D2 / D3 |
| RXEN | D5 |
| TX RF switch / TCXO | DIO2 / 1,8 V |

Le module RadioLib recoit explicitement `SPI`, puis `SPI.begin()` est appele.
Ce chemin du coeur Seeed ne configure pas SS/D7, qui reste le SCL ADS.
RXEN est configure par `setRfSwitchPins(5, RADIOLIB_NC)` ; DIO2 commande TX.

Le Heltec conserve VEML7700 sur GPIO4/5, adresse `0x10`, et OLED sur le second
controleur I2C GPIO17/18, `0x3C`, RESET21, Vext36 actif bas. Sa radio integree
utilise SCK9/MISO11/MOSI10/NSS8/RESET12/BUSY13/DIO1=14, DIO2 RF switch,
TCXO 1,8 V. Les erreurs radio laissent tourner les lux a 1 s et PRG.

## Format radio strict, version 1

Trame fixe de **20 octets**, sans structure C copiee directement ; les entiers
multioctets sont encodes en petit-boutiste.

| Octet(s) | Champ |
| --- | --- |
| 0–1 | Signature ASCII `PS` |
| 2 | Version `1` |
| 3 | Noeud `1` = SOIL-01, `2` = SOIL-02, `3` = SOIL-03, `4` = SOIL-04 ; autres identifiants rejetes |
| 4–7 | Session de demarrage non nulle, entier 32 bits |
| 8–11 | Compteur circulaire 32 bits |
| 12–15 | Uptime XIAO en ms, modulo 2³² |
| 16 | Statut : `0` OK, `1` ADS indisponible, `2` erreur lecture, `3` hors plage |
| 17 | Adresse ADS `0x48`–`0x4B`, ou `0` uniquement si indisponible |
| 18–19 | Brut signe 16 bits ; `0` canonique pour tout defaut |

Une mesure OK accepte les codes `-8` a `26400` (offset possible pres de zero,
plage nominale jusqu'a 3,3 V). Une lecture hors plage reste dans le journal USB
ADC existant, mais devient un defaut radio explicite sans valeur courante.
Les versions, noeuds, statuts, longueurs et combinaisons adresse/brut inconnus
sont rejetes. Le statut de defaut est transmis meme si l'ADS ne repond pas.

La session utilise quatre octets du RNG materiel nRF52840, avec une attente
totale de 25 ms. Un echec ou zero interdit l'emission et declenche une nouvelle
tentative espacee de 10 s. Ce nombre distingue les redemarrages sans preuve
de securite et sans garantie mathematique d'unicite.

Pour chaque noeud, une nouvelle session remplace son etat precedent. Dans la session courante,
un compteur n'est nouveau que si la difference non signee est dans
`1..0x7fffffff`. Doublon, ancien compteur, paquet malforme et erreur CRC ne
rafraichissent ni mesure ni age. A **5 400 000 ms (90 minutes)** sans nouveau paquet accepte,
l'ecran retire brut et volts et affiche « PERIMEE ». Un defaut ADS
accepte remplace immediatement la valeur. L'expiration reste acquise jusqu'au
paquet suivant, meme apres un tour complet de `millis()` ; les calculs de
cadence et d'age utilisent des differences non signees.

L'OLED conserve lux, les quatre tensions arrondies et ages individuels, leurs etats et celui de la radio ; les bruts restent dans les journaux USB. Un
nouveau paquet, un changement radio ou la frontiere de peremption rafraichit
l'ecran sans attendre la prochaine mesure lux. Le service RX est non bloquant
entre interruptions ; chaque traitement relance
la reception. Il place d'abord la radio en standby, puis controle longueur,
presence du CRC dans l'en-tete LoRa explicite et charge utile sur le meme
paquet fige ; un echec est rejete et la reception est relancee.
Les reinitialisations apres erreur sont espacees de 10 s. La recherche
synchrone de puce de RadioLib s'execute dans une tache privee : elle possede
seule SPI/radio jusqu'a publication atomique de son resultat. La boucle
principale continue lux et PRG pendant une initialisation lente ou en panne.
PRG 3 s puis relachement met capteur, OLED et radio en sommeil. Si SX1262
ne confirme pas le sommeil, RESET radio reste bas. Si une initialisation est
encore en cours, la demande reste acquise et les GPIO de sommeil ne sont
prepares qu'apres son retour, pour eviter tout acces concurrent. RST restaure
les fonctions.

## Parametres et temps d'antenne

868,1 MHz, BW125 kHz, SF7, CR4/5, sync prive `0x12`, CRC 2 octets, en-tete
explicite, preambule 8, puissance emise conduite **0 dBm**. Une trame courante
est tentee toutes les **30 minutes**, sans repetition, ACK ni rattrapage apres retard.
Le compteur avance egalement lors d'un echec d'emission. La prochaine trame
contient une nouvelle lecture plutot qu'une ancienne valeur.

Pour 20 octets, le calcul LoRa donne `(8 + 4,25 + 43) × 1,024 ms = 56,576 ms`,
soit **0,003143 %** sur 30 minutes. Le TX verifie aussi `getTimeOnAir(20) <= 300000 µs`
avant de declarer la radio prete : ce plafond historique reste conservateur pour la nouvelle cadence.
L'[annexe, bande 48, de la decision UE 2025/105](https://eur-lex.europa.eu/eli/dec_impl/2025/105/oj/eng/pdf)
prevoit pour 868–868,6 MHz une limite de 25 mW PAR avec, en alternative aux
techniques d'acces au spectre indiquees, un rapport cyclique maximal de 1 %.
Le calcul ci-dessus porte sur ce test, pas sur une homologation du montage.

## Capturer et comparer

Fermer les moniteurs serie et choisir un prefixe neuf ; l'outil refuse
d'ecraser des preuves. Le fichier JSON est reserve exclusivement avant tout
acces aux ports. Les deux ports sont ouverts avant une fenetre commune,
puis leurs tampons d'entree sont vides. Chaque ligne porte un horodatage hote
`[HOST t_s=...]`. Les lectures se font simultanement, avec timeout, lignes
bornees et une duree limitee a 7200 s (deux heures).

```powershell
python tools/capture_soil_lora.py --tx-port COM6 --rx-port COM5 --seconds 3700 --output-prefix diagnostics/soil-lora-30min-hardware
```

Avec la cadence de 30 minutes, 3700 s permettent d'observer trois releves
si la capture commence avant le premier cycle suivant le demarrage du profil
P-MOS (premier releve apres 30 s). Sinon, choisir une fenetre plus longue,
jusqu'a 7200 s. Un controle court peut seulement confirmer le premier echange :
il ne valide pas trois periodes reelles, qui sont aussi testees avec une horloge
simulee. Le profil USB historique continue ses lectures locales a environ 1 s ;
les deux profils radio XIAO respectent maintenant un garde TX de 30 minutes.
Sans option `--seconds`, la capture dure **5500 s**, pour couvrir trois releves
meme si elle commence juste apres une emission.

Ce prefixe est un exemple : verifier les vrais ports. Sorties distinctes :
`-tx.log`, `-rx.log` et `.json`. La comparaison porte exactement sur noeud, session,
compteur, uptime, statut, adresse, brut et microvolts. Elle signale pertes TX,
RX sans TX apparie, doublons, divergences, defauts ADC, rejets et erreurs de
capture. Une marge de1s a chaque extremite distingue les paires de frontiere
des pertes dans la fenetre pleinement observable ; ces paires sont conservees
dans `boundary_pairs`. La sortie vaut zero seulement avec au moins trois
**mesures valides par noeud suivi** appariees dans cette fenetre et aucune erreur/perte.
Une interruption, un UTF-8 invalide ou une ligne de plus de8192octets est une
erreur explicite, avec conservation des octets defectueux en hexadecimal.
Une ligne partielle commencee dans la derniere seconde est conservee comme
frontiere ; une ligne restee partielle plus longtemps est une erreur. Une
capture vide n'est jamais une reussite. Ctrl+C arrete les lecteurs avant de
fermer les ports, puis conserve un rapport `completed:false` et le code retour2.

Exemple de ligne (simulee) :

```text
[SOIL_TX] authenticated=no node=SOIL-01 session=1234 seq=1 uptime_ms=30000 status=ok address=72 raw=21565 uv=2695625
```

La ligne RX correspondante doit etre identique sauf `TX`/`RX`. Le diagnostic
USB ADS `[MESURE] ... A0_brut=... A0_V=...` garde son format existant et est
produit par la meme lecture utilisee pour le paquet.

## Preuves et limites

Les tests hote couvrent codec, champs, longueurs, defauts, sessions, ordre,
debordements, frontiere 90 min, remplacement des valeurs OLED, reessais radio,
lux concurrents et veille/repli RESET avec les vrais modules firmware. Les
tests cibles de `tools/run_soil_lora_host_tests.py` executent aussi le vrai
`serviceTransmit` et le vrai `setup/loop` XIAO : espacement minimal 30 min,
absence de rattrapage, debordement, compteur apres erreur TX, echec/reprise du
RNG, statuts ADS et terminal absent ou bloque. Le main Heltec est teste avec
une initialisation differee de plus de 14 s, PRG et attente de sommeil, ainsi
qu'avec de nouvelles trames distinctes dont le CRC est absent ou incorrect.
Les tests historiques Heltec et leur schema JSON strict sont conserves. Les tests
de capture n'ouvrent aucun port serie. Les journaux historiques du 3 novembre sont dans
`diagnostics/soil-lora-host-tests-20261103.log`,
`diagnostics/soil-lora-xiao-builds-20261103.log` et
`diagnostics/soil-lora-heltec-builds-20261103.log`.

Pour la cadence de 30 minutes, `python tools/run_soil_lora_host_tests.py`
execute maintenant **66 scenarios** sur le protocole partage et les vrais firmwares,
dont premier cycle apres 30 s, frontiere 30 min, attente sans alimentation de
sonde, commandes AUTO repetees et maintien OLED jusqu'a la frontiere 90 min.
Les preuves du changement sont consignees dans
[le compte rendu cadence du 4 novembre](../diagnostics/soil-30min-20261104.md) ;
les cas multisonde et leur installation sont decrits dans [quatre sondes](soil-four-nodes.md).

Les corrections historiques du 3 novembre etaient aussi verifiees par
`python tools/run_soil_lora_host_tests.py` : neuf scenarios executaient les vrais
firmwares, notamment initialisation differee14s, CRC absent/incorrect,
cadence TX, RNG et USB bloque/deconnecte. Les **vingt-six tests** actuels de
`python tests/test_capture_soil_lora.py` couvrent appariement, frontieres,
corruption, interruption, nettoyage des lecteurs et reservation exclusive
du rapport. Aucun de ces tests n'accede aux vraies cartes.

Les preuves materielles sont a enregistrer separement par l'agent principal :
trois paquets apparies, lux pendant reception, observation OLED, absence ou
defaut sans ancienne valeur, PRG/veille/RST et retour reception. Les tests
logiciels ne constituent pas une preuve de reception radio reelle. Aucune
page Web, authentification, calibration, optimisation batterie ni garantie
de livraison n'est apportee par ce premier test.
