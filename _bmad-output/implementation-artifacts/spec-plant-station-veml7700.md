---
title: 'Plant Station — diagnostic de luminosité VEML7700 sur Heltec V3'
type: 'feature'
created: '2026-10-28'
status: 'done'
route: 'dispatch'
baseline_commit: 'NO_VCS'
review_loop_iteration: 0
context: []
---

<frozen-after-approval reason="demande détaillée de l'utilisateur et autorisation de réalisation autonome">

## Intent

Fournir dans Plant Station un firmware autonome, compilable et documenté pour mesurer la lumière avec le VEML7700 raccordé au hub Heltec. L'utilisateur demande explicitement de créer les fichiers, compiler et corriger sans étapes intermédiaires d'approbation.

## Boundaries & Constraints

Conserver VIN=3V3, GND commun, SDA=GPIO4, SCL=GPIO5, 3Vo libre. Cible `heltec_wifi_lora_32_V3`, plateforme Espressif32 6.12.0, Arduino, UART0 115200 via CP2102 avec CDC natif désactivé. Identité matérielle vérifiée sur COM5 : ESP32-S3 QFN56 révision 0.2, flash intégrée 8 Mo, MAC 3c:0f:02:eb:f5:28. La sous-révision physique V3.x reste non confirmée ; ne pas prétendre l'avoir identifiée par logiciel.

Créer `firmware/PlantStationVeml7700/` sans remplacer PlantHubDiagnostic ni les projets météo C3. Ne pas initialiser LoRa, Wi-Fi ou OLED. Sortie texte par défaut, JSON configurable avec type stable et lux null en erreur. Acquisition toutes les 1000 ms réglables, scanner au démarrage configurable, erreurs explicites, version, uptime, récupération après déconnexion. Pas de longues attentes dans loop.

Le livrable demandé comprend compilation, tests logiciels et instructions de flash/essais ; ne pas flasher automatiquement pour remplacer le diagnostic RX actuellement installé. Distinguer détection USB, compilation et essai capteur réel.

## I/O & Edge-Case Matrix

| État | Résultat attendu | Vérification |
|---|---|---|
| Capteur présent, configuration valide | Lux finis non négatifs à cadence réglée | Tests transport simulé et compilation cible |
| Obscurité, ALS=0 | Zéro lux valide | Test calcul |
| NACK, lecture courte ou bus défaillant | Erreur typée, aucune ancienne mesure publiée | Injection transport |
| Configuration perdue après coupure | Erreur puis reconfiguration périodique et attente de conversion | Injection transport et temps |
| Saturation 65535 | État saturation, lux null | Test borne |
| JSON valide / erreur | Même schéma, uptime entier, lux nombre ou null, status chaîne | Test sérialisation réelle |
| Débordement millis | Cadence et tentatives restent correctes | Test arithmétique temporelle |

</frozen-after-approval>

## Code Map

- `firmware/PlantHubDiagnostic/platformio.ini` et `short_paths.py` : configuration V3 et contournement des chemins Windows réutilisables ; ne pas modifier ce firmware RX.
- `.tools/platformio/platforms/espressif32/boards/heltec_wifi_lora_32_V3.json` : ESP32-S3, 8MB, QIO ; variante Arduino UART TX43/RX44. SDA/SCL par défaut 41/42 à remplacer explicitement.
- `docs/plant-station/plan-realisation-balcon-v1.md` : hub Heltec USB pour pots LoRa ; ce diagnostic est une étape de luminosité, pas le hub complet.
- `.tools/python` : PlatformIO et pyserial existants. Leur accès nécessite ici une exécution approuvée hors sandbox. `PLATFORMIO_CORE_DIR=.tools/platformio`.
- https://github.com/sparkfun/SparkFun_VEML7700_Arduino_Library : bibliothèque maintenue v2.0.1, `SparkFunVEML7700`, `getAmbientLight(uint16_t&)` et lectures/configuration avec `sfTkError_t`, réussite `ksfTkErrOk`. Épingler aussi Toolkit après vérification du manifest. `getLux` utilise ancien coefficient : calcul local justifié.
- https://www.vishay.com/docs/84323/designingveml7700.pdf : résolution 0.5376 lux/count à gain 1/8, intégration 100 ms ; correction polynomiale au-dessus de 1000 lux. Ne pas confondre ACK et identification certaine : aucun registre d'identité unique.
- https://heltec.org/project/wifi-lora-32-v3/ et https://s.heltec.cn/download/WiFi_LoRa_32_V3 : sources matérielles officielles. GPIO4/5 disponibles ; OLED sur bus séparé 17/18.

## Tasks & Acceptance

- [x] `firmware/PlantStationVeml7700/platformio.ini`, `short_paths.py` : fixer cible/dépendances et deux profils texte/JSON, compilation Windows.
- [x] `include/config.h` : centraliser broches, adresse, cadence, timeout et reprise, version, mode sortie ; vérifier les bornes à compilation.
- [x] `src/sensors/VEML7700Sensor.*` : transport à statut vérifié, gain 1/8 et 100 ms, économie désactivée, readback configuration, lecture contrôlée, délai première mesure non bloquant, reprise bornée avec reconfiguration et nouveau bus.
- [x] `src/diagnostics/Diagnostics.*` : scanner borné, logs français structurés, JSON uniquement en mode JSON (diagnostics inclus sous forme JSON), schéma constant des mesures et erreurs ; pas de nan/inf ni valeurs périmées.
- [x] `src/main.cpp` : orchestration légère sans réseau/radio/OLED, uptime et version, cadence résistante au wrap.
- [x] `test/` : tests exécutables du vrai code avec transport simulé couvrant toutes les lignes de la matrice, y compris récupération ; ne pas annoncer exécutés si seulement compilés.
- [x] `README.md` : contexte Plant Station, installation propre PlatformIO, build des deux profils, ports réels, upload/monitor, architecture, config JSON, dépannage, protocole manuel clair, limites gain fixe et suites possibles. Sources primaires liées, résultat logiciel séparé du matériel.
- [x] `diagnostics/plant-station-veml7700-20261028.md` : preuves exactes détection, builds et tests ; aucun succès matériel inventé.

Given le dépôt et PlatformIO, when les deux profils sont compilés, then les builds réussissent avec les versions fixées.
Given un capteur absent ou une lecture invalide, when une acquisition échoue, then le firmware publie une erreur sans ancien lux et retente sans redémarrage nécessaire.
Given le projet Plant Station existant, when ce diagnostic est ajouté, then les firmwares précédents restent disponibles sans modification.

## Implementation Notes

Livraison finale : correctifs de revue relus, deux profils reconstruits avec succès (RAM 18768 octets, flash 282361 octets chacun). Tests hôte relancés par l'agent principal : pilote réel, véritable setup/loop sur deux scénarios, 35 JSON contrôlés, chemins Windows C:/D: vérifiés. Tous les patchs retenus sont résolus ; aucun travail différé. Les essais physiques restent explicitement hors de cette livraison sans téléversement.

Audit principal : nouveaux fichiers relus et diff conservé dans le répertoire temporaire ; toutes les lignes de la matrice sont couvertes par les tests exécutés consignés. Deux profils compilés, code retour 0. Aucun flash réalisé.

Le dépôt n'a aucun commit : tous les fichiers existants sont non suivis. L'utilisateur autorise ce nouveau sous-projet autonome ; conserver les fichiers préexistants et ne pas créer de commit ni tout ajouter à l'index. Les choix de transport et de calcul peuvent être affinés si la bibliothèque réelle l'exige, en documentant la raison.

## Spec Change Log

## Review Triage Log

| ID | Verdict | Décision et preuve |
|---|---|---|
| Blind-1 | low | Rejet du changement de transport : Arduino 2.0.17 Wire.cpp 458–464 diffère le transfert sans STOP et requestFrom ne retourne que rxLength. NACK de lecture et timeout peuvent tous deux produire erreur_bus ; message et récupération restent corrects. Distinguer exigerait une autre API ou une transaction additionnelle. Limite à préciser dans la documentation. |
| Blind-2 | medium | patch : main met effectivement Status::Ok sur scan_ack avec lux initial nul ; Diagnostics le traite comme vraie mesure. Réserver les champs mesure à l'événement mesure et tester le scanner applicatif. |
| Blind-3 | medium | patch : os.path.relpath lève ValueError entre deux lecteurs Windows ; garder alors le chemin absolu. Le scénario est démontrable avec ntpath.relpath. |
| Blind-4 | low | patch : status() conserve Ok après acquisition saturée ; synchroniser l'état exposé avec le dernier résultat et vérifier le retour à Ok. |
| Blind-5 | false | Les seuls appelants applicatifs passent une mesure produite par le pilote, qui rejette lux>140000 avant formatage, ou un état sans mesure. Un double énorme fini n'atteint donc pas ce tampon dans le firmware ; ne pas ajouter de branche pour un état non démontré. |
| Blind-6 | medium | patch, même groupe que Verification-1 : le runner n'exécute pas main.cpp. Ajouter une couverture applicative avec temps injecté et sorties capturées. |
| Blind-7 | low | Rejet : les résultats successifs de service sont publiés dans les mesures toutes les secondes ; l'échec et le retour Ok figurent au journal. Un historique de chaque tentative nécessiterait un nouveau mécanisme de notification sans besoin utilisateur distinct. |
| Blind-8 | low | patch : le sentinelle adresse -1 devient bien 0xFFFFFFFF dans les événements globaux texte ; le rendre explicitement sans adresse. |
| Blind-9 | low | patch : placer la venv documentée dans .tools déjà ignoré pour éviter un environnement Python non suivi dans le dépôt. |
| Edge-1 | low | Même constat vérifié que Blind-1 ; Wire public ne conserve pas la cause de requestFrom=0. Récupération et absence de mesure fausse correctes, pas de changement de transport supplémentaire. |
| Verification-1 | medium | patch : preuve de lacune acceptée ; retirer lastSample=now ne serait détecté par aucun test existant. Vérifier échéance, répétition, pause et wrap dans la véritable boucle Arduino simulée. |
| Verification-2 | medium | patch : preuve de lacune acceptée ; permutation des chaînes NACK/config_perdue invisible aux assertions actuelles. Comparer la séquence exacte des statuts JSON attendus. |

## Verification

Compiler les profils texte et JSON avec PlatformIO depuis la racine. Exécuter des tests hôte avec un compilateur disponible ou un outil portable isolé (Zig est acceptable) pour le pilote et les diagnostics réels. Consigner sorties/versions et ressources de build. Les essais physiques de variation lumineuse, déconnexion/reconnexion et saturation restent manuels sans flash autorisé.
