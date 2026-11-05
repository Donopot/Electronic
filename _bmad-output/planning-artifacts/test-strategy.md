# Stratégie de vérification

Date : 27 octobre 2026. Les commandes se lancent depuis la racine Electronic.

## Contrôles logiciels reproductibles

```powershell
node tests/dashboard_state_test.cjs
$compiler = '.tools\platformio\packages\toolchain-riscv32-esp\bin\riscv32-esp-elf-g++.exe'
& $compiler -std=gnu++14 -Wall -Wextra -Werror -c tests/compensation_test.cpp -o .pio/compensation_test.o
& $compiler -std=gnu++14 -Wall -Wextra -Werror -c tests/lora_register_validation_test.cpp -o .pio/lora_register_validation_test.o
```

Chaque commande doit terminer avec le code 0. Les fichiers C++ contiennent
des `static_assert` : leur compilation constitue l'exécution du contrôle.
Le test Node exécute le script UI réel dans un contexte simulé ; il ne valide
pas un navigateur complet ou la carte physique.

```powershell
$env:PYTHONPATH = Join-Path $PWD '.tools\python'
$env:PLATFORMIO_CORE_DIR = Join-Path $PWD '.tools\platformio'
python -m platformio run -e enviroterminal_local_web_meteo_tx
python -m platformio run -d firmware/WeatherReceiver
python -m platformio run -d firmware/PlantHubDiagnostic
```

Les trois commandes doivent réussir. Ces chemins correspondent aux outils
déjà installés sur ce poste. Sur un autre poste, installer PlatformIO et les
dépendances fixées avant exécution. La story 1.2 ajoutera l'arrêt automatique
au premier échec et la restauration de l'environnement.

## Matrice de non-régression

| Sujet | Contrôle présent | Limite / contrôle à ajouter |
| --- | --- | --- |
| Compensation BMP180 | Vecteur Bosch et rejets à la compilation | Ne couvre pas I2C physique |
| Registres radio | Modes, fréquence, CRC, puissance et sync | Ne mesure pas la RF rayonnée |
| Interface | Local, distant, 179 999/180 000 ms, réseau absent | Ne couvre pas l'API sur carte |
| Parseur RX | Règles lisibles dans `publishWeatherPacket` | Story 1.3 : tests du code réel |
| Builds | TX web, RX, Heltec | Ne vérifie ni flash ni matériel |
| LoRa durable | Derniers essais archivés | Stories 2.1/2.2 : collecte et analyse |
| Panne et retour | Logique de fraîcheur existante | Story 2.3 : campagne sur carte |
| Heltec | Build et ancien journal de veille | Courant réel et nouveau lien non mesurés |

Cas minimum du parseur : paquet valide ; bornes température −40/85 et
pression 300/1100 acceptées ; valeurs juste hors plage rejetées ; `v`, `node`,
`seq`, `uptime_ms`, température ou pression absents ; chaînes à la place des
numériques ; entiers non signés hors plage ; JSON vide/tronqué ; encodages
non finis rejetés selon ArduinoJson. Contrôler les effets de publication,
pas seulement le booléen de retour.

## Campagnes matérielles

Conserver la configuration et l'état de départ ; identifier les cartes et
leurs ports réels. Les captures et scripts de contrôle série peuvent provoquer
un reset selon DTR/RTS : décrire cet effet dans le protocole d'essai.
Le script `check_heltec_sleep.py` commande PRG et RST et peut laisser la carte
en veille ; il ne fait pas partie des tests logiciels automatiques.

Pour la campagne radio, enregistrer simultanément les émissions confirmées
et réceptions ou expliciter l'absence de TX sur batterie. Les compteurs
cumulés seuls ne donnent pas un taux de livraison exact. Conserver origine
temporelle, interruptions, redémarrages et paramètres ; utiliser le rapport
de la story 2.1. Les seuils proposés figurent dans le PRD.

## Définition d'une story terminée

Critères d'acceptation vérifiés, commandes pertinentes réussies, preuve liée,
effets réels documentés, risques restants explicites et suivi mis à jour.
Un essai matériel absent ne peut pas être remplacé par une compilation.
