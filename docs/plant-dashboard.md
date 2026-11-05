# Plant Station : tableau de bord local

Le profil `heltec_v3_plant_dashboard` ajoute le visage du robot, les détails des
quatre sondes et un QR d’accès au site hébergé sur le Heltec. Les trois profils
existants sont conservés. Aucune modification des XIAO ni du protocole PS v1.

## Configuration Android

Le parcours principal utilise l'application native Mes plantes : elle
préremplit le SSID courant si Android l'autorise, sinon garde sa saisie manuelle,
associe le kit par son QR Android/code et transmet le mot de passe saisi via
Bluetooth Security1. Le téléphone reste sur le Wi-Fi de la maison. La réussite
est confirmée après connexion et sauvegarde du kit ; le navigateur externe ouvre
ensuite ce même tableau de bord. Aucun mot de passe enregistré par Android
n'est lu. QR/codes et mots de passe ne sont ni journalisés ni persistés dans
l'app. Voir [le projet Android](../android/PlantStation/README.md), le
[contrat BLE](plant-station/android-ble-contract.md) et le [guide cadeau](plant-station/guide-cadeau.md).

Le premier QR pendant la configuration est maintenant le QR Android ; PRG
court affiche ensuite le QR Wi-Fi et le QR URL du portail de secours décrit
ci-dessous. PRG 8 s rouvre les deux voies sans effacer noms ni historique.

## Utilisation sans application

1. Sans réseau enregistré, le kit ouvre son Wi-Fi temporaire protégé
   `PLANT-<suffixe MAC>`. Un réseau connu indisponible pendant trente secondes
   ouvre aussi cet assistant, sans effacer ses identifiants et en poursuivant
   la reconnexion. Le mot de passe propre au kit est généré et conservé en NVS.
2. Après le QR Android, un appui court PRG présente le QR Wi-Fi. Le scanner propose de rejoindre le réseau du
   kit. Conserver cette connexion même si le téléphone annonce « pas d’Internet ».
   Si le QR est difficile à lire, rejoindre manuellement le nom affiché avec
   la clé présentée sur deux lignes de huit caractères (sans espace ajouté).
3. Ouvrir l’assistant qui apparaît automatiquement. Sinon, un appui court PRG
   après le QR Wi-Fi présente le QR de secours ; l’adresse est toujours
   `http://192.168.4.1/`. Il faut être connecté au Wi-Fi du kit pour l’ouvrir.
4. Choisir le Wi-Fi domestique 2,4 GHz dans le scan ou saisir son nom exact.
   Le candidat est testé en RAM pendant 25 secondes maximum. Une erreur ou une
   annulation rétablit l’ancien réseau, sans changement de sa sauvegarde.
5. Après connexion et sauvegarde confirmées, l’assistant affiche l’URL LAN et
   demande de rejoindre le Wi-Fi domestique. L’OLED présente le QR du tableau
   de bord. Le bouton de fermeture garde deux secondes pour envoyer sa réponse ;
   sinon le réseau temporaire ferme cinq minutes après la réussite. Il ne ferme
   pas automatiquement pendant un formulaire inachevé. Si la connexion réussie
   est ensuite perdue, le formulaire redevient éditable et la fermeture est annulée.

PRG court fait défiler trois écrans seulement : visage, détails, QR. Le QR est
unique et suit l’action courante — appairage Android (BLE) pendant la
configuration, puis Wi-Fi du kit, puis accès local `http://<ip>/`. La clé Wi-Fi
du kit et le code d’appairage restent affichés en texte à côté du symbole.
À trois secondes, relâcher demande la veille ; continuer
jusqu’à huit secondes puis relâcher annule cette demande et rouvre le formulaire
même si l’assistant est encore accessible après une réussite. La réception et la
publication radio continuent pendant les invites. Les profils historiques gardent leur comportement.
RST réveille le kit. La commande série `wifi-setup` reste un accès de diagnostic
facultatif ; le destinataire du cadeau n’a pas besoin d’ordinateur.

Le [guide cadeau](plant-station/guide-cadeau.md) explique ce parcours sans détails
techniques. Aucun compte, service externe ni publication Internet.

Les seuils restent indéfinis : les sondes fraîches sont **à étalonner**. Attente,
défaut ADC et péremption de 90 min sont des états distincts. Le modèle prépare
une qualification configurable pour un calibrage ultérieur sans la proposer
dans le formulaire de noms. Un état sec conserve en parallèle les avertissements
des autres sondes. La batterie est **non communiquée** (`B--` sur l’OLED,
`battery_percent:null` dans l’API), car PS v1 ne transporte pas cette mesure.

## Configuration Wi-Fi : frontières du SDK

Arduino-ESP32 2.0.17 n’applique `WiFi.persistent(false)` qu’à l’initialisation du
pilote. Le code appelle donc directement `esp_wifi_set_storage(WIFI_STORAGE_RAM)`
avant la configuration du point d’accès et de tout candidat. Il capture le
réseau sauvegardé avant ce passage, et ne sélectionne FLASH pour écrire le
candidat qu’après réception de GOT_IP et vérification du SSID connecté. Il
tente aussitôt de revenir en RAM. Un échec de ce retour ne change pas la réussite
déjà persistée ; tout prochain candidat exige une nouvelle sélection RAM réussie.
Les paramètres Wi-Fi restent séparés du journal et des noms.
Les appels C `WiFi.begin` utilisent des buffers terminés de 33/65 octets pour les
limites de 32 octets de SSID et 64 caractères hexadécimaux de PSK.

Le point d’accès est configuré en WPA2 pendant que le pilote est arrêté, avant
son redémarrage en AP+STA ; aucun AP par défaut ouvert n’est lancé. Sa fermeture
passe directement en STA, sans vider la configuration AP active. La clé du kit
reste sur l’OLED et en NVS ; elle n’est jamais renvoyée par les API ou les logs.
L’URL/hostname LAN restent journalisés sans identifiants pour le diagnostic.

Le DNS captif utilise WiFiUDP non bloquant, lié à l’adresse AP, et un parseur
local borné : une question A/IN, au plus 512 octets, labels contrôlés, pas de
compression, éventuel OPT EDNS vérifié. Un datagramme est traité par tour.
Le parseur DNSServer du SDK n’est pas utilisé. Les sondes de portail Android,
Apple et Windows sont redirigées vers l’adresse AP canonique.

Le QR Wi-Fi suit le [format ZXing](https://github.com/zxing/zxing/wiki/Barcode-Contents#wifi-network-config-android-ios-11).
La clé ASCII hexadécimale est entourée de guillemets dans le contenu. Le rendu
OLED adapte l’échelle à la taille du symbole et garde quatre modules de marge
calme. Le QR d’URL court garde deux pixels par module ; le QR Wi-Fi plus dense
utilise un pixel et dispose du secours manuel nom/clé et d’un second QR URL.
Le décodage au téléphone reste une vérification matérielle distincte.

## Horloge et historique

Le journal utilise directement la partition existante `spiffs` de 1,5 Mio, sans
monter ni formater un système de fichiers. Il vérifie l’intégralité de la
partition avant toute première écriture : seule une partition entièrement
`0xFF` est initialisée. Des données inconnues désactivent le stockage et sont
préservées ; les acquisitions continuent. La table de partitions 8 Mio reste
inchangée. Toujours sauvegarder la flash avant installation.

Un marqueur de propriété permanent précède un anneau de secteurs de 4 Kio.
Chaque secteur a une génération et un CRC ; chaque enregistrement de 40 octets
a son CRC. Un enregistrement interrompu est ignoré et son emplacement n’est pas
réécrit. L’anneau contient jusqu’à 38 683 enregistrements, au-delà des 5 760
mesures prévues pour quatre sondes à une mesure par 30 minutes pendant 30 jours.
Les lectures datées sont limitées aux trente jours glissants ; les secteurs
plus anciens sont recyclés à l’avancement de l’anneau. Ce filtre de lecture
n’est pas une garantie d’effacement physique immédiat des anciens octets.

Les dates proviennent uniquement du téléphone. Avant synchronisation, chaque
mesure conserve identifiant de démarrage et uptime ; l’API restitue une date
`null` si elle ne peut pas la connaître. Les mesures antérieures à la première
synchronisation du démarrage courant sont situées grâce à un ancrage CRC ajouté
au journal puis relu avant d’exposer la date. Cet ancrage est restauré après
redémarrage ; les dates déjà établies restent donc disponibles. La résolution
précède le filtre de période et la pagination. Sans ancrage durable, les relevés
restent sans date et ne sont pas placés sur la courbe ; leur nombre reste visible.
Une synchronisation échouée est retentée au plus toutes les 30 secondes par la
page, y compris après redémarrage du hub. Elles ne redeviennent jamais des mesures
courantes. Le redémarrage recrée une banque radio entièrement en attente.
L’expiration utilise toujours l’uptime, indépendamment de l’heure du téléphone.

La boucle radio publie des snapshots et des mesures dans des files sans attente.
Un travailleur sur le cœur 0 possède HTTP, provisioning, NVS et journal. Les
requêtes ne déclenchent pas directement d’écriture flash. Les noms sont sauvés
après 1,5 s de calme et avant la veille dans un bloc NVS atomique avec CRC, puis
relus pour confirmer leur persistance. Une réponse HTTP 202 signifie qu’elle
est encore en attente ; le dialogue reste ouvert jusqu’à confirmation du statut
`names_persistence:saved`. Un échec est signalé dans le dialogue et l’API, y
compris si la dernière tentative avant veille échoue. Une file de 16 mesures absorbe les pauses
du travailleur ; un débordement est compté et visible. Les SDK flash ESP32 peuvent
suspendre brièvement les caches pendant une écriture, même avec cette séparation.

## API locale bornée

| Requête | Résultat / validation |
| --- | --- |
| `GET /api/status` | Snapshot du même modèle que l’OLED, quatre identifiants, noms, état, tension µV ou null, âge, batterie inconnue, lux, réseau, horloge et santé du journal. |
| `GET /api/history?node=1&days=30&after=0` | `node` 1–4, `days` 1/7/30, curseur décimal optionnel, 96 enregistrements maximum par page ; `next` est une chaîne décimale et `more` indique une page suivante. |
| `POST /api/time` | Formulaire `utc_ms`, entier entre 2020-01-01 et 2100-01-01. Réponse 202 ; ancrage durable différé, statut `clock_persistence`. |
| `POST /api/plants` | Formulaire `id` 1–4 et `name` : UTF-8 valide, 1–24 caractères, 48 octets maximum, sans contrôles ni espaces aux extrémités. Réponse 202, sauvegarde différée. |

Endpoints d’assistant disponibles uniquement depuis le côté AP, avec Host
`192.168.4.1` (port 80 facultatif) :

| Requête | Effet |
| --- | --- |
| `GET /api/setup/status` | État, message, SSID du kit, URL LAN après réussite et temps de fermeture ; aucun mot de passe. |
| `GET /api/setup/networks` | Scan asynchrone, 16 SSID maximum, RSSI et indicateur de protection ; SSID aux octets UTF-8 invalides écartés, saisie manuelle toujours possible. |
| `POST /api/setup/connect` | Formulaire `ssid` 1–32 octets, `password` vide pour réseau ouvert, 8–63 octets ou 64 caractères hexadécimaux ; candidat mis en attente, pas d’écriture directe. |
| `POST /api/setup/cancel` | Corps vide et candidat encore en attente/en cours ; rétablir l’ancien réseau en RAM. Après commit, réponse 409 sans déconnexion. |
| `POST /api/setup/close` | Corps vide et configuration réussie ; fermer l’AP après deux secondes. |

Les POST de configuration exigent aussi une origine correspondant
au Host AP (port 80 facultatif). Leur corps est borné à 512 octets pour le formulaire encodé ; les
corps des API de suivi conservent leur limite de 256 octets.

Les POST exigent `Content-Type: application/x-www-form-urlencoded` et
`X-Plant-Request: 1` ; une origine fournie doit correspondre à l’hôte. Le serveur
LAN accepte uniquement son IP actuelle et son nom annoncé `.local`, avec port 80
facultatif et sans distinction de casse DNS. Les redirections captives côté AP
conservent leur traitement propre. Le serveur
ferme chaque connexion, borne les en-têtes à 1 024 octets, le corps à 256 octets
et la durée à 4 s (corps de configuration : 512 octets). Il écrit les réponses en fragments bornés, sans attendre
qu’un client lent libère sa fenêtre TCP : un seul `send(fd, …, MSG_DONTWAIT)`
de 512 octets maximum par tour du travailleur, reprise au tour suivant en cas
de `EAGAIN`/`EWOULDBLOCK`/`EINTR`, fermeture sur erreur définitive. Le SDK installé
Arduino-ESP32 2.0.17 expose `fd()` mais hérite d’un `availableForWrite()` qui renvoie
toujours zéro ; ce dernier n’est donc pas utilisé. Les noms sont rendus avec `textContent`.
L’API reste un service du réseau domestique sans gestion de comptes.

Dans le navigateur, chaque requête expire après 6 secondes et libère son état
d’attente pour permettre un nouvel essai. Les polls de statut n’interrompent pas
un historique en cours ; un choix explicite de sonde ou de période le remplace.
Un échec de pagination garde un avertissement d’historique incomplet lors des
redimensionnements, jusqu’à un rechargement réussi. Les quatre noms sont validés
avant le premier POST de leur formulaire.
Les courbes sont coupées après 40 minutes entre deux relevés (cadence nominale
de 30 minutes avec marge), sur défaut ADC et lors d’un nouveau démarrage.

## Vérification reproductible

```powershell
$env:PYTHONUTF8 = '1'
$env:PYTHONPATH = (Join-Path $PWD '.tools/xiao-diagnostic-python')
$env:PLATFORMIO_CORE_DIR = (Join-Path $PWD '.tools/platformio')
python -m platformio run -d firmware/PlantStationVeml7700 -e heltec_v3_text -e heltec_v3_json -e heltec_v3_soil_lora -e heltec_v3_plant_dashboard
python tools/run_plant_dashboard_tests.py
python tools/run_soil_lora_host_tests.py
python firmware/PlantStationVeml7700/test/run_host_tests.py
node tests/plant_dashboard_browser_test.cjs
node tests/plant_setup_browser_test.cjs
```

Le test navigateur exige Playwright disponible pour Node et un navigateur
Chromium installé (`PLANT_BROWSER_PATH` peut désigner son exécutable). Sur ce poste,
`NODE_PATH` désigne les modules du runtime Codex et Edge est utilisé. Les captures
`output/plant-dashboard-tests/browser-simulation-*.png` sont des **simulations
d’API**, jamais une preuve de réception matérielle. L’aperçu autonome demeure
explicitement fictif dans `design/plant-dashboard-preview/index.html`.

La sauvegarde, l’installation application seule à `0x10000` et les preuves
matérielles sont suivies séparément. Il reste à vérifier sur la carte l’assistant Wi-Fi,
le QR scanné au téléphone, la réception pendant HTTP lent, PRG/RST et l’historique
après redémarrage. Les simulations de trente jours ne prouvent pas trente jours
de fonctionnement réel ou une autonomie sur batterie.
