# Plant Station — veille PRG restaurée, 28 octobre 2026

Version **1.2.0** installée sur COM5, ESP32-S3 révision0.2, MAC3c:0f:02:eb:f5:28. Les acquisitions VEML7700 GPIO4/5 et l'OLED GPIO17/18 sont conservés.

## Utilisation

- Maintenir **PRG 3 secondes**, attendre « Relacher PRG », puis relâcher pour la veille profonde.
- Appui court sans effet. Si PRG était tenu au démarrage, le relâcher avant un nouvel appui long.
- **RST**, avec PRG relâché, redémarre écran et acquisitions. Aucun réveil automatique ni réveil par PRG seul.

Le VEML7700 reçoit ALS_SD=1 avec relecture. L'OLED reçoit DISPLAYOFF puis Vext est coupé ; LED35 basse, radio RESET12 bas et NSS8 haut, GPIO tenus pendant le sommeil. Les maintiens sont libérés au redémarrage. Capteur absent ou shutdown non confirmé : avertissement, puis sommeil du hub quand même. Aucun courant réel n'a été mesuré ; ce mode n'est pas une coupure physique de l'alimentation.

## Vérification logicielle

Deux profils texte/JSON compilés : RAM19892/327680, flash314217/3342336 octets. [Builds](plant-station-sleep-builds-20261028.log).

Tests hôte réussis : précédents tests pilote/OLED/main et 62 JSON ; PRG court/long, rebonds, relâchement, bouton tenu au boot, premier appui sans tour relâché, seuil3000ms/2999ms, wrap ; séquence GPIO/flush. Le vrai main atteint la veille dans sept scénarios : capteur normal, déconnexion, absent au boot, démarrage bus refusé, bitSD non pris en compte, lecture courte et request échoué. La séquence OLED/extinction et entrée en sommeil reste obligatoire dans tous ces cas. [Tests](plant-station-sleep-tests-20261028.log).

Revue indépendante effectuée : deux cas limites de bouton corrigés, couverture des erreurs d'extinction étendue, documentation complétée. Le retour RST de bout en bout est vérifié sur matériel ci-dessous.

## Téléversement et test matériel

BIN1.1.0 conservé : `.tools/private/plantstation-veml-v1.1.0-pre-sleep-20261028.bin`. Téléversement1.2.0 réussi en12,334s ; hachages vérifiés par esptool. [Upload](plant-station-sleep-upload-20261028.log).

Test via DTR/RTS du CP2102 commencé à **09:09:26 UTC /10:09:26 à Paris** :

1. Démarrage1.2.0, OLED0x3C et VEML0x10 confirmés ; mesures normales.
2. PRG0,3s : mesures conservées, aucune veille.
3. PRG3,6s : demande de relâcher observée à10500ms, pas d'entrée en veille tant que PRG est tenu.
4. Relâchement : sommeil VEML7700 confirmé à11099ms, entrée en veille profonde à11110ms.
5. Silence vérifié pendant5s, puis appui PRG seul sans réveil ni sortie série.
6. RST : OLED réinitialisé et nouvelles acquisitions à1403,2403,3403 et4403ms, de108,5952 à109,1328lux.

Résultat **PASS, code0**. Carte laissée réveillée avec écran et acquisitions actifs. [Journal matériel complet](plant-station-sleep-hardware-20261028.log).

Commande reproductible (le script commande réellement PRG/RST et termine réveillé) :

```powershell
$env:PYTHONPATH = Join-Path $PWD '.tools\python'
python tools/check_plant_station_sleep.py --port COM5 --output diagnostics/plant-station-sleep-retest.log
```

La réception des futures sondes LoRa868MHz reste prévue pour Plant Station et n'est pas initialisée dans ce diagnostic. Les validations ci-dessus n'attestent ni une liaison radio ni un courant de veille mesuré.
