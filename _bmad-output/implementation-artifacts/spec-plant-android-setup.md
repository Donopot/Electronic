---
title: 'Plant Station : configuration Android depuis le Wi-Fi du téléphone'
type: 'feature'
created: '2026-11-04'
status: 'in-review'
route: 'dispatch'
baseline_commit: '417908321bd610b6f82d0fe941eb1805d64cd5ca'
review_loop_iteration: 0
context:
  - 'docs/plant-station/android-setup-design.md'
  - 'docs/plant-dashboard.md'
---

<frozen-after-approval reason="évolution explicitement demandée par l’utilisateur, qui confirme Android">
## Intent

Une petite app Android configure le kit cadeau avec le Wi-Fi auquel le
téléphone est déjà connecté. Préremplir son nom si Android l’autorise ; le
mot de passe reste saisi une fois. Le QR associe le bon Heltec. Le téléphone
garde son Wi-Fi pendant l’envoi sécurisé par Bluetooth, puis ouvre le tableau
de bord local existant. Cette demande remplace le choix précédent sans app.

## Boundaries & Constraints

Garder quatre sondes, leurs programmes, la radio, les noms, l’historique30jours,
l’OLED et les boutons. Calibrage différé et batterie inconnue inchangés.
Conserver le portail web comme secours. Pas de compte, publication Internet,
télémétrie, lecture promise du mot de passe Android, ni cryptographie maison.
Garder les anciens identifiants jusqu’à GOT_IP et sauvegarde réussie du nouveau.

## I/O & Edge-Case Matrix

| Situation | Résultat |
| --- | --- |
| SSID accessible | Nom courant prérempli, éditable ; pas de scan Heltec requis |
| SSID masqué/permission refusée | Saisie manuelle, reste du parcours accessible |
| QR valide et Bluetooth autorisé | Association au kit et session Security1 authentifiée |
| Mauvais QR/code, Security0, perte BLE | Erreur claire, reprise ; aucun mot de passe transmis sans session sûre |
| Mot de passe Wi-Fi faux | Essai borné, ancien réseau conservé, correction possible |
| Connexion et sauvegarde réussies | Confirmation et accès au site local ; pas de faux succès à l’envoi |
| PRG8s / redémarrage | Réassociation possible ; noms/historique conservés |
| BLE/HTTP occupés | Radio et lux continuent ; commandes et mémoire bornées |
</frozen-after-approval>

## Code Map

- Nouveau projet `android/PlantStation/` : app Java native, ressources, tests,
  Gradle reproductible, APK signé localement ; détails dans le contexte.
- `firmware/PlantStationVeml7700/src/web/PlantDashboard.cpp` possède le worker,
  l’essai RAM, commit FLASH après IP, rollback et files. Les réutiliser.
- Ajouter `src/web/PlantBleSetup.*` : adaptateur protocomm/queues, sans accès
  concurrent direct aux Strings/NVS du worker. Modifier QR/OLED et réseau
  seulement pour l’association Android et le secours web.
- `test/dashboard-support/`, tests dashboard/worker et runner : étendre les
  frontières réelles ; préserver leurs assertions antérieures.

## Tasks & Acceptance

- [x] Définir un contrat BLE borné et utiliser Security1 Espressif avec secret
  propre au kit ; refuser tout déclassement, conserver la transaction Wi-Fi.
- [x] Construire l’app : permissions au moment utile, SSID courant/fallback,
  QR, Bluetooth, mot de passe, progression/reprise et ouverture du site local.
- [x] Intégrer démarrage/réouverture/arrêt BLE et QR lisible, secours web,
  radio active et arrêt propre avant veille.
- [x] Compiler APK et quatre profils Heltec ; tests de contrats, permissions,
  refus/timeout/reprise, vraie boucle worker et suites existantes pertinentes.
- [ ] Guide cadeau, preuves distinctes simulées/réelles ; essais physiques du
  parent après revue, aucun flash ni accès série par l’implémenteur.

Étant donné un Android connecté à la maison, quand le kit est associé et le
mot de passe fourni, alors le Heltec rejoint ce réseau sans changement de
Wi-Fi du téléphone et n’annonce la réussite qu’après persistance.
Étant donné un refus ou une interruption, quand l’utilisateur réessaie,
alors il garde une voie accessible et aucune ancienne configuration n’est perdue.

## Implementation Notes

Décision utilisateur Android explicite, pas de nouvelle demande d’approbation
générale. Le parent prépare l’outillage et effectue installations/matériel.
Ne pas invoquer de workflow imbriqué. Préserver les modifications antérieures,
les deux rapports historiques modifiés et les fichiers non suivis préexistants.
Source de vérité : ce spec et son contexte ; aucun autre agent ne modifie le
code pendant l’implémentation. Vérification/revue puis essai téléphone requis.

## Review Triage Log

Revue du 2026-11-05, diff complet `plant-android-review-be80c48000914827a1c55c8343b33cca.diff`.
Chaque constat est conservé séparément ; les causes communes seront corrigées ensemble.

| ID | Verdict / route | Preuve et correction attendue |
| --- | --- | --- |
| B1 | medium / patch | Le favori Android ne conserve que l'IP DHCP ; le nom stable connu doit offrir un accès lorsque cette IP change. |
| B2 | medium / patch | Après interruption, la session authentifiée ouvre toujours le formulaire ; exploiter le statut existant pour récupérer une réussite déjà terminée, sans annoncer un nouvel essai réussi. |
| B3 | medium / patch | Le scanner peut devenir null après la vérification initiale Bluetooth ; contrôler sa disponibilité au démarrage réel de la recherche. |
| B4 | medium / patch | Back recrée indéfiniment l'accueil ; terminer l'Activity depuis sa racine, nettoyer la session depuis les autres vues. |
| B5 | medium / patch | Les labels ne sont pas associés aux champs ; ajouter des identifiants et labelFor pour la lecture accessible. |
| B6 | low / patch | isKitWifi interdit aussi PLANT-HOME ; reconnaître uniquement le format réel du kit. |
| B7 | low / patch | Le préfixe 0x est rejeté après retrait des guillemets ; un vrai SSID cité 0xKitchen doit rester lisible. |
| B8 | medium / patch | BLE reste actif si l'AP échoue ; son délai de fermeture après réussite dépend actuellement uniquement du drapeau AP. |
| B9 | low / patch | Les copies HTTP dans FormFields et request ne sont pas effacées explicitement ; appliquer le nettoyage déjà utilisé pour les candidats BLE. |
| B10 | medium / patch | Les tests Activity initiaux n'exercent pas connexion/session/poll ni API31 ; compléter les frontières réelles et les callbacks après annulation. |
| E1 | medium / patch | Même crash reproductible que B3 lorsque Bluetooth est coupé pendant le QR ; garde au point d'usage. |
| E2 | high / patch | Une admission HTTP conserve bleAttempt ; sa réussite ultérieure peut être publiée sous l'ancien id BLE. Retirer cette corrélation dès admission HTTP. |
| E3 | medium / patch | Même fermeture manquante que B8 après réussite BLE avec AP indisponible ; vérifier le délai indépendant. |
| E4 | low / patch | Même exclusion injustifiée que B6 des réseaux domestiques préfixés PLANT-. |
| E5 | low / patch | L'audit accepte --apk extérieur mais relative_to(root) échoue lors du rapport ; conserver un chemin absolu dans ce cas. |
| V1 | medium / patch | Écart de vérification pré-vérifié : tester le vrai poll Activity, refus des états queued/testing/id périmé et persistance uniquement après connected corrélé. |
| V2 | medium / patch | Écart de vérification pré-vérifié : exercer le vrai événement/session, refus sec_ver/no_pop et absence de formulaire avant session valide. |
| V3 | medium / patch | Écart de vérification pré-vérifié : lecture SSID réussie sur ancienne API et callback API31+, jusqu'au champ affiché. |
| P1 | medium / patch | Les callbacks submit effacent pendingSecret avant de vérifier leur génération ; un ancien callback peut effacer le nouvel envoi. Inverser l'ordre et tester le vrai callback. |
| P2 | medium / patch | Le passage manuel laisse une lecture SSID active avec la même génération ; sa réponse peut écraser le réseau choisi sur l'écran suivant. Annuler cette lecture lors du changement d'écran. |

Confirmation des corrections : E1–E5 et V1–V3 résolus par les relecteurs ;
B1/B3–B9 et P1/P2 vérifiés dans les sources corrigées. Deux restes de la même
revue sont traités avant installation :

| ID | Verdict / route | Preuve et correction attendue |
| --- | --- | --- |
| B2-reste | medium / patch | PRG8 efface le snapshot d'essai après fermeture ; une connexion actuelle identique aux identifiants sauvegardés doit rester consultable en lecture seule, avec id0 distinct d'une nouvelle tentative. Vérifier aussi redémarrage et refus d'une connexion candidate non sauvegardée. |
| B10-reste | medium / patch | Les branches de demande/résultat BLUETOOTH_SCAN et CONNECT sur API31 ne sont pas encore exercées ; ajouter un test Activity avec refus puis autorisation et passage à la permission caméra, sans radio réelle. |

## Verification

Tests Java/Android exécutables et assemblage release ; contrat BLE et worker
réels avec I/O SDK simulées ; quatre builds et suites dashboard/radio/lux.
Le parent vérifie le binaire installé, association Android réelle, mot de passe
faux puis correct, conservation du Wi-Fi du téléphone, LAN et redémarrage.

### Audit de la matrice avant revue — 2026-11-05

Les dix tests Android, la vraie boucle worker avec adaptateur BLE et I/O SDK
simulées, quatre compilations et suites existantes passent. Chaque ligne de
la matrice a une couverture exécutée : SSID/fallback et permissions refusées
(JUnit + vraie Activity Robolectric), QR/downgrade/code et callbacks périmés
(contrats), mauvais réseau puis réessai/GOT_IP/commit et contention HTTP/BLE
(worker), réouverture/arrêt et radio/lux maintenus (worker/orchestration).
L'authentification cryptographique et les lectures optiques/matérielles restent
à vérifier physiquement après revue. Guides et preuves logicielles sont prêts :
[rapport](../../diagnostics/plant-android-software-20261105.md),
[manifeste](../../diagnostics/plant-android-verification-manifest-20261105.json).
Le dernier item reste ouvert jusqu'aux essais réels ; aucun résultat matériel
Android n'est déduit des simulations.
