---
title: 'Test LoRa de la sonde XIAO vers le Heltec'
type: 'feature'
created: '2026-11-03'
status: 'done'
route: 'dispatch'
baseline_commit: '6292c5ac654e7850631a88d28644bbcd7ff7d836'
review_loop_iteration: 0
context: []
---

<frozen-after-approval reason="intention autorisée par l'utilisateur">

## Intent

Les deux cartes sont branchées et les deux antennes 868 MHz sont confirmées.
Transmettre la mesure brute de SOIL-01 depuis le XIAO au Heltec et l'afficher
sur son OLED, avec preuve que la valeur reçue correspond exactement à l'USB.
La sonde est dans l'air au démarrage. Conserver lux, OLED et PRG/veille du
Heltec. Installer et tester les firmwares après sauvegarde des applications.

## Boundaries & Constraints

Toujours : diagnostic autonome sur USB ; D0 XIAO relâchée ; ADS1115 D6/D7,
adresse unique 0x48–0x4B ; aucun pourcentage sans étalonnage. Profils actuels
Heltec texte/JSON conservés, nouveau profil radio optionnel. Erreurs capteur,
radio et données anciennes explicites ; aucune valeur ancienne présentée
comme actuelle. Les erreurs radio n'interrompent ni lux ni PRG.

Ce premier test démontre le transport : sans conseil d'arrosage, authentification,
ACK, page Web ou optimisation batterie. Les trames diagnostic sont identifiées
comme non authentifiées. Pas d'effacement usine ni de changement du chargeur.

## I/O & Edge-Case Matrix

| Cas | Résultat attendu |
| --- | --- |
| Mesure valide | Même session, compteur et brut aux deux extrémités ; volts dérivés du brut |
| Trame courte/longue, version/noeud/statut inconnu, brut incompatible | Rejet sans rafraîchir mesure ni âge |
| Défaut ADS explicite | Réception du défaut ; valeur indisponible |
| Doublon/compteur ancien | Rejet sans rafraîchir l'âge |
| Nouvelle session XIAO | Nouvelle mesure acceptée après redémarrage |
| Aucun nouveau paquet depuis 90 s | Affichage périmé remplaçant la valeur actuelle |
| Débordement millis | Cadence, ordre et âge corrects par soustraction non signée |
| PRG long puis relâché | Sommeil radio, capteur et OLED ; RST reprend les fonctions |

</frozen-after-approval>

## Code Map

- `firmware/XiaoSoilDiagnostic/src/main.cpp` : bus ADS borné et journal USB
  éprouvés ; conserver le profil actuel et créer un profil TX optionnel.
- `firmware/PlantStationVeml7700/src/main.cpp`, `src/display/OledDisplay.*`,
  `src/power/SleepPower.cpp` : lux à 1 s, deux bus I2C, PRG puis sommeil.
- `firmware/PlantHubDiagnostic/src/main.cpp` : référence RX SX1262 validée.
- `firmware/PlantStationVeml7700/test/run_host_tests.py` : vrais pilotes et
  orchestration, schéma JSON strict ; préserver les validations existantes.

## Tasks & Acceptance

- [x] `lib/PlantSoilProtocol/` : codec binaire portable, format strict et état
  de réception ; tests hôte des cas de la matrice, session, ordre et péremption.
- [x] `firmware/XiaoSoilDiagnostic/` : profil LoRa TX, journal brut inchangé,
  paquet courant toutes les 30 s, même en erreur ADS ; aucun rattrapage en rafale.
- [x] `firmware/PlantStationVeml7700/` : profil `heltec_v3_soil_lora`, RadioLib
  7.8.1, réception serviceable, OLED lux + SOIL-01 brut/volts/âge/état, sommeil radio.
- [x] Tests ciblés protocoles et écran/états ; tests Heltec existants ; compiler
  profils XIAO diagnostic/TX et Heltec texte/JSON/radio.
- [x] `tools/capture_soil_lora.py` : capture simultanée bornée, fichiers distincts,
  comparer exactement session/compteur/brut des TX et RX, signaler pertes/erreurs.
- [x] Documenter profils, branchements, protocole et limites ; conserver les preuves.
- [x] Agent principal uniquement : sauvegarder, installer, vérifier au moins
  trois paquets appariés, lux concurrents, OLED, PRG/veille/RST et retour réception.

Acceptation : étant donné les deux cartes avec antennes sur USB, quand SOIL-01
émet, alors le Heltec affiche sa tension actuelle et les captures confirment
au moins trois mesures identiques. Étant donné une absence/erreur, quand la
mesure est invalide ou périmée, alors l'écran remplace la valeur et garde les lux.

## Implementation Notes

Décisions : trame compacte à longueur fixe, entiers encodés explicitement ;
version, SOIL-01, session de démarrage, séquence, uptime, statut ADC, adresse
et brut. Tension = brut × 125 µV. Session obtenue par RNG matériel avec attente
bornée sur nRF52840. Pas de confiance de sécurité associée à ces identifiants.
Un changement de session invalide la mesure précédente ; doublons et anciens
compteurs de la session courante rejetés. 868,1 MHz, BW125, SF7, CR4/5,
sync privé 0x12, CRC, préambule 8 ; puissance TX 0 dBm ; pas de répétition/ACK.
Contrôler le temps d'antenne pour rester sous 1 % à 30 s (texte UE 2025/105).

XIAO : SPI D8/D9/D10, NSS D4, DIO1 D1, RESET D2, BUSY D3 ; D5 RXEN avec
`setRfSwitchPins(5, RADIOLIB_NC)`, DIO2 TX, TCXO 1,8 V. Appeler `SPI.begin()`
explicitement avec la surcharge Module recevant SPI ; ce core ne touche pas
SS/D7. Heltec : SCK9/MISO11/MOSI10/NSS8/RESET12/BUSY13/DIO1=14, TCXO1,8 V,
DIO2 RF switch. Réessais radio espacés et bornés, pas de boucle infinie.

Seul l'agent principal accède aux cartes, sauvegarde/flash et valide le matériel.
L'agent d'implémentation produit le code et ses validations logicielles.
Arbre initial : majorité non suivie, deux fichiers du diagnostic précédent
modifiés ; conserver ces états. Restreindre le diff/relecture aux fichiers de
ce changement, comparer les fichiers existants non suivis à une copie préalable.

Implémentation logicielle reçue : 23 fichiers, manifeste et diff contrôlés
contre les copies prises avant intervention, cinq profils compilés, tests hôte
historiques et nouveaux exécutés avec succès. Relecture avant installation ;
preuve matérielle encore à obtenir. Diff de revue :
`C:/Users/Anwender/AppData/Local/Temp/tmptvfl7rc4-soil-lora-review.diff`.

Validation finale : cinq profils compilés, suite hôte historique, neuf scénarios
firmware et treize tests de capture réussis. Quatre TX/RX identiques en105s,
105 lux valides à1s, aucun défaut/perte. OLED confirmé par l'utilisateur.
Veille et RST vérifiés sur carte ; réception reprise sans moniteur XIAO.
Les deux cartes restent actives. Rapport : `diagnostics/soil-lora-20261103.md`.

## Verification

Tests purs protocoles : paquet valide et faute, longueurs, champs, doublons,
changement de session, compteur circulaire, dépassement millis, frontière 90 s.
Tests Heltec existants et affichage des nouveaux états. Compiler tous les profils
mentionnés. Validation matérielle du principal et compte rendu dans `diagnostics/`.

## Spec Change Log

## Review Triage Log

- B1 high / patch : RadioLib 7.8.1 `findChip()` effectue dix `reset(true)`
  avec attente de 1 s, depuis la boucle principale. Isoler l'initialisation
  pour conserver lux/PRG pendant l'échec et protéger la transition sommeil.
- B2 high / patch : TinyUSB `write()` boucle tant que DTR est vrai et le FIFO
  plein ; les diagnostics XIAO précèdent le service TX. Journalisation bornée.
- B3 medium / patch : KeyboardInterrupt n'est pas Exception ; joindre les
  lecteurs avant fermeture et conserver un rapport interrompu explicite.
- B4 medium / patch : les deux limites de capture peuvent séparer un couple
  TX/RX valide. Horodatages hôte et fenêtre pleinement observable nécessaires.
- B5 low / patch : limite 8192 testée uniquement sur le reliquat ; contrôler
  aussi chaque ligne complète avant son écriture.
- B6 medium / patch : ouverture finale JSON en écrasement après précontrôle
  laisse une course entre invocations. Réserver la sortie exclusivement.
- B7 medium / patch : remplacement silencieux d'UTF-8 invalide peut cacher
  un préfixe corrompu. Garder les octets et signaler l'erreur de capture.
- B8 medium / patch : aucun test n'exécute le vrai TX XIAO ; sa cadence et son
  comportement USB ne sont donc protégés que par compilation. Ajouter ce test.
- E1 medium / patch : longueur lue avant gel RX, puis payload pouvant changer.
  Passer en standby avant longueur/en-tête/données puis reprendre RX.
- E2 medium / patch : CRC configuré ne garantit pas sa présence dans un
  en-tête LoRa explicite. Contrôler `getLoRaRxHeaderInfo` et rejeter sans CRC.
- E3 low / patch : conversion Python d'un entier de plus de 4300 chiffres
  lève ValueError avant le résumé ; classer la ligne comme malformée.
- E4 low / patch : même défaut de ligne complète que B5 ; correction commune.
- E5 medium / patch : même interruption non gérée que B3 ; correction commune.
- E6 medium / patch : même course de résumé que B6 ; correction commune.
- V1 medium / patch : manque de vérification TX pré-vérifié par la couche de
  vérification ; même racine que B8. Tester le vrai ordonnanceur et ses fautes.
- V2 medium / patch : le test CRC injecte un doublon déjà rejeté par le
  compteur ; utiliser un nouveau compteur et un brut distinct pour prouver CRC.
- V3 medium / patch : les tests de comparaison n'exercent pas `main` ni la
  propagation des erreurs de capture vers JSON/code retour ; ajouter ce cas.

Tous les constats concernent l'implémentation et les preuves du test autorisé.
Corrections transmises ensemble à l'agent initial, sans extension fonctionnelle
publique ni changement de l'intention. Aucune action différée à ce stade.

Résolution finale : tous les constats corrigés et tests exécutés ; les trois
couches de relecture ont confirmé leurs corrections. Initialisation privée
avec publication atomique, sommeil sans concurrence, CDC borné4ms, captures
horodatées et erreurs explicites. Aucune action différée.
