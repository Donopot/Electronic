# Plant Station : tableau de bord local

Le profil `heltec_v3_plant_dashboard` ajoute le visage du robot, les détails des
quatre sondes et un QR d’accès au site hébergé sur le Heltec. Les trois profils
existants sont conservés. Aucune modification des XIAO ni du protocole PS v1.

## Utilisation

1. Sur le Heltec, appuyer brièvement sur PRG pour passer du visage aux détails,
   puis au QR. Un appui de trois secondes demande la veille, effective après
   relâchement ; RST redémarre.
2. Sans réseau enregistré, la page QR affiche **CONFIGURATION BLE**, le nom
   `PROV_Plant_…` et son code local. Dans ESP BLE Provisioning sur le téléphone,
   sélectionner ce nom, saisir le code puis le Wi-Fi domestique et son mot de
   passe. Le transport utilise Security1 avec preuve de possession. Les secrets
   ne sont pas publiés dans l’API ou les journaux série.
3. Une fois connecté, la page **ACCÈS LOCAL** affiche le QR de l’URL IP et le
   journal série indique URL et nom `plant-….local`. Le téléphone doit être sur
   le même réseau. Le QR d’accès contient `HTTP://IP/`, en majuscules pour obtenir
   un symbole alphanumérique version 1 à deux pixels par module avec quatre
   modules de marge calme. Ce QR est distinct du provisioning BLE.
4. La page synchronise l’heure depuis le téléphone. Elle montre les mesures
   réellement reçues, les noms personnalisables et les courbes 24 h, 7 j et 30 j.
   Une interruption de réseau du navigateur laisse un avertissement explicite
   sur les dernières valeurs affichées.

La commande série `wifi-setup` suivie d’un retour à la ligne ouvre une nouvelle
session BLE ; PRG garde entièrement son rôle d’interface et de veille. Un réseau
perdu déclenche la reconnexion, puis une possibilité de provisioning après
30 secondes. Pas de point d’accès, compte, service externe ni publication Internet.

Les seuils restent indéfinis : les sondes fraîches sont **à étalonner**. Attente,
défaut ADC et péremption de 90 min sont des états distincts. Le modèle prépare
une qualification configurable pour un calibrage ultérieur sans la proposer
dans le formulaire de noms. Un état sec conserve en parallèle les avertissements
des autres sondes. La batterie est **non communiquée** (`B--` sur l’OLED,
`battery_percent:null` dans l’API), car PS v1 ne transporte pas cette mesure.

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
synchronisation du démarrage courant peuvent être situées avec son horloge.
Après un nouveau redémarrage, celles restées sans date ne sont pas placées sur
la courbe ; leur nombre reste visible. Elles ne redeviennent jamais des mesures
courantes. Le redémarrage recrée une banque radio entièrement en attente.
L’expiration utilise toujours l’uptime, indépendamment de l’heure du téléphone.

La boucle radio publie des snapshots et des mesures dans des files sans attente.
Un travailleur sur le cœur 0 possède HTTP, provisioning, NVS et journal. Les
requêtes ne déclenchent pas directement d’écriture flash. Les noms sont sauvés
après 1,5 s de calme et avant la veille ; une réponse HTTP 202 signifie que leur
persistance est encore en attente. Une file de 16 mesures absorbe les pauses
du travailleur ; un débordement est compté et visible. Les SDK flash ESP32 peuvent
suspendre brièvement les caches pendant une écriture, même avec cette séparation.

## API locale bornée

| Requête | Résultat / validation |
| --- | --- |
| `GET /api/status` | Snapshot du même modèle que l’OLED, quatre identifiants, noms, état, tension µV ou null, âge, batterie inconnue, lux, réseau, horloge et santé du journal. |
| `GET /api/history?node=1&days=30&after=0` | `node` 1–4, `days` 1/7/30, curseur décimal optionnel, 96 enregistrements maximum par page ; `next` est une chaîne décimale et `more` indique une page suivante. |
| `POST /api/time` | Formulaire `utc_ms`, entier entre 2020-01-01 et 2100-01-01. Horloge RAM uniquement. |
| `POST /api/plants` | Formulaire `id` 1–4 et `name` : UTF-8 valide, 1–24 caractères, 48 octets maximum, sans contrôles ni espaces aux extrémités. Réponse 202, sauvegarde différée. |

Les POST exigent `Content-Type: application/x-www-form-urlencoded` et
`X-Plant-Request: 1` ; une origine fournie doit correspondre à l’hôte. Le serveur
ferme chaque connexion, borne les en-têtes à 1 024 octets, le corps à 256 octets
et la durée à 4 s. Il écrit les réponses en fragments bornés, sans attendre
qu’un client lent libère sa fenêtre TCP. Les noms sont rendus avec `textContent`.
L’API reste un service du réseau domestique sans gestion de comptes.

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
```

Le test navigateur exige Playwright disponible pour Node et un navigateur
Chromium installé (`PLANT_BROWSER_PATH` peut désigner son exécutable). Sur ce poste,
`NODE_PATH` désigne les modules du runtime Codex et Edge est utilisé. Les captures
`output/plant-dashboard-tests/browser-simulation-*.png` sont des **simulations
d’API**, jamais une preuve de réception matérielle. L’aperçu autonome demeure
explicitement fictif dans `design/plant-dashboard-preview/index.html`.

La sauvegarde, l’installation application seule à `0x10000` et les preuves
matérielles sont suivies séparément. Il reste à vérifier sur la carte BLE/Wi-Fi,
le QR scanné au téléphone, la réception pendant HTTP lent, PRG/RST et l’historique
après redémarrage. Les simulations de trente jours ne prouvent pas trente jours
de fonctionnement réel ou une autonomie sur batterie.
