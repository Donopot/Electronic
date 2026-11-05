# Assistant Wi-Fi local : vérification logicielle

Évolution ciblée du profil `heltec_v3_plant_dashboard` : remplacer l’onboarding
BLE par un assistant sans application, conserver les modèles, noms, historique,
correctifs de revue et profils historiques. Aucun accès série ni flash par
l’implémenteur ; les essais physiques restent au parent.

## Code livré

- SoftAP WPA2 à clé propre au kit, générée et relue en NVS. Configuration AP
  protégée posée pendant l’arrêt du pilote, avant émission. Fermeture par
  passage direct en STA, sans vider une configuration AP qui émet encore.
- Scan asynchrone, saisie manuelle, essai de 25 s maximum en RAM, rollback sur
  erreur ou annulation. FLASH sélectionnée uniquement après GOT_IP et contrôle
  du réseau connecté ; retour immédiat en RAM. SSID32 et PSK64 passent par
  des buffers C terminés de33/65 octets.
- API de configuration limitée au côté AP et au Host/origine canonique. Corps
  de512 octets pour ces seuls endpoints ; API de suivi toujours limitée à256.
  Aucun mot de passe dans les réponses ou les journaux publics.
- Portail captif et DNS via WiFiUDP : parser A/IN borné, compression et paquets
  tronqués rejetés, un datagramme par tour. DNSServer du SDK n’est pas utilisé.
- QR Wi-Fi adaptatif à quatre modules de marge calme, clé lisible sur deux
  lignes, QR d’URL de secours ; QR LAN après réussite. PRG8s puis relâchement
  ouvre l’assistant, PRG3s puis relâchement conserve la veille.
- Page mobile/ordinateur autonome et guide cadeau. Formulaire sans vocabulaire
  technique de stockage/PSK ; les limites restent dans la documentation technique.

## Résultats exécutés

| Vérification | Résultat / journal |
| --- | --- |
| Quatre profils Heltec, compilation finale incrémentale | PASS — [builds](plant-setup-builds-20261104.log) |
| Modèle/journal/API + vrai worker/HTTP avec SDK simulé | PASS — [host](plant-setup-host-tests-20261104.log) |
| Radio, dont quatre émetteurs simulés | PASS — [radio](plant-setup-soil-tests-20261104.log) |
| Lux/OLED/veille et profils historiques | PASS — [lux/veille](plant-setup-lux-sleep-tests-20261104.log) |
| JavaScript réel de l’assistant sous Edge mobile/ordinateur | PASS — [assistant](plant-setup-browser-tests-20261104.log) |
| JavaScript réel du tableau de bord préservé | PASS — [suivi](plant-setup-dashboard-browser-tests-20261104.log) |

Le test worker vérifie protection avant émission, secret persistant, absence
de commit avant IP, conservation de l’ancien réseau en panne, mauvais mot de
passe/timeout, annulation et échec de commit, Host/origine/interface, scan,
limites SSID32/PSK64, fermeture explicite/5min et réouverture demandée. Les
tests DNS couvrent les troncatures à chaque longueur, labels invalides,
compression, type non pris en charge et bornes OPT/capacité. Le test du vrai
main/OLED exerce 3s/8s, relâchement, absence OLED, QR29/33 avec clé lisible et
secours URL ; le tour retardé franchissant directement8s est aussi couvert.

Les tests navigateur utilisent exclusivement des API **simulées**. Ils
vérifient scan, SSID spéciaux rendus comme texte, saisie manuelle, reprise après
mot de passe refusé, SSID32/PSK64, annulation, remise du lien LAN après réussite,
fermeture et délai/reprise sur fetch suspendu. Aucune ressource distante ni
erreur JavaScript. Captures inspectées par l’implémenteur et le parent :
`output/plant-dashboard-tests/setup-simulation-390.png` et
`output/plant-dashboard-tests/setup-simulation-1280.png` ; ce sont des simulations.

Runtime navigateur :
`NODE_PATH=C:/Users/Anwender/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules`.
Le seul avertissement host est celui du tableau de taille variable du Toolkit
SparkFun. `git diff --check` est propre.

## Preuves restant matérielles

Revue finale du parent, installation application seule, première connexion
SoftAP depuis un téléphone, lisibilité/décodage des deux QR sur l’OLED réel,
portail captif selon le téléphone, candidat mauvais puis correct, conservation
des données après redémarrage et réception pendant HTTP. Ce rapport ne prétend
pas que ces essais aient été effectués.

## Correctifs de seconde revue

Les onze demandes ciblées sont corrigées : formulaire rouvert après perte STA
ou demande PRG8s pendant la période de succès, échéances annulées, annulation
tardive refusée, commit FLASH réussi conservé même si le retour RAM échoue,
prochain candidat bloqué tant que RAM ne peut être sélectionnée, Host LAN limité
à l’IP courante/nom annoncé, scan UTF-8 valide, réception/publication radio pendant
les invites, validation préalable des quatre noms et avertissement de pagination
incomplète conservé au redimensionnement.

Le stub de tâche capture maintenant la fonction réellement enregistrée et
exécute `worker()` avec un nombre d’itérations borné. Le scénario soumet le
candidat par HTTP, injecte GOT_IP, constate le commit, soumet un nom, attend sa
persistance puis relance le vrai worker pour vérifier sa restauration. Aucun
appel manuel à `serviceNetwork` dans ce scénario. Le runner existant l’exécute
avec l’argument `worker-loop`. Les tests HTTP d’historique vérifient aussi les
réponses réelles datées/non datées, tension numérique ou null ADC et deux pages
de 96 puis 2 relevés, en utilisant le curseur renvoyé.

Vérifications après ces correctifs, limitées aux fichiers concernés :

- PASS worker (helpers de production + tâche réelle bornée) et intégration
  main/radio/OLED avec et sans OLED :
  [journal ciblé host](plant-setup-review-fixes-host-20261104.log).
- PASS page réelle de suivi sous Edge mobile/ordinateur : dernier nom invalide
  sans aucun POST, échec de deuxième page puis resize, rechargement réussi,
  scénarios précédents de cette page conservés :
  [journal ciblé navigateur](plant-setup-review-fixes-browser-20261104.log).
- `git diff --check` propre. Avertissement tiers VLA Clang du Toolkit SparkFun.

Aucun nouveau fichier de test ni stub : les tests et stubs existants sont
étendus (`test_dashboard_worker.cpp`, `test_dashboard_orchestration.cpp`,
`DashboardSdk.h`, `freertos/task.h`, test navigateur et runner). Les compilations
et autres suites du tableau précédent précèdent ces correctifs ; la vérification
complète et les essais matériels sont laissés au parent selon sa demande.


## Vérification finale du parent et installation

Les trois reviewers confirment leurs constats retenus résolus. Le parent a
relu le delta corrigé et exécuté les quatre builds, suites dashboard/worker,
radio, lux/veille et les deux suites navigateur : tous PASS. Les journaux
`plant-setup-final-*-20261104.log` et le manifeste
[de provenance](plant-setup-verification-manifest-20261104.json) correspondent
au binaire final installé,881504octets, SHA256
`4c2277e00206dda021399c745a636c615170c1bb2ba1e693b123bccbe2bd6a5d`.

Installation application seule et table préservée :
[preuve](plant-setup-installation-20261104.json).
Premier démarrage :24lux valides, historique ready, OLED OK, LoRa listening,
aucune erreur fatale : [preuve](plant-setup-first-boot-20261104.json).
Le réseau PLANT-EBF528 et WPA2 sont réellement observés après actualisation
explicite WlanScan du cache Windows : [preuve](plant-setup-wlan-beacon-20261104.json).
Le réseau du PC n’a pas été changé.

La lecture du QR au téléphone et la configuration Wi-Fi restent à confirmer
par l’utilisateur, puis les essais LAN et de persistance physique pourront
être terminés. Cette étape ne constitue pas encore une validation matérielle
complète ni un mois de fonctionnement réel.
