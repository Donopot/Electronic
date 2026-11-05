# Configuration Android : décisions et interfaces

Cette évolution est autorisée par la demande d’une petite app et la réponse
« Android ». Le portail testé reste un secours ; son scan n’est pas nécessaire
au nouveau parcours. Aucun firmware XIAO ne change. Le Heltec installé est
COM5/MAC3c:0f:02:eb:f5:28, application seule0x10000, table8Mio inchangée.
Dernier binaire validé/installé : SHA256
4c2277e00206dda021399c745a636c615170c1bb2ba1e693b123bccbe2bd6a5d.

## Architecture retenue

Android reste sur le Wi-Fi domestique. Il lit son SSID si les permissions et
réglages Android l’autorisent ; sinon le champ reste saisissable. Le mot de
passe Android enregistré n’est jamais récupéré : saisie unique dans l’app,
uniquement en mémoire pendant l’essai. Ne rien journaliser ni sauvegarder de
ces secrets. Le tableau de bord reste celui du Heltec ; bouton vers navigateur
externe, avec URL locale et nom stable mémorisés sans identifiants Wi-Fi.

Le gestionnaire standard wifi_prov_mgr_configure_sta écrit FLASH avant IP :
ne pas l’utiliser pour la transaction. Réutiliser les primitives SDK
protocomm_ble_start/stop, protocomm_set_security et protocomm_security1.
Security1 fournit X25519/PoP/AES ; aucune réimplémentation crypto.
L’app utilise la bibliothèque officielle Espressif Android lib-2.4.4,
ESPDevice avec TRANSPORT_BLE/SECURITY_1, setProofOfPossession,
initSession et sendDataToCustomEndPoint. Ne pas appeler provision().
Contrôler le sec_ver annoncé et refuser Security0 avant l’envoi d’identifiants.

Prévoir proto-ver compatible SDK, prov-session Security1 et endpoints applicatifs
de configuration/état. Contrat simple et versionné, corps bornés (512octets pour
SSID/mot de passe, réponse d’état de quelques centaines), commandes via file
du worker. Les callbacks BLE ne touchent ni NVS/flash, ni state-machine/Strings
du worker. Le worker traite le candidat avec beginCandidate/commitCandidate/
rollbackCandidate existants et publie un snapshot BLE cohérent. Identifier
l’essai pour distinguer les réponses périmées, refuser un second essai occupé,
annoncer succès seulement après commit. Nettoyer les buffers secrets.

Le secret local propre au kit existe déjà : plant-setup/password,16hex générés
et relus en NVS. Il peut servir de PoP pour le même droit de configuration.
Jamais dans API de suivi ni logs. QR d’association standard Espressif JSON :
ver=v1, name du kit, transport=ble, security=1 explicite, pop secret. Adapter
la colonne OLED au QR réel et garder quatre modules de marge ; prévoir clé
lisible et association manuelle si nécessaire. Conserver un accès aux QR Wi-Fi
temporaire et URL de secours. Interface simple, sans vocabulaire de protocole.

Ouvrir BLE au premier démarrage sans réseau et sur PRG8s, avec possibilité de
réouvrir après succès/fermeture. Finir/arrêter les sessions proprement avant
veille ; ne pas libérer irréversiblement la mémoire BT/BLE. Le portail peut
rester en secours, mais deux demandes concurrentes ne doivent pas se remplacer.
La réception radio et les lux continuent pendant ces états.

## App et construction

Projet Java natif android/PlantStation, applicationId local.plantstation.kit,
minSDK26, compile/target35, JDK17, Gradle8.11.1 et AGP8.9.2. Adapter seulement
si une incompatibilité concrète est constatée. Dépendance officielle :
com.github.espressif:esp-idf-provisioning-android:lib-2.4.4 via JitPack.
Le parent installe les outils sous .tools/android-bootstrap, sans installation
système ; attendre ses chemins si nécessaires. Aucun accès matériel ni flash
par l’implémenteur. Les builds peuvent nécessiter require_escalated.

Palette/crème/vert et typographie système cohérentes avec le site. Parcours
guidé : bienvenue/SSID du téléphone → QR du kit → Bluetooth sécurisé → saisie
du mot de passe/connexion → confirmation et « Ouvrir mes plantes ». Le nom du
réseau reste éditable ; rappeler seulement la compatibilité2,4GHz. Détecter si
le téléphone est encore sur PLANT-… et inviter à rejoindre la maison. Demander
les permissions au moment utile, expliquer refus/Localisation désactivée,
laisser la saisie manuelle accessible ; ne jamais forcer permission de lecture
SSID pour configurer avec un nom connu. Pas de logs QR/PoP : la bibliothèque
en contient ; les supprimer du build livré via R8/règles adaptées et garder
les callbacks EventBus/serialization nécessaires. Secrets absents des états
persistés, screenshots de récents et rapports publics.

API31+ : callback ConnectivityManager filtré TRANSPORT_WIFI avec
FLAG_INCLUDE_LOCATION_INFO, WifiInfo via NetworkCapabilities. Lecture SSID
soumise à ACCESS_FINE_LOCATION et localisation système ; NEARBY_WIFI_DEVICES
ne suffit pas à rendre ces champs visibles. Ancien Android : connectionInfo.
Retirer seulement les guillemets d’encadrement ; UNKNOWN_SSID/redaction et
SSID non représentable donnent un champ manuel. Bluetooth12+ : SCAN/CONNECT
runtime, anciens Bluetooth maxSDK30. Caméra au scan. Le SDK fournit CameraX
scanQRCode(PreviewView,Activity,QRCodeScanListener), ou scanner distinct si
nécessaire. QR JSON security=1 indispensable, défaut SDK=2.

Capturer les callbacks sur le thread UI, borner scan/connexion/session/essai,
annuler proprement au changement d’écran, éviter callbacks d’anciennes sessions
et doubles soumissions. Vérifier les URLs de succès : HTTP local IPv4/nom du
kit, pas d’identifiants/fragment/port arbitraire. Aucun service distant d’usage.
APK release signé localement ; clé de signature privée et mots de passe dans
.tools/private, jamais Git ni logs. Le parent fournit l’APK au téléphone pour
installation et tests ; aucune publication Play Store ni compte créé.

## Sources primaires et preuves

- https://github.com/espressif/esp-idf-provisioning-android/tree/lib-2.4.4
- https://github.com/espressif/esp-idf/blob/v4.4.7/components/wifi_provisioning/src/manager.c
- https://developer.android.com/reference/android/net/wifi/WifiInfo#getSSID()
- https://developer.android.com/reference/android/net/ConnectivityManager.NetworkCallback#FLAG_INCLUDE_LOCATION_INFO
- https://developer.android.com/develop/connectivity/bluetooth/bt-permissions

Tester contrat/transactions avec vraie boucle worker et SDK I/O simulées,
payload QR/limites OLED, downgrade/auth refusés et lifecycle réouvrable. Tests
app des champs, permissions refusées, callbacks périmés, réseau faux puis
réessai et liens locaux. Distinguer compilation/tests simulés et exécution
sur vrai Android, qui reste une vérification utilisateur après APK prêt.
