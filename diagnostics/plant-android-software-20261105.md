# Plant Station Android — preuves logicielles

Implémentation du spec `spec-plant-android-setup.md` et de ses deux contextes.
Revue parent puis essais physiques séparés requis. Aucun flash, accès série,
installation téléphone ou échange BLE matériel par l'implémenteur.

- App native Java `android/PlantStation`, JDK 17/Gradle 8.11.1/AGP 8.9.2,
  min26/cible35 ; dépendance officielle Espressif lib-2.4.4, aucune crypto maison.
- Lecture SSID optionnelle (API31 callback Wi-Fi avec location-info), permissions
  au moment utile, repli manuel, caméra locale CameraX/MLKit, association par
  QR Security1 explicite ou nom/code. Le téléphone reste sur son Wi-Fi.
- Session SDK authentifiée, refus downgrade/no_pop, envoi custom endpoint,
  état corrélé, délais et reprises, confirmation seulement après commit.
- BLE protocomm et files bornées, vraie transaction du worker réutilisée ;
  essais RAM/GOT_IP/FLASH et rollback communs à HTTP. Les callbacks ne touchent
  pas Strings/NVS. Queue et buffers secrets nettoyés, arrêt/réouverture BLE.
- QR Android avec marge de quatre modules et nom/code lisibles ; QR Wi-Fi et
  portail conservés. Aucun changement des programmes XIAO, noms, historique,
  calibrage différé, batterie inconnue ou protocole radio.
- Guide cadeau, contrat BLE, commande de construction et audit APK reproductible.

## Résultats

Quatre builds Heltec **PASS**. Dashboard : 1 450 097 octets de code,
74 092 octets RAM statiques. APK release signé : **23 165 724 octets**,
SHA256 `28b310f4fba01347d8ed290031785819e7a0cb9cebf39e28ed727edfbfbbbdb3`.
Firmware dashboard SHA256
`7da6fa07a583a1b9c3652d0785eca0c3a7ea417d72f52243b0d394c16ce4667b`.

**10 tests Android PASS** : 7 contrats JUnit et 3 tests de la vraie Activity
sous Robolectric (permissions refusées, voie manuelle, Wi-Fi temporaire du kit,
code effacé en arrière-plan, champs non sauvegardés et FLAG_SECURE).
Lint : zéro erreur, 24 avertissements (notamment textes français directs et
API de compatibilité), sans baseline masquant d'erreur.

Audit du binaire signé **PASS** : certificat attendu, min26/cible35,
permission INTERNET absente du manifeste livré, zéro invocation Log ou
printStackTrace dans le SDK Espressif, zéro écriture Android Log dans tous
les DEX. Les seules lectures de Log d'autres dépendances ne journalisent rien.
Clé privée et mots de passe restent hors Git sous `.tools/private`.

Suite dashboard **PASS**, assertions antérieures conservées ; nouveau scénario
avec **vraie boucle worker et véritable adaptateur BLE**, uniquement SDK I/O
simulé : auth refusée, version/tailles/NUL, file pleine, contention BLE/HTTP,
identifiants périmés, mauvais réseau/timeout/ancien FLASH préservé, correction,
GOT_IP, commit, arrêt/réouverture/échec de démarrage et absence de secrets.
Tests OLED vérifient les bornes 128×64 et les quatre modules de marge jusqu'à
une taille QR de 53 modules ; l'encodeur/scanner est simulé dans ces tests.

Suites existantes radio et lux/OLED/veille **PASS**. Tests navigateurs tableau
de bord et portail de secours **PASS**, avec API explicitement simulées.
Les captures navigateur ne sont pas des preuves matérielles.

Les artefacts prêts à revue sont sous `output/plant-android/` ; tous les SHA,
logs et distinctions simulé/réel figurent dans
`plant-android-verification-manifest-20261105.json`. L'audit peut se répéter avec
`python tools/verify_plant_android_apk.py --expected-cert 3ff44e455caaf5a2e4a24d50a33dd150f9ca2d919567ff29f0e6e81a32c337ce`.

## Reste au parent après revue

Installer les binaires revus et vérifier sur un vrai Android : permissions,
QR OLED et code manuel, authentification Security1, mot de passe faux puis
correct, Wi-Fi domestique du téléphone maintenu, ouverture LAN et sauvegarde
après RST, PRG8, arrêt avant veille et radio/lux actifs. Les simulations ne
prouvent ni le décodage optique ni la coexistence matérielle ni le succès d'une
session cryptographique réelle. La sauvegarde matérielle faite par le parent
est suivie séparément ; aucune preuve physique nouvelle n'est revendiquée ici.
