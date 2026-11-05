---
title: 'Plant Station : robot et tableau de bord local'
type: 'feature'
created: '2026-11-04'
status: 'in-progress'
route: 'dispatch'
baseline_commit: '417908321bd610b6f82d0fe941eb1805d64cd5ca'
review_loop_iteration: 0
context: []
---

<frozen-after-approval reason="interface demandée ; décisions utilisateur Wi-Fi domestique, un mois, calibrage ultérieur">

## Intent

Suivre quatre plantes par un robot expressif sur le Heltec et un tableau de
bord mobile local accessible par QR. Wi-Fi domestique et historique persistant
de trente jours glissants. Réglages/calibrage après : préparer leurs états,
sans seuil inventé.

## Boundaries & Constraints

**Toujours :** nouveau profil `heltec_v3_plant_dashboard` ; PS v1, quatre
identifiants, cadence 30 min, péremption 90 min, lux et profils conservés.
Un état partagé alimente OLED/API. Sans seuil : « à étalonner » ; absence,
défaut ADC et mesure périmée restent distincts de « sèche ». Aucun pourcentage
d’humidité sans référence. Aucun reflash des XIAO.

**Jamais :** publication Internet, compte, dépendance web externe, automatisme
d’arrosage, nouvelle calibration imposée ou promesse d’autonomie. Les exemples
restent dans l’aperçu explicitement fictif, séparés de la page réelle.

## I/O & Edge-Case Matrix

| Situation | Résultat |
| --- | --- |
| Quatre mesures fraîches sans seuil | Robot neutre, quatre « à étalonner » |
| Toutes qualifiées satisfaisantes | Sourire |
| Une ou plusieurs sèches | Mine triste, liste explicite des identifiants |
| Absence/défaut/péremption | Visage inquiet, avertissement distinct ; jamais sèche par déduction |
| Wi-Fi absent | Réception/OLED maintenus ; QR remplacé par état réseau |
| Redémarrage | Historique conservé ; valeurs courantes en attente de nouveaux paquets |
| PRG court / long puis relâché | Visage→détails→QR / veille existante préservée |

## Ajout utilisateur pendant l’implémentation

Ajouter un indicateur de batterie propre à chaque sonde dans les cartes web et
le détail OLED. Le contrat PS v1 actuel ne transporte pas la batterie : exposer
un champ absent/inconnu, afficher « non communiqué » / « -- » et ne jamais
inventer un pourcentage ou assimiler une absence de donnée à 0 %. Les valeurs
d’exemple sont réservées à l’aperçu fictif. L’ajout de la mesure dans les sondes
reste distinct de la préparation de l’interface ; aucun reflash XIAO ici.

</frozen-after-approval>

## Code Map

Racine des chemins `src/` et `test/` : `firmware/PlantStationVeml7700/`.

- `src/main.cpp`, `src/radio/SoilRadio.*` : banque, propriétaire SPI et
  initialisation asynchrone à préserver.
- `src/display/OledDisplay.*`, `src/power/PrgButton.h`,
  `src/power/SleepPower.*` : OLED 128×64, bouton, veille/reprise.
- `lib/PlantSoilProtocol/src/PlantSoilProtocol.h` : fraîcheur et brut→µV.
- `firmware/EnviroTerminalDiagnostic/WebDashboard.cpp` : reprendre les principes
  BLE Security1, reconnexion, mDNS ; pas d’inclusion directe (BMP180/GPIO9).
- `design/plant-dashboard-preview/index.html` : aperçu autonome et interactif,
  fictif, base visuelle sans connexion matérielle.

## Tasks & Acceptance

**Exécution :**

- [ ] `src/ui/PlantView.*` et `src/main.cpp` : centraliser états, noms,
  qualification configurable mais initialement indéfinie ; garder les cycles.
- [ ] `src/display/OledDisplay.*`, `src/power/PrgButton.h` : visage, détails,
  page QR lisible avec marge calme ; seuil 3 s et relâchement inchangés.
- [ ] `firmware/PlantStationVeml7700/platformio.ini`, `src/web/PlantDashboard.*`,
  `src/web/PlantDashboardPage.h` : profil dédié et adaptation de l’aperçu,
  serveur embarqué, API état/historique, Wi-Fi domestique/BLE, mDNS et URL/IP.
- [ ] `src/storage/PlantHistory.*` : journal borné et résistant aux coupures,
  conservation 30 jours ; configuration séparée des mesures.
- [ ] `test/`, `tests/`, documentation et preuves : tester intégration et
  erreurs, compiler les profils, revue indépendante puis essai Heltec.

**Critères d’acceptation :**

- Étant donné les quatre états, quand OLED et API sont actualisés, alors
  ils désignent les mêmes plantes ; aucun exemple ne fuit en mode réel.
- Étant donné un téléphone sur le même réseau, quand le QR réel est scanné,
  alors la page locale s’ouvre et affiche identifiant, tension, âge et état.
- Étant donné trente jours de données simulées puis un redémarrage, quand
  l’historique est relu, alors les quatre courbes restent distinctes, les trous
  restent visibles et les données courantes ne redeviennent pas fraîches.
- Étant donné HTTP lent, perte Wi-Fi, flash pleine/corrompue ou OLED absent,
  quand la boucle tourne, alors radio, lux et PRG restent disponibles.

## Implementation Notes

- Base `417908321bd610b6f82d0fe941eb1805d64cd5ca` ; préserver les deux rapports
  historiques modifiés et les fichiers non suivis préexistants.
- Empreinte substantielle et interfaces réseau/stockage nouvelles : dispatch.
- Décisions utilisateur : Wi-Fi maison, trente jours au lieu de sept,
  interface maintenant, réglages/calibrage après. Aucun secret demandé au chat.
- Investigation indépendante et aperçu préparés ; aucune modification matérielle.

## Spec Change Log

## Review Triage Log

## Design Notes

Palette crème/vert, quatre cartes, courbes 24 h /7 j /30 j, noms personnalisables.
Robot souriant, triste, inquiet ou neutre.
Les informations de panne restent visibles même avec une alerte de sécheresse.
Connexion au Wi-Fi via provisioning BLE existant adapté ; mot de passe saisi
sur le téléphone. Le QR d’accès contient l’URL, distinct du QR BLE de configuration.

API bornée : `GET /api/status`, `/api/history` paginé par nœud/période ;
`POST /api/time` et `/api/plants` pour horloge et noms, validation stricte.
Horloge synchronisée depuis le téléphone ; sinon indiquer heure inconnue,
sans inventer de dates ni modifier la péremption basée sur l’uptime.
Journal borné des mesures acceptées, récupération d’écriture interrompue ;
aucune écriture flash par requête web.
Partition existante `spiffs` de 1,5 Mio ; conserver la table 8 Mio.
Sauvegarder avant installation ; ne pas formater une partition contenant
des données inconnues. La boucle radio ne doit attendre ni HTTP ni la flash.
Arrêter proprement réseau et stockage avant la veille.

## Verification

- Compiler les trois profils Heltec existants et `heltec_v3_plant_dashboard`.
- Exécuter `tools/run_soil_lora_host_tests.py`, tests lux/veille existants et
  nouveaux tests de modèle, historique, API et navigation.
- Vérifier navigateur mobile/ordinateur : scénarios, courbes vides, hors ligne,
  noms longs, clavier ; aucune ressource distante ni erreur JavaScript.
- Sur Heltec : BLE/Wi-Fi, réception réelle pendant HTTP, QR scanné au téléphone,
  PRG/RST et historique après redémarrage. Distinguer simulation et preuve réelle.


- Approbation utilisateur « Valider et continuer » reçue le 4 novembre ; bloc frozen verrouillé.
- Outils locaux : Python `C:/Users/Anwender/AppData/Local/Programs/Python/Python312/python.exe`, `PYTHONUTF8=1`, `PYTHONPATH=.tools/xiao-diagnostic-python`, `PLATFORMIO_CORE_DIR=.tools/platformio`. Utiliser cette installation PlatformIO pour les deux projets. Pour esptool : `PYTHONPATH=.tools/python`, paquet `.tools/platformio/packages/tool-esptoolpy/esptool.py`. Les écritures outils/compilations peuvent demander l’exécution escaladée.
- Carte Heltec autorisée COM5, CP2102 VID10C4:EA60, numéroUSB0001, MAC3c:0f:02:eb:f5:28. Lectures série115200 DTR=false/RTS=false définis avant ouverture ; ne pas laisser PRG tenu. XIAO1–4 sur batterie, aucun flash ni changement demandé. Sauvegardes binaires et logs sensibles dans `.tools/private/`, ne jamais les committer ni afficher les secrets. Installation application seule à0x10000 après tests, garder bootloader/partition/NVS. Vérifier la partition historique avant toute initialisation, ne pas effacer des données inconnues.
- Source visuelle autonome `design/plant-dashboard-preview/index.html`, aperçu déjà servi en127.0.0.1:8788 (session66945). Ne pas remplacer la maquette fictive par des valeurs prétendues réelles ; nouvelle page embarquée distincte. Syntaxe JavaScript de l’aperçu vérifiée.

- Changement explicitement demandé par l’utilisateur pendant le développement : indicateur batterie pour chaque sonde. Ajout à son intention gelée autorisé par cette nouvelle demande ; support visuel et valeur inconnue réels, car les trames actuelles n’ont pas ce champ.
