# Plant Station : vérification logicielle du 4 novembre 2026

Périmètre : nouveau profil `heltec_v3_plant_dashboard`, modèle commun OLED/API,
page réelle locale et batterie inconnue par sonde, journal brut sur la partition
existante. Aucun flash des XIAO. Les deux rapports historiques déjà modifiés ne
sont pas modifiés par cette implémentation.

## Résultats exécutés

- Les quatre profils `heltec_v3_text`, `heltec_v3_json`, `heltec_v3_soil_lora` et
  `heltec_v3_plant_dashboard` compilent. Le dernier journal ne contient aucune
  erreur ni avertissement de compilation : [compilations](plant-dashboard-builds-20261104.log).
- Les nouveaux tests portables vérifient le vrai modèle, les validateurs API et
  le journal : 31 jours simulés pour quatre nœuds, filtre glissant 30 jours,
  pagination, valeurs distinctes, trous, ADC, reboot, écriture interrompue,
  partition inconnue préservée, flash pleine avec rotation bornée, UTF-8, batterie
  inconnue et navigation/veille PRG. L’intégration exécute les vrais
  main/radio/OLED avec **travailleur réseau et encodeur QR simulés** : identité
  des quatre sondes, noms, lux, état hors réseau, limites OLED et marge QR,
  péremption et arrêt asynchrone avant veille, OLED présent puis absent.
  [Journal des tests](plant-dashboard-host-tests-20261104.log).
- Après détection par l’audit du parent d’un blocage réel de l’envoi HTTP
  (`availableForWrite()` hérité de Print renvoie toujours zéro dans le SDK
  installé), le transport utilise maintenant le descripteur public `fd()` et
  un seul `send(..., MSG_DONTWAIT)` par tour. Un test vérifie ce contrat dans les
  vrais en-têtes/sources SDK puis exécute le vrai helper de production avec
  syscall simulé : réponse complète après écritures partielles, EAGAIN,
  EWOULDBLOCK, EINTR, borne de 512 octets, fermeture et erreur définitive.
  Le nouveau profil est recompilé contre lwIP réel :
  [compilation après correction HTTP](plant-dashboard-http-fix-build-20261104.log).
  Aucun flash effectué.
- La suite radio existante passe pour le récepteur et les quatre émetteurs
  simulés : [tests radio](plant-dashboard-soil-tests-20261104.log).
- La suite existante lux/OLED/veille passe, avec vrai setup/loop, panne OLED,
  débordement d’uptime et attente d’initialisation radio :
  [tests lux/veille](plant-dashboard-lux-sleep-tests-20261104.log).
- Edge headless exécute le vrai JavaScript embarqué avec **API de test
  simulée**, en 1280×900 et 390×844 : quatre cartes, batterie inconnue, alertes
  mixtes, défauts coupant la courbe, périodes, historique vide, heure inconnue,
  réseau absent, noms UTF-8/injection rendue comme texte, clavier et dialogue,
  aucun débordement horizontal, axes SVG d’au moins 11 px effectifs et sidebar
  mobile en bloc, aucune ressource externe ou erreur JavaScript.
  [Journal navigateur](plant-dashboard-browser-tests-20261104.log).

Captures de cette simulation :
`output/plant-dashboard-tests/browser-simulation-1280.png` et
`output/plant-dashboard-tests/browser-simulation-390.png`. Elles ont été
inspectées visuellement. Le graphique mobile et les rangées de la sidebar ont
été corrigés après cette inspection, puis revérifiés.

Les tests host rapportent un avertissement du Toolkit SparkFun déjà présent
(tableau de taille variable, extension Clang en C++) ; aucun échec. `git diff --check` est propre sur les
modifications du firmware.

## Limites des preuves

Corrections de revue vérifiées séparément, sans compilation globale ni flash :

- [Tests ciblés dashboard](plant-dashboard-review-fixes-tests-20261104.log) :
  ancrages durables et restauration, filtres 1/7/30 j avant pagination,
  corruption des secteurs anciens comptée une seule fois, limites OLED à
  140000 lux, vrai `PlantDashboard.cpp` avec SDK simulé pour HTTP complet,
  Content-Length, envois partiels/EAGAIN, routes invalides, files et ingestion
  exactement une fois, sauvegarde/restauration/échec/arrêt Preferences,
  reconnexion sans effacement, DHCP et lifecycle BLE avec défauts init/start.
- [Tests navigateur ciblés](plant-dashboard-review-fixes-browser-20261104.log) :
  trois pages dépendantes des curseurs et point final tracé, historique de 6,6 s
  traversant les polls de statut de 5 s, annulation explicite par choix de sonde,
  requête suspendue puis expiration et reprise, retry d’horloge borné et reboot,
  dialogue de noms pending/failed/saved, caractère supplémentaire + 23 ASCII,
  rupture de courbe pour un cycle manquant de 60 minutes.

`NODE_PATH` utilisé pour Edge/Playwright :
`C:/Users/Anwender/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules`.
La compilation finale et la vérification complète après ces corrections restent
à exécuter par l’agent principal.

Ce rapport ne constitue pas un essai matériel : pas de preuve ici de BLE/Wi-Fi
réel, de décodage/scannage du QR par téléphone, de réception réelle pendant HTTP
lent ni de restauration d’historique sur flash physique. Ces contrôles, la
sauvegarde complète et l’installation application seule sont suivis séparément
par l’agent principal. Le test QR host contrôle la disposition et le contenu
d’URL attendu ; il ne teste pas l’encodeur Espressif.

L’horloge civile reste inconnue avant synchronisation par un téléphone. Des
relevés sans date d’un démarrage précédent restent conservés mais ne sont pas
positionnés arbitrairement sur les courbes. La conservation glissante est un
filtre de lecture des enregistrements datés ; le recyclage physique se fait par
secteurs. Les captures ne prouvent pas trente jours de fonctionnement réel.
