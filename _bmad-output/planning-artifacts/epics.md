---
project: Electronic
date: 2026-10-27
status: technically-ready-proposed-order
inputDocuments:
  - _bmad-output/planning-artifacts/prd.md
  - _bmad-output/planning-artifacts/architecture.md
  - _bmad-output/planning-artifacts/test-strategy.md
---

# Electronic : epics et stories

Ordre proposé, sans engagement de date. Les fonctions déjà présentes sont une
référence de non-régression. Les intitulés `Epic` et `Story` sont conservés
pour le parseur officiel BMAD. Six stories ; aucun travail matériel déclaré terminé.

## Couverture des exigences

| Exigences | Stories et responsabilité |
| --- | --- |
| FR1–FR8, NFR1–NFR5 | 1.1 inventaire ; 1.2 vérifications de référence |
| FR3, FR11, NFR1 | 1.3 tests dédiés du parseur |
| FR9, NFR2, NFR5 | 1.1 adoption et suivi |
| FR10, NFR2 | 1.2 commande de vérification |
| FR12, NFR5 | 2.1 analyse de campagne |
| FR13, NFR1, NFR4 | 2.2 campagne et bilan ; 2.3 reprise après panne |

## Epic 1: Maintenir une station avec une reference verifiable

Valeur : le mainteneur retrouve les décisions et détecte les régressions.

### Story 1.1: Socle BMAD et reference verifiee

En tant que mainteneur, je veux une référence de projet et un suivi BMAD pour
reprendre le développement à partir de faits.

**Exigences :** FR9 ; référence FR1–FR8 ; NFR2, NFR3, NFR5.
**Dépendances :** aucune. **État :** réalisé dans cette adoption.

**Critères d'acceptation :**

1. Étant donné le dépôt existant, l'installation officielle BMAD est identifiée
   par version, modules et intégration Codex, avec configuration française.
2. L'état, le PRD, l'architecture, les stories et le plan de test sont liés
   depuis un guide ; faits et objectifs proposés sont distingués.
3. Les tests existants et les trois compilations sont exécutés ; résultats
   et limites sont enregistrés sans secrets ni validation matérielle inventée.
4. Le suivi est généré et validé par le script BMAD ; les autres stories
   ne sont pas présentées comme terminées.

### Story 1.2: Commande de verification reproductible

En tant que mainteneur, je veux lancer les contrôles avec une commande pour
éviter les oublis et obtenir un échec explicite.

**Exigences :** FR10 ; non-régression FR1–FR8 ; NFR2, NFR3.
**Dépendances :** aucune ; réutilise les commandes de la story 1.1.

**Critères d'acceptation :**

1. Un script `tools/check_project.ps1` exécute le test JavaScript et compile
   les deux fichiers C++ d'assertions avec C++14 et warnings en erreurs.
2. Une option `-BuildFirmware` ajoute séquentiellement TX web, RX et Heltec,
   avec les paramètres PlatformIO locaux documentés dans la stratégie.
3. Un outil absent ou un code non nul arrête la commande et indique l'étape ;
   le succès des trois tests et, si demandé, des trois builds est explicite.
4. La commande restaure dossier de travail et variables d'environnement ;
   elle ne téléverse rien et n'ouvre aucun port série.
5. README et guide donnent une invocation vérifiée sur ce poste.

### Story 1.3: Validation des paquets meteo testable

En tant qu'utilisateur, je veux que les paquets incorrects soient rejetés
de manière vérifiée pour ne pas consulter de fausses mesures.

**Exigences :** FR3, FR11, NFR1, NFR4.
**Dépendances :** aucune ; suit ADR3 et la matrice du plan de test.

**Critères d'acceptation :**

1. La validation actuelle est isolée sans changement de contrat, avec longueur
   d'entrée explicite et résultat consommé uniquement après succès.
2. Les cas valides, champs manquants, mauvais types, version/nœud incorrects,
   JSON tronqué, valeurs non finies et limites de plages sont testés.
3. Les bornes numériques `seq` et `uptime_ms` sont vérifiées sans introduire
   une politique de déduplication ou d'anti-rejeu.
4. Les tests exécutent le parseur réellement utilisé par le RX, avec
   ArduinoJson 6.21.5 ; les contrôles UI/radio existants et le build RX passent.
5. Aucun paquet rejeté ne publie de mesure ou ne rafraîchit l'âge du capteur.

## Epic 2: Connaitre la fiabilite reelle de la liaison meteo

Valeur : l'utilisateur comprend ce qui a été reçu, perdu ou non mesuré.
Les outils analysent le contrat existant ; cet epic ne dépend pas de l'epic 1.

### Story 2.1: Rapport de campagne depuis les captures

En tant que mainteneur, je veux analyser les captures hors ligne pour
quantifier la livraison sans confondre une absence de collecte avec une perte radio.

**Exigences :** FR12, NFR2, NFR5. **Dépendances :** aucune.

**Critères d'acceptation :**

1. Un outil Python accepte deux fichiers JSONL TX/RX normalisés :
   `time_utc` ISO8601, `node`, `seq`, `uptime_ms`, `valid` ; les événements
   RX valides ajoutent `temperature_c`, `pressure_hpa`, `rssi_dbm`, `snr_db`.
   Un événement TX porte `tx_done` ; seules les émissions confirmées servent
   au dénominateur. Les erreurs de format sont rapportées avec numéro de ligne.
2. Le rapport donne émissions confirmées, réceptions valides uniques,
   doublons, séquences manquantes, sessions et intervalle de collecte.
3. Sans preuve TX correspondante, le taux exact est marqué indisponible.
   Reset, wrap/ordre ambigu et interruption déclarée de collecte restent explicites.
4. Cas déterministes : série complète, manque, doublon, reset, collecte
   interrompue et absence de fichier TX ; résultats attendus documentés.
5. Le traitement est hors ligne et conserve les sources ; un adaptateur de
   capture décrit comment transformer les JSON série existants dans ce format.

### Story 2.2: Campagne radio au placement de reference

En tant qu'utilisateur, je veux une observation prolongée sur batterie
pour connaître les limites de la station à mon emplacement.

**Exigences :** FR13, NFR1, NFR5. **Dépendance :** story 2.1.
**Prérequis externe :** sonde et récepteur disponibles, placement confirmé,
capture TX sans alimenter la sonde par USB ou modification documentée du protocole
de mesure si cette observation n'est pas possible. En l'absence de TX, publier
des observations RX sans taux exact. Aucun résultat n'est présumé.

**Critères d'acceptation :**

1. Au moins 60 minutes et 30 émissions sont observées, ou l'impossibilité de
   compléter la campagne est constatée avec sa cause, story laissée ouverte.
2. Placement, batterie, orientation et paramètres radio sont consignés.
3. RSSI/SNR, CRC, santé SPI, réceptions et périodes de données périmées
   sont capturés et analysés sans changement de puissance ou cadence.
4. Le rapport compare l'objectif proposé de 95 % au taux effectivement
   calculable et relève les défauts SPI ; un objectif manqué produit un écart
   explicite et une action, sans effacer les résultats.
5. L'écart de configuration ou l'absence d'émissions observables interdit
   une comparaison présentée comme équivalente à la référence.

### Story 2.3: Reprise et peremption en situation reelle

En tant qu'utilisateur, je veux comprendre une interruption puis revoir
les nouvelles mesures quand la liaison revient.

**Exigences :** FR4–FR6, FR13, NFR1, NFR3, NFR4.
**Dépendances :** aucune ; matériel et fenêtre d'interruption disponibles.

**Critères d'acceptation :**

1. Lors d'un arrêt contrôlé de sonde, l'API masque les valeurs à 180 000 ms ;
   la page le fait au premier rafraîchissement suivant, nominalement sous 2 s.
2. Au retour de la sonde, la première mesure valide rétablit l'affichage et
   un âge récent ; l'ancienne valeur reste masquée jusqu'à cette réception.
   Ce contrôle n'ajoute pas de garantie d'anti-rejeu au protocole existant.
3. Une indisponibilité HTTP produit l'état réseau inaccessible ; une absence
   LoRa laisse le Wi-Fi du récepteur disponible.
4. Le résultat précise durée de coupure, timestamps et états observés ;
   aucune credential BLE ou Wi-Fi ne figure dans les captures publiées.

## Travaux différés, hors sprint

Fiabilité cible définitive, autonomie, qualification électrique LCD, courant
de veille Heltec, modèle/usage XIAO, nouveaux capteurs, authentification radio
et application multiappareils. Les besoins et moyens de mesure doivent être
précisés avant de les transformer en stories de réalisation.
