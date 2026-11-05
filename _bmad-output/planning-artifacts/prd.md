---
project: Electronic
date: 2026-10-27
status: baseline-and-proposed-backlog
inputDocuments:
  - docs/project-overview.md
  - README.md
  - WEB.md
  - firmware/WeatherReceiver/README.md
  - firmware/PlantHubDiagnostic/README.md
---

# PRD : consolider Electronic

## Problème et périmètre

Le prototype fonctionne, mais les informations sont dispersées entre code,
comptes rendus et diagnostics historiques. Il manque une référence de
validation facilement reproductible et une mesure durable de la fiabilité LoRa.

L'utilisateur doit pouvoir consulter une mesure récente, comprendre une perte
de liaison et configurer le Wi-Fi local. Le mainteneur doit pouvoir modifier
les firmwares en conservant les comportements validés.

Cette itération organise la connaissance et propose une consolidation de la
station météo. Elle conserve le diagnostic Heltec comme cible de régression.
Application native, cloud, OTA, arrosage et nouveau firmware XIAO sont hors
périmètre de cette itération ; ils demandent un cadrage distinct.

## Exigences fonctionnelles existantes à conserver

| ID | Exigence | Source vérifiable |
| --- | --- | --- |
| FR1 | La sonde acquiert température et pression à cadence nominale 2 s et rejette les mesures invalides | Sketch, `Bmp180Compensation.h` |
| FR2 | Le profil TX transmet uniquement une mesure valide, au plus toutes les 120 s, en JSON v1 `meteo-1` | `WeatherLoRaTx.cpp`, `WeatherRadioConfig.h` |
| FR3 | Le récepteur vérifie version, nœud, types, valeurs finies et plages −40..85 °C / 300..1100 hPa avant publication | `WeatherReceiver/src/main.cpp` |
| FR4 | API et page masquent les mesures distantes dès 180 000 ms ; le profil local utilise 7 000 ms | `WebDashboard.cpp`, `.h`, configuration RX |
| FR5 | Le Wi-Fi se configure par BLE, avec reprise et identifiant stable ; les API ne publient aucun secret de provisioning | `WebDashboard.cpp`, `WEB.md` |
| FR6 | Le récepteur expose état radio et compteurs sans présenter une erreur CRC comme mesure météo | `WeatherReceiver/src/main.cpp` |
| FR7 | Les profils BMP180/LCD, TX, RX et Heltec gardent leurs options et brochages propres | Trois configurations PlatformIO |
| FR8 | Heltec conserve PRG long puis relâchement pour la veille et RST pour reprendre RX | `PlantHubDiagnostic/src/main.cpp` |

FR1–FR8 sont une référence de non-régression, pas huit nouvelles fonctions à développer.

## Objectifs de consolidation proposés

| ID | Résultat attendu | Indicateur |
| --- | --- | --- |
| FR9 | Un développeur retrouve état, exigences, architecture et travail restant | Liens du guide BMAD et suivi cohérents |
| FR10 | Une commande contrôle les tests existants, et une option compile les trois cibles | Code de sortie non nul au premier échec |
| FR11 | Le rejet des paquets incorrects est couvert par des cas déterministes | Matrice du parseur et exécution des cas |
| FR12 | Un rapport de campagne distingue paquets uniques, manquants, redémarrages et lacunes d'observation | Analyse reproductible sur fichiers, sans accès matériel |
| FR13 | Une campagne réelle mesure les réceptions et l'âge des données au placement retenu | Journal TX/RX horodaté et rapport lié |

## Exigences non fonctionnelles

- **NFR1 — Exactitude :** aucune valeur périmée/invalide affichée comme actuelle ;
  garder l'âge, la source et les erreurs explicites.
- **NFR2 — Reproductibilité :** conserver versions de plateforme et bibliothèques,
  commandes, résultat et limites de chaque validation.
- **NFR3 — Confidentialité :** aucun mot de passe, QR ou code de possession dans
  les livrables ; HTTP existant réservé au réseau local de confiance.
- **NFR4 — Continuité :** maintenir acquisition, boucle web et reprise radio ;
  l'adoption BMAD n'introduit aucune modification du firmware installé.
- **NFR5 — Preuves :** séparer tests logiciels et observations matérielles ;
  garder les fichiers sources de chaque résultat.

## Critères proposés pour la campagne radio

Prévoir au moins 60 minutes et 30 émissions observées, sur batterie, au même
placement déclaré que la référence et avec paramètres constants. Objectif
initial proposé : au moins 95 % de paquets uniques valides parmi les émissions
TX effectivement observées, et aucun défaut SPI persistant non récupéré.
Calculer aussi le nombre et la durée des passages à l'état périmé.

Ces seuils servent à construire le rapport ; ils ne constituent pas une garantie
de portée ni une exigence déjà confirmée par l'utilisateur. Une absence de
journal TX empêche de conclure à un taux de livraison exact. Les interruptions
de collecte sont signalées séparément.

## Questions réservées à l'évolution produit

- Quelle autonomie et quelle fiabilité sont nécessaires au quotidien ?
- Quels capteurs, quel modèle XIAO et quels usages pour Plant Station ?
- La station doit-elle dépasser le réseau domestique ou servir plusieurs utilisateurs ?

Ces réponses ne conditionnent pas l'adoption actuelle. Elles conditionnent
la spécification d'un nouveau produit et les objectifs de livraison définitifs.
