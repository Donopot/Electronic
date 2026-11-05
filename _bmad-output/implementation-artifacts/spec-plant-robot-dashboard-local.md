---
title: 'Plant Station : robot et tableau de bord local'
type: 'feature'
created: '2026-11-04'
status: 'in-review'
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

## Évolution utilisateur : kit à offrir et configuration sans application

L’utilisateur veut offrir le kit, changer facilement de Wi-Fi sans ordinateur,
et scanner un QR qui guide la configuration. Il choisit explicitement un
assistant web local sans application, qui remplace le parcours BLE préparé.
Au premier démarrage ou si le Wi-Fi enregistré reste indisponible, proposer un
Wi-Fi temporaire du kit protégé par un mot de passe propre au kit. Le QR sur
l’OLED permet de le rejoindre ; un assistant local fait choisir le Wi-Fi de
la maison et saisir son mot de passe. Après succès, le kit rejoint ce réseau
et présente le QR du tableau de bord. Prévoir un lien/QR d’adresse de secours
si le portail ne s’ouvre pas automatiquement.

Un geste expliqué sur le Heltec doit permettre de rouvrir la configuration
pour une nouvelle maison, sans PC ni commande série. Garder PRG court pour
les vues et PRG3s puis relâcher pour la veille ; un maintien plus long peut
ouvrir la configuration avec indication à l’écran. Préserver l’ancien réseau
pendant une panne temporaire ; remplacer ses identifiants seulement après
connexion réussie au nouveau. Garder l’historique, les noms et les quatre
sondes. Aucun mot de passe dans l’API de suivi ou les journaux publics.

</frozen-after-approval>

## Code Map

Racine des chemins `src/` et `test/` : `firmware/PlantStationVeml7700/`.

- `src/main.cpp`, `src/radio/SoilRadio.*` : banque, propriétaire SPI et
  initialisation asynchrone à préserver.
- `src/display/OledDisplay.*`, `src/power/PrgButton.h`,
  `src/power/SleepPower.*` : OLED 128×64, bouton, veille/reprise.
- `lib/PlantSoilProtocol/src/PlantSoilProtocol.h` : fraîcheur et brut→µV.
- `firmware/EnviroTerminalDiagnostic/WebDashboard.cpp` : reprendre les principes
  reconnexion et mDNS ; pas d’inclusion directe (BMP180/GPIO9). Le parcours BLE est remplacé par l’assistant local ci-dessous.
- `design/plant-dashboard-preview/index.html` : aperçu autonome et interactif,
  fictif, base visuelle sans connexion matérielle.

## Tasks & Acceptance

**Exécution :**

- [x] `src/ui/PlantView.*` et `src/main.cpp` : centraliser états, noms,
  qualification configurable mais initialement indéfinie ; garder les cycles.
- [x] `src/display/OledDisplay.*`, `src/power/PrgButton.h` : visage, détails,
  page QR lisible avec marge calme ; seuil 3 s et relâchement inchangés.
- [x] `firmware/PlantStationVeml7700/platformio.ini`, `src/web/PlantDashboard.*`,
  `src/web/PlantDashboardPage.h` : profil dédié et adaptation de l’aperçu,
  serveur embarqué, API état/historique, Wi-Fi domestique, assistant local et URL/IP.
- [x] `src/storage/PlantHistory.*` : journal borné et résistant aux coupures,
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

| Constat | Verdict | Route | Vérification / preuve |
| --- | --- | --- | --- |
| B1 | high | patch | Le SDK efface bien les identifiants avec reset_provisioned=true ; le chemin automatique à30s y arrive. Préserver la reconnexion et réserver le reset à wifi-setup explicite. |
| B2 | medium | patch | refresh relance loadHistory toutes5s tant que lastHistory ancien ; loadHistory annule le précédent sans garde loading. Téléchargement lent nominal bloqué. |
| B3 | medium | patch | fetch sans AbortController/délai ; refresh.busy reste vrai tant que le réseau ne rejette pas. Borner les appels et libérer le poll. |
| B4 | medium | patch | POST time exécuté une fois, catch vide ; aucune resynchronisation après reset du hub ou échec transitoire. Réessayer quand horloge absente, avec borne. |
| B5 | high | patch | Les dates reconstituées utilisent uniquement clockUtc/clockUptime RAM ; utcMs reste0 sur flash. Après reset, des courbes déjà affichées perdent leurs dates. Rendre la résolution durable. |
| B6 | medium | patch | begin ne décode que le dernier secteur pour corrupt_ ; page ignore sans compter les invalides plus anciens. Compter une fois tous les secteurs conservés. |
| B7 | medium | patch | 202/persistence pending est traité comme succès Enregistrer ; échec putString répété et shutdown silencieux. Confirmer durabilité ou rendre attente/échec explicites. |
| B8 | low | patch | maxlength24 compte les unités UTF16, serveur24points Unicode ; un emoji+23ASCII est valide mais bloqué. Correction directe de maxlength et validation existante. |
| B9 | low | patch | Lux140000 PRG:details fait22cellules/132pixels ; limite128. Raccourcir libellé et tester valeur maximale. |
| B10 | medium | patch | URL rafraîchie seulement si connected change ; GOT_IP peut changer IP sans WL_CONNECTED différent. Republisher adresse réelle. |
| B11 | medium | patch | Absence vérifiée de test du worker réel : méthodes remplacées et API Node fictive. Étendre tests sur les frontières de production, sans changer la surface fonctionnelle. |
| E1 | high | patch | Même effacement SDK queB1, déclenché après panne routeur30s ; vérifié dans WiFiProv.cpp local. |
| E2 | medium | patch | Même absence de nouvelle synchronisation queB4 après redémarrage avec page laissée ouverte. |
| E3 | medium | patch | Même annulation périodique du chargement queB2 pour historique prenant plus5s. |
| E4 | medium | patch | history.page filtre utcMs stocké0 puis route reconstitue une date hors période. Appliquer date effective avant filtre/pagination. |
| E5 | low | patch | Même débordement OLED six chiffres queB9 ; correction directe. |
| E6 | medium | patch | WiFiProv.cpp retourne silencieusement si mgr_init ou start échoue ; provisioning est déjà true, sans événement START ni échéance. Latch réel à reprendre proprement. |
| E7 | medium | patch | Même absence de refreshIP queB10 ; QR et adresse API périmés. |
| E8 | medium | patch | saveNames échoué avant finished=true en shutdown non signalé ; même durabilité annoncée prématurément queB7. |
| E9 | medium | patch | Même comptage limité au dernier secteur queB6 ; défaut ancien invisible dans compteur après rotation. |
| E10 | low | patch | Même divergence UTF16/pointsUnicode queB8 ; correction directe sans nouveau validateur. |
| E11 | medium | patch | Deux relevés séparés60min sont reliés car coupure>=90min. Un paquet30min manquant est masqué ; adapter seuil avec marge sur cadence nominale. |
| V1 | medium | patch | Pré-vérifié : test HTTP avance son propre sent et ne compile pas serviceHttp ; mutation sent+=remain non détectée. Tester automate de production et longueur exacte. |
| V2 | medium | patch | Pré-vérifié : journal rempli directement, publish remplacé ; xQueueSend supprimé non détecté. Tester ingestion réelle, dédoublonnage, champs et relecture. |
| V3 | medium | patch | Pré-vérifié : noms seulement en mémoire fictive, aucune restauration Preferences après save interval/shutdown. Tester production avec Preferences simulé et reboot. |
| V4 | medium | patch | Pré-vérifié : mock navigateur toujours more:false ; un break inconditionnel non détecté. Fournir plusieurs pages et point unique de dernière page. |

Groupes par cause : B1/E1 ; B2/E3 ; B3 ; B4/E2 ; B5 ; B6/E9 ; B7/E8 ; B8/E10 ; B9/E5 ; B10/E7 ; E4 ; E6 ; E11 ; B11/V1/V2/V3 (frontière worker non exécutée) ; V4. Tous sont des corrections locales dans la fonctionnalité et ses vérifications, sans nouvel endpoint ni décision produit ; aucun différé. Le défaut socket trouvé par le parent avant la revue est déjà corrigé et vérifié séparément.


### Revue de l’assistant sans application

| Constat | Verdict | Route | Vérification / preuve |
| --- | --- | --- | --- |
| R2-B1 | medium | patch | Après réussite puis déconnexion STA, state reste connected et masque le formulaire ; fermeture automatique exige online. Réinitialiser l’état de formulaire lors de cette transition. |
| R2-B2 | medium | patch | cancel ne vérifie ni candidatePending ni testingWifi ; une requête retardée après commit appelle rollback et coupe le réseau déjà enregistré. Refuser l’annulation après la fin de l’essai. |
| R2-B3 | medium | patch | Le commit FLASH peut réussir avant un échec de retour RAM ; savedWifi est alors nouveau mais le résultat annonce échec/ancien réseau. Refléter la réussite déjà persistée et garder la vérification RAM avant tout prochain candidat. |
| R2-B4 | medium | patch | Le formulaire des noms valide et poste dans la même boucle ; un dernier nom invalide arrive après les premiers POST. Prévalider tous les noms avant tout envoi, sans nouvel endpoint batch. |
| R2-B5 | medium | patch | Le Host LAN arbitraire et Origin égal à ce Host autorisent un scénario de DNS rebinding. Restreindre aux adresse et nom annoncés par le kit ; conserver la redirection captive AP. |
| R2-B6 | maybe-false | defer | Les scans synchrones de secours existent ; un dépassement des délais HTTP dépend de la distribution des boots/ancrages et des latences flash réelles. Il manque un benchmark plein journal sur ESP32 pour confirmer le préjudice annoncé (medium si confirmé). |
| R2-B7 | false | reject | Un marqueur interrompu ne permet pas d’établir la propriété de la partition ; unknown_data_preserved est précisément le refus conservateur prévu pour toute propriété non prouvée. Il est signalé dans l’API, les acquisitions continuent et aucune donnée antérieure n’est effacée. Une réinitialisation automatique violerait cette garantie de conservation. |
| R2-B8 | maybe-false | defer | Le SDK active LWIP_HOOK_IP4_ROUTE_SRC ; le seul chevauchement de préfixe ne prouve donc pas l’échec annoncé. Vérifier sur deux interfaces réelles avec routeur/STA/AP 192.168.4.0/24, y compris adresses identiques et DHCP, avant de modifier le sous-réseau du portail (medium si confirmé). |
| R2-B9 | medium | patch | Pendant le maintien PRG, main retourne avant soilRadio.service ; deux paquets rapprochés peuvent dépasser la réception gardée en FIFO. Servir/publier la radio pendant l’attente en conservant les invites OLED. |
| R2-B10 | medium | patch | Une pagination échouée laisse un préfixe dans history ; resize appelle chart qui efface l’avertissement. Garder un état d’échec/incomplétude jusqu’au prochain chargement réussi. |
| R2-B11 | low | reject | La correction d’une horloge de téléphone initialement fausse n’est pas proposée une fois l’heure connue ; cas peu rencontré avec les téléphones synchronisés, dérive d’un mois négligeable à la cadence 30min. Une nouvelle action/politique de réécriture des dates dépasse une correction directe ; rejet selon la règle low/complexité. |
| R2-E1 | medium | patch | startSetup retourne immédiatement si l’AP est déjà ouvert ; PRG8s après succès ne rouvre pas le formulaire pendant les cinq minutes de grâce. Réinitialiser le formulaire et ses échéances pour cette demande explicite. |
| R2-E2 | medium | patch | Même transition vérifiée que R2-B1 : déconnexion après succès et formulaire masqué. |
| R2-E3 | medium | patch | quote conserve les octets UTF8 invalides d’un SSID scanné ; response.json rejette alors toute la liste. Écarter les SSID non représentables en UTF8, sans modifier les noms valides. |
| R2-V1 | medium | patch | Constat pré-vérifié : la tâche enregistrée n’est jamais exécutée par le harness. Exécuter worker borné et faire avancer une connexion soumise via HTTP sans appels manuels à serviceNetwork. |
| R2-V2 | medium | patch | Constat pré-vérifié : aucune réponse HTTP d’historique valide n’est assertée, contrairement au stockage direct et aux fixtures navigateur. Tester le contrat réel, tensions/null, dates et pagination. |

Groupes par cause : R2-B1/R2-E2 ; chaque autre patch est indépendant. Les deux
hypothèses matérielles sont consignées pour mesure, sans changement destructif
ni nouvel état produit. Les corrections sont locales, sans nouvelle surface API.

## Design Notes

Palette crème/vert, quatre cartes, courbes 24 h /7 j /30 j, noms personnalisables.
Robot souriant, triste, inquiet ou neutre.
Les informations de panne restent visibles même avec une alerte de sécheresse.
Connexion Wi-Fi sans application via réseau temporaire protégé et assistant web. Le QR de connexion contient les paramètres du Wi-Fi temporaire ; une seconde vue fournit son URL de secours. Après configuration, le QR ouvre le tableau de bord sur le réseau domestique.

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
- Sur Heltec : assistant Wi-Fi local, réception réelle pendant HTTP, QR scanné au téléphone,
  PRG/RST et historique après redémarrage. Distinguer simulation et preuve réelle.


- Approbation utilisateur « Valider et continuer » reçue le 4 novembre ; bloc frozen verrouillé.
- Outils locaux : Python `C:/Users/Anwender/AppData/Local/Programs/Python/Python312/python.exe`, `PYTHONUTF8=1`, `PYTHONPATH=.tools/xiao-diagnostic-python`, `PLATFORMIO_CORE_DIR=.tools/platformio`. Utiliser cette installation PlatformIO pour les deux projets. Pour esptool : `PYTHONPATH=.tools/python`, paquet `.tools/platformio/packages/tool-esptoolpy/esptool.py`. Les écritures outils/compilations peuvent demander l’exécution escaladée.
- Carte Heltec autorisée COM5, CP2102 VID10C4:EA60, numéroUSB0001, MAC3c:0f:02:eb:f5:28. Lectures série115200 DTR=false/RTS=false définis avant ouverture ; ne pas laisser PRG tenu. XIAO1–4 sur batterie, aucun flash ni changement demandé. Sauvegardes binaires et logs sensibles dans `.tools/private/`, ne jamais les committer ni afficher les secrets. Installation application seule à0x10000 après tests, garder bootloader/partition/NVS. Vérifier la partition historique avant toute initialisation, ne pas effacer des données inconnues.
- Source visuelle autonome `design/plant-dashboard-preview/index.html`, aperçu déjà servi en127.0.0.1:8788 (session66945). Ne pas remplacer la maquette fictive par des valeurs prétendues réelles ; nouvelle page embarquée distincte. Syntaxe JavaScript de l’aperçu vérifiée.

- Changement explicitement demandé par l’utilisateur pendant le développement : indicateur batterie pour chaque sonde. Ajout à son intention gelée autorisé par cette nouvelle demande ; support visuel et valeur inconnue réels, car les trames actuelles n’ont pas ce champ.

- Audit parent du diff complet initial effectué : modèle partagé, stockage, OLED/main/PRG, page et tests lus. Matrice : modèle couvre neutre/sourire/sec+avertissement/attente/ADC/péremption ; intégration vraie boucle couvre Wi-Fi absent, détails/QR, PRG et OLED absent ; journal simule31j/reboot et corruption, tests navigateur exécutés. Réception/provisioning/scan QR et persistance physique restent à vérifier après revue, conformément à l’ordre du plan.
- Défaut bloquant trouvé pendant cet audit : `WiFiClient::availableForWrite` hérité renvoie0 dans le SDK installé. Correctif socket non bloquant appliqué par l’implémenteur, tests ciblés du helper et contrat SDK exécutés, compilation réussie. Le code courant utilise `send(MSG_DONTWAIT)` borné, sans attente de client.

- Corrections des26constats appliquées : les trois reviewers initiaux confirment qu’aucun constat initial ne reste ouvert. Le parent a relancé4compilations,66scénariosradio, suite lux/veille, tests dashboard incluant vrai worker/HTTP et navigateurdesktop/mobile : tous PASS. Retrait de PSv1 dans les libellés produit, documentation technique conservée. Manifeste de provenance : `diagnostics/plant-dashboard-verification-manifest-20261104.json`.
- Installation réelle Heltec COM5/MAC attendu réussie : application seule0x10000,1444192octets,SHA256929022739474ba02f256987f7d1a541d0d5de9347152202ee937a6f9a2696f07,table de partitions comparée à la sauvegarde et inchangée. Démarrage :15mesures lux valides, aucune erreur fatale ; deux appuis courts PRG demandés pour afficher configuration BLE. Saisie Wi-Fi sur téléphone demandée, essaisQR/API/historique physique encore en attente. Aucun reflash XIAO.

- Évolution explicite en cours d’essais matériels : demande de QR de configuration sur le Heltec, puis précision d’usage cadeau. L’utilisateur choisit ensuite « Sans application : assistant web local » ; ce choix remplace le parcours ESP BLE Provisioning. Reprise de l’implémentation sur cette seule évolution, autres corrections validées à préserver. Aucune revalidation générale du plan demandée : les décisions produit sont explicites.


## Implémentation ciblée : assistant cadeau sans application

Remplacer seulement l’onboarding BLE, préserver les correctifs déjà revus et les
interfaces de suivi. Ne pas relancer un workflow imbriqué ni toucher les XIAO.
Implémenter et tester ; l’installation et les gestes matériels restent au parent.

- `src/web/PlantDashboard.*` : SoftAP WPA2 `PLANT-<suffixe MAC>`, mot de passe
  propre au kit généré aléatoirement et persisté en NVS. Ne jamais exposer ce
  secret dans l’API de suivi, les logs ou les captures publiques. Pas d’AP ouvert.
  Démarrer immédiatement sans réseau enregistré, ou après 30 s sans connexion,
  en gardant les identifiants existants. Continuer la reconnexion au réseau connu.
- Assistant en AP+STA, DNS captif et routes des sondes de portail ; page locale
  autonome, Wi-Fi 2,4 GHz, scan asynchrone et saisie manuelle, retour d’erreur et
  nouvelle tentative. Échapper les SSID, ne pas renvoyer les mots de passe.
  Tester un candidat en RAM ; enregistrer en flash seulement après GOT_IP.
  Si échec ou annulation, restaurer l’ancien réseau sans effacer la NVS.
  Ne pas fermer l’AP pendant le formulaire ; après succès afficher l’URL LAN
  et demander de rejoindre le Wi-Fi domestique. Borne de fermeture explicite.
- `src/web/PlantSetupPage.h` (nouveau), tests navigateur : parcours guidé,
  délais bornés, erreurs de réseau, SSID spéciaux, aucune dépendance distante.
  Ajouter les endpoints de configuration nécessaires sans élargir les corps
  des autres API. Valider l’origine/Host et limiter la configuration au côté AP.
- `src/display/OledDisplay.*` : générer le QR standard `WIFI:T:WPA;S:...;P:...;;`.
  QR adaptatif avec marge calme de 4 modules, texte sans chevauchement ; conserver
  le QR URL du tableau de bord. Vue de secours URL du portail `http://192.168.4.1/`.
  Indiquer scan, ouverture de l’assistant puis choix du Wi-Fi maison.
- `src/power/PrgButton.h`, `src/main.cpp` : PRG court conserve les vues ; 3 s puis
  relâcher conserve la veille ; maintien 8 s puis relâcher ouvre la configuration
  et annule la veille demandée. À 3 s indiquer les deux possibilités, à 8 s
  confirmer la configuration. Comportement des profils historiques inchangé.
- `test/test_dashboard_worker.cpp`, mocks SDK, tests OLED/bouton et outils :
  exécuter le vrai worker, commit/rollback des identifiants, panne transitoire,
  AP protégé, QR payload/taille, geste 8 s, et tests JS réels de l’assistant.
  Compiler les quatre profils Heltec et suites appropriées ; docs et guide
  cadeau expliquent l’accès de secours et le changement de maison sans PC.

Critères spécifiques :
- Étant donné un kit neuf, quand le téléphone scanne le QR, il rejoint le Wi-Fi
  protégé du kit ; l’assistant local permet de choisir le réseau domestique.
- Étant donné un mauvais mot de passe, quand la connexion échoue, l’assistant
  permet de corriger ; l’ancien réseau reste conservé jusque succès du nouveau.
- Étant donné un kit connecté, quand PRG est maintenu 8 s puis relâché,
  la configuration redevient accessible sans PC et sans perte d’historique.
- Étant donné un routeur temporairement absent, quand le kit attend,
  radio/OLED continuent et les identifiants enregistrés ne sont jamais effacés.

Vérifier les API contre le SDK local Arduino-ESP32 2.0.17, et notamment que le
changement RAM/FLASH ne persiste pas un candidat avant succès. QR Wi-Fi standard :
https://github.com/zxing/zxing/wiki/Barcode-Contents#wifi-network-config-android-ios-11


Note SDK relevée par le parent pendant l’implémentation : le `DNSServer.cpp`
d’Arduino-ESP32 2.0.17 copie 12 octets et parcourt les labels sans borne avant de
vérifier la longueur du datagramme. Pour ce portail, préférer une réponse DNS
minimale bornée via WiFiUDP (requête A/IN unique, taille et labels contrôlés,
compression/paquets tronqués rejetés), traitée une fois par tour du worker.
Test ciblé de requête valide et datagrammes courts/tronqués ; ne pas faire
entrer cette lecture SDK non bornée dans le nouveau chemin de réception.


- Revue additionnelle de l’assistant sans application :16constats classés,11groupes
  de corrections locales terminés. Les trois reviewers confirment la résolution
  de leurs constats retenus ; deux hypothèses matérielles sont consignées pour
  mesure, deux demandes non retenues expliquées dans le journal de triage.
- Audit parent du delta corrigé complet (58518octets) : états de reprise,
  annulation/commit, Host/origine, UTF8, radio pendant PRG, prévalidation des
  noms et historique incomplet ; tests du vrai worker et réponses HTTP relus.
  Chaque ligne de la matrice initiale reste couverte par des tests exécutés.
- Vérification finale du parent après tous correctifs : quatre compilations,
  modèle/journal/DNS/API/worker/OLED/PRG, radio, lux/veille et deux navigateurs
  desktop/mobile : tous PASS. Manifeste source/binaire et logs :
  `diagnostics/plant-setup-verification-manifest-20261104.json`.
  Application881504octets, SHA2564c2277e00206dda021399c745a636c615170c1bb2ba1e693b123bccbe2bd6a5d.
  Tests matériels de l’assistant et QR encore en attente ; statut in-review
  conservé jusqu’à leur vérification, aucun commit final prématuré.

- Installation réelle sans application réussie sur le même Heltec COM5/MAC
  confirmé, application seule à0x10000 ; table de partitions inchangée.
  Preuves : diagnostics/plant-setup-installation-20261104.json/.log.
- Premier redémarrage réel :24mesures lux valides, OLED OK, historique ready,
  LoRa listening, aucune erreur fatale. Réseau PLANT-EBF528 réellement observé
  WPA2 après WlanScan Windows explicite ; la première liste netsh était en cache,
  sans défaut du firmware ni nouvelle modification. QR physique et assistant
  sur téléphone, URL LAN/API et persistance après redémarrage restent à valider.


- Nouvelle évolution utilisateur après essai réel du portail : demande d’une
  petite app utilisant le Wi-Fi déjà connecté au téléphone, puis confirmation
  Android. Le parcours principal devient Android/Bluetooth ; le portail reste
  un secours. Travail suivi dans spec-plant-android-setup.md, sans changer les
  quatre sondes, l’historique, les noms ou le calibrage différé.
