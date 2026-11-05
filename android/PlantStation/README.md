# Mes plantes · Android

Application Java native locale, Android 8/API 26 minimum, cible/compilation 35.
Elle garde le téléphone sur son Wi-Fi, associe le kit par QR/code et envoie le
candidat par le SDK officiel Espressif Security1. Elle n'appelle jamais
`provision()`. Aucun compte, Internet, service d'usage distant ou télémétrie :
la permission INTERNET héritée est explicitement retirée du manifeste livré.
Le navigateur externe ouvre ensuite le tableau de bord HTTP local existant.

## Construire

JDK 17, Gradle 8.11.1 (wrapper et SHA256 fournis), AGP 8.9.2, plateforme
android-35, build-tools 35.0.0. Les versions transitives sont verrouillées dans
`app/gradle.lockfile` et les téléchargements vérifiés par
`gradle/verification-metadata.xml`. Espressif est épinglé au tag `lib-2.4.4`
via JitPack. Aucun outil système n'est installé par le projet.

Sur ce poste, `tools/build_plant_android.ps1` lit les chemins préparés dans
`.tools/android-bootstrap/environment.json`. Sur un autre poste, définir
`JAVA_HOME` et `ANDROID_HOME`, puis utiliser `gradlew.bat` ou `./gradlew` :

```powershell
tools/build_plant_android.ps1
# Équivalent depuis ce dossier avec l'environnement prêt :
./gradlew.bat --no-daemon :app:testDebugUnitTest :app:lintDebug :app:assembleRelease
```

La clé de signature est privée et locale, hors Git, sous `.tools/private` à la
racine du dépôt. Créer un keystore avec `keytool -genkeypair` (RSA 3072, alias
`plantstation`), puis `.tools/private/plantstation-signing.properties` :

```properties
storeFile=../../.tools/private/plantstation.jks
keyAlias=plantstation
storePassword=<mot-de-passe-prive>
keyPassword=<mot-de-passe-prive>
```

Ne jamais publier ces fichiers ni leurs mots de passe. Le build ne génère pas
de clé de remplacement et ne publie rien. APK :
`app/build/outputs/apk/release/app-release.apk`. Le certificat permet de
réinstaller les futures versions avec la même signature.

## Vérifier et livrer

Les tests JUnit exercent les QR/security=1 explicite, refus de downgrade et
`no_pop`, SSID/UTF-8, tailles, états périmés/doubles envois, reprise et URLs.
Robolectric exécute la vraie Activity pour les permissions refusées, la voie
manuelle, le Wi-Fi temporaire du kit et l'effacement du code en arrière-plan.
Ce sont des **simulations Android**, sans téléphone ni radio.

La release R8 supprime les appels Log et printStackTrace du SDK, qui contient
sinon le QR et le code dans ses logs. Vérifier **le DEX livré**, pas seulement
les règles : `tools/verify_plant_android_apk.py`. Les règles EventBus et
protobuf restent présentes. Les mots de passe/codes restent en mémoire, les
champs ne sont ni sauvegardés ni proposés à l'autofill et FLAG_SECURE protège
captures/récents. Une mise en arrière-plan retire la session et ses secrets.

Le parent fournit l'APK signé au téléphone, puis effectue les essais réels :
permissions acceptées/refusées, QR OLED/code manuel, mauvaise clé puis bonne,
Wi-Fi du téléphone conservé, confirmation après sauvegarde, navigateur LAN,
PRG 8 s, veille/RST et noms/historique conservés. Aucun flash ni accès série
n'est effectué par l'implémenteur.
