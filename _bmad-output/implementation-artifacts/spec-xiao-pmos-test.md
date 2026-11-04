---
title: 'Commander et contrôler le P-MOS de SOIL-01'
type: 'feature'
created: '2026-11-03'
status: 'done'
route: 'oneshot'
review_loop_iteration: 0
context: []
---

<frozen-after-approval reason="continuité du montage et de son test autorisés par l'utilisateur">

## Intent

L'utilisateur a câblé l'AO3401 (S/2 sur 3V3, D/3 sur VCC sonde, G/1 vers D0 via 220 Ω) et confirme un rappel G-S de 10 kΩ. Il a remis les deux cartes en USB, antenne raccordée et batterie déconnectée. Installer un profil de test qui commande cette alimentation et conserve la liaison SOIL-01 au Heltec, puis vérifier au multimètre OFF et ON avant l'essai automatique.

Le nouveau profil doit relâcher D0 au démarrage, utiliser uniquement LOW puis OUTPUT pour ON et INPUT sans rappel pour OFF, sans jamais forcer HIGH sur le bouton partagé. Au repos automatique, couper la sonde ; toutes les trente secondes environ, l'alimenter, attendre une seconde, prendre une médiane de cinq conversions ADS1115 et couper avant toute initialisation ou émission radio. Les erreurs d'acquisition doivent aussi couper l'alimentation, sans publier une ancienne mesure comme courante. Conserver format radio, cadence minimale de 30 s, firmware Heltec et profils historiques. Ne pas annoncer la veille longue ou l'autonomie comme réalisées.

Prévoir des commandes USB bornées OFF (maintenu, sans acquisition ni émission), ON (contrôle manuel limité à 120 s, puis OFF ; coupure aussi si USB physiquement déconnecté), AUTO (retour au cycle). La fermeture d'un terminal seule ne doit pas interrompre le contrôle manuel au multimètre ; les sorties USB ne doivent jamais bloquer. Le premier cycle après démarrage automatique attend 30 s ; AUTO explicite peut déclencher la première mesure immédiatement, en respectant le garde d'émission existant. Mesurer réellement VCC/GND de sonde en OFF puis ON, ensuite comparer au moins trois mesures automatiques TX/RX.

</frozen-after-approval>

## Implementation Notes

- Base : `bb27e548dedb794b03f64a0e9f5673b754f77a80`. Travaux et diagnostics préexistants conservés ; seuls les fichiers de cette étape seront enregistrés.
- Chemins : `firmware/XiaoSoilDiagnostic/src/main.cpp` et nouveau contrôleur interne de puissance ; profil additionnel dans `platformio.ini` ; doubles GPIO/USB et scénarios dans `test/` ; lanceur ciblé existant et documentation du test. Aucune modification du protocole ou du Heltec.
- Réutiliser le bus logiciel borné et `readA0()`, le garde 30 s de `serviceTransmit()`, `BoundedUsbLog` (4 ms par ligne) et la variante batterie corrigée. Contrôler aussi le mapping D0 = P0.02. Ne pas considérer la lecture logique de grille comme une mesure de VCC.
- Vérification : tests hôte sur le vrai contrôleur et intégration, régressions historiques, trois compilations XIAO, sauvegarde de l'application actuelle, installation USB, contrôles matériels puis capture TX/RX. La présence de résistances et la coupure réelle seront distinguées des preuves purement logicielles.
- Réalisation : profil `xiao_soil_lora_pmos`, contrôleur `ProbePower.h` avec stabilisation 1 s/médiane cinq, commandes USB bornées et maintien manuel de VCC. Le mode manuel ne réalise aucun scan ADC ; les erreurs d'acquisition automatique sont différées jusqu'à la coupure. Reader interne injectable pour les scénarios d'intégration du vrai cycle.
- Vérification logicielle : dix scénarios P-MOS, cinq historiques XIAO et quatre Heltec réussissent ; trois compilations XIAO et trois tests batterie/configuration/ELF réussissent. Relecture indépendante ciblée : six constats traités, aucun restant.
- Sauvegarde de l'application précédente : `.tools/private/before-pmos-20261103/`. Profil P-MOS installé par DFU application seule (92 400 octets) ; OFF immédiatement commandé et acquitté avec grille à 1. Le Heltec n'a pas été reflashé.
- Contrôle utilisateur avant installation : **136,6 mV** entre VCC et GND sonde en OFF (correction du premier texte 1366 mV). Commande ON de contrôle acceptée ensuite avec grille à 0 ; tension ON encore attendue. Batterie déconnectée, USB seul.
- Reprise le 4 novembre : l'utilisateur annonce le fonctionnement et **3,49 V en ON**. La XIAO n'est pas énumérée en USB ; le Heltec reçoit quatre paquets consécutifs valides en 105,062 s, séquences 11 à 14, autour de 2,607 V, sans erreur. Preuves dans `diagnostics/xiao-pmos-20261104.md`. La sortie 3V3 et l'alimentation actuelle sont demandées pour expliquer les 3,49 V ; retour OFF après installation et comparaison simultanée TX/RX restent requis. Le statut reste `in-progress`.
- Contrôle suivant le 4 novembre : **3,49 V était une erreur de saisie, corrigée explicitement en 3,2 V ON** ; l'utilisateur mesure **3,3 V sur 3V3** et confirme **USB et batterie ensemble**. Aucun flash ni recâblage réalisé. COM8 est de nouveau détecté ; profil P-MOS confirmé, OFF commandé et acquitté à 10:45:16 (Paris), mesure physique OFF attendue avant AUTO et capture TX/RX.
- Validation finale le 4 novembre : **196 mV OFF** relevés par l'utilisateur sous le profil installé ; AUTO repris à 10:47:45 (Paris). Capture simultanée COM8/COM5 de 105,093 s acceptée : trois mesures identiques TX/RX, séquences 54–56, session 3105728615, ADS 0x48, 2,679750 / 2,679500 / 2,679375 V, intervalle 31,138 s, sans erreur, perte observée, doublon ni divergence. Journal de grille relâchée avant chaque émission. Preuves `diagnostics/soil-pmos-hardware-20261104-094757Z*` et compte rendu `diagnostics/xiao-pmos-20261104.md`. AUTO laissé actif. Les relevés manuels ne mesurent ni le courant résiduel ni l'autonomie ; la veille longue et la validation de recharge restent hors périmètre. Intent réalisé, statut `done`.

## Review Triage Log

- `low`, corrigé : les erreurs `readA0` étaient journalisées avant la coupure malgré le commentaire ; messages différés jusqu'au rapport après OFF, test avec observateur GPIO lors des journaux.
- `medium`, corrigé : `ON\0bad` reconnu à tort par `strcmp` ; rejet de toute ligne contenant un octet hors A-Z (sauf CR/LF) et test binaire.
- `low`, corrigé : scan et conversions manuels pouvaient retarder l'échéance ; ON manuel limité au maintien VCC, sans acquisition, échéance vérifiée à chaque tour de 10 ms et aux frontières testées.
- `medium`, corrigé : stabilisation 1 s non protégée par les tests ; premier appel lecteur vérifié à partir de l'horodatage de la vraie activation GPIO.
- `medium`, corrigé : absence d'un cycle complet valide puis défaillant ; intégration médiane reçue, erreur suivante sans ancien brut, puis hors plage et paquets vérifiés.
- `medium`, corrigé : coupure contrôlée uniquement après la boucle ; GPIO observé dans `radio.begin` et `transmit`, avec échec et reprise radio.
- Relecture indépendante finale : aucun des six constats ne subsiste ; scénarios recompilés et exécutés ensuite avec succès.
