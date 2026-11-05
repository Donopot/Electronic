# Contrat de configuration Android/BLE v1

Transport SDK Espressif `protocomm_ble`, service
`021a9004-0382-4aea-bff4-6b3f1c5adfb4`. Nom public `PLANT-` + six caractères
MAC hexadécimaux majuscules. Aucun gestionnaire `wifi_prov_mgr_configure_sta`.

Le QR Espressif JSON contient `ver:"v1"`, `name`, `transport:"ble"`,
`security:1` **explicite** et `pop` (clé propre au kit, 16 hex en NVS
`plant-setup/password`). Cette clé n'est exposée que sur l'OLED/QR ; jamais
dans l'API de suivi, les réponses BLE ou les logs. Le QR OLED accepte jusqu'à
la version 9 avec quatre modules de marge. Nom et code sont lisibles à côté ;
PRG court passe au QR Wi-Fi puis au QR URL du portail de secours.

| Caractéristique / endpoint | UUID court | Charge |
| --- | --- | --- |
| `proto-ver` | FF50 | Non chiffré : `{"prov":{"ver":"v1.1","sec_ver":1,"cap":[]},"plant":{"ver":1}}` |
| `prov-session` | FF51 | Protobuf Security1 officiel : X25519/PoP/AES, aucun Security0. |
| `plant-config` | FF52 | Après session authentifiée, formulaire UTF-8 URL-encodé, 512 octets max. |
| `plant-status` | FF53 | Après session authentifiée, requête ASCII `1`, réponse JSON ≤384 octets. |

Android refuse QR incomplet/Security0, `sec_ver` absent/différent de 1,
capacité `no_pop`, version différente et échec de session **avant** tout envoi
Wi-Fi. Il utilise `ESPDevice` BLE/SECURITY_1, `setProofOfPossession`,
`initSession`, puis `sendDataToCustomEndPoint`, sans `provision()`.

Configuration : `v=1&id=123&ssid=Ma+maison&password=…`. Identifiant aléatoire
32 bits non nul par essai. Exactement quatre champs, pas de doublon/NUL ni de
contrôle ; SSID 1–32 octets, mot de passe vide ou 8–63 octets, ou PSK hex de
64 caractères. La réponse `{"queued":true}` confirme uniquement la copie en
file ; `{"error":"busy"}` ou `invalid` permet une reprise sans faux succès.

Snapshot : `{"v":1,"id":123,"rejected_id":0,"state":"testing","url":"",
"hostname":"plant-xxxxxx.local"}`. États `idle`, `testing`, `failed`,
`connected`. Un second essai alors que BLE/HTTP teste reste refusé, avec son
identifiant dans `rejected_id` ; l'essai courant garde son `id`. Le client
ignore les snapshots d'un autre essai. `connected` et l'URL n'apparaissent
qu'après GOT_IP, vérification du SSID et écriture FLASH réussie. Un envoi ne
signifie jamais une réussite. L'échec à 25 s restaure l'ancien réseau RAM et
conserve sa sauvegarde ; un nouvel essai corrigé est possible.

Sans candidat ni essai BLE identifié, `id:0,state:"connected"` décrit aussi
une connexion déjà sauvegardée, après réouverture PRG ou redémarrage. Ce statut
en lecture seule exige que la station soit actuellement en ligne et que son
SSID **et** son mot de passe courants correspondent aux credentials conservés
par le worker. Il n'atteste aucun nouvel essai : les envois continuent à exiger
leur identifiant non nul correspondant. Aucun candidat RAM en attente/en cours
ni configuration différente/non sauvegardée ne peut produire cette reprise.
La lecture de configuration SDK n'est effectuée que lorsque BLE est actif et
la copie temporaire est effacée ; formulaire web/OLED et état d'essai restent
inchangés.

Les callbacks ne possèdent aucun String/state-machine Wi-Fi/NVS. Une file
fixe d'une commande et une file snapshot d'un élément bornent la mémoire.
Le worker prend les commandes sans attendre, utilise la transaction existante
RAM → IP → FLASH/rollback et publie le snapshot. L'admission reste réservée
pendant l'effacement des octets retenus dans la file. Buffers de code/candidat
et copies temporaires sont nettoyés ; les tableaux SDK restent sa propriété.
HTTP/radio/lux continuent à leurs frontières existantes.

BLE ouvre au démarrage sans réseau, après perte prolongée et sur PRG 8 s.
Le portail web demeure disponible. Après réussite, fermeture avec le portail
au bout de cinq minutes, réouverture PRG possible ; arrêt avant veille sans
libération irréversible de mémoire Bluetooth. L'app borne recherche à 20 s,
session à 15 s, confirmation à 40 s, lecture SSID à 4 s et scan QR à 45 s.
Passer en arrière-plan annule les callbacks par génération et efface les
secrets. Une perte BLE laisse au kit le temps de terminer son essai borné ;
l'app exige une nouvelle confirmation et ne suppose pas le résultat.

La vérification du worker exécute la vraie boucle avec uniquement l'I/O SDK
simulée ; le refus d'authentification du SDK est **simulé**, sans prétendre
tester la crypto ou une liaison BLE réelle. L'essai physique Android du parent
reste nécessaire pour le QR, le SDK, la coexistence radio et les permissions.
