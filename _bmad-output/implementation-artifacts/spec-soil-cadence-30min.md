---
title: 'Espacer les relevés SOIL-01 de trente minutes'
type: 'feature'
created: '2026-11-04'
status: 'done'
route: 'oneshot'
review_loop_iteration: 0
context: []
---

<frozen-after-approval reason="changement de cadence demandé par l'utilisateur">

## Intent

L'utilisateur demande un relevé d'humidité toutes les trente minutes au lieu
d'environ trente et une secondes. Appliquer cette cadence au profil P-MOS
installé sur la XIAO et conserver la réception et l'affichage sur le Heltec.
Le premier relevé automatique reste disponible trente secondes après le
démarrage ; les cycles suivants attendent trente minutes après la fin du
traitement précédent. La sonde reste coupée pendant cette attente. Le garde
radio commun passe aussi à trente minutes ; les commandes manuelles de
diagnostic conservent leurs règles, AUTO pouvant demander une acquisition
immédiate sans contourner le garde d'émission.

Le Heltec doit conserver une mesure courante entre deux réceptions normales :
porter sa péremption de quatre-vingt-dix secondes à quatre-vingt-dix minutes,
soit trois périodes nominales. Les paquets rejetés ne rafraîchissent toujours
pas l'âge et un défaut ADC ne réutilise pas une ancienne valeur. Conserver
le format radio, les réglages PHY, les broches, le contrôle partagé D0,
la médiane, la stabilisation et les commandes OFF/ON bornées. Ce changement
de cadence n'implémente pas une nouvelle veille matérielle et ne constitue
pas une mesure d'autonomie.

Vérifier les frontières de trente minutes et de quatre-vingt-dix minutes,
l'absence de rafale après une pause et le débordement de millis par les
tests du vrai code avec horloge simulée, puis compiler les profils affectés.
Sauvegarder les applications et installer les deux profils radio convenus,
avec la batterie XIAO déconnectée avant passage dans son bootloader.
Vérifier un premier échange TX/RX et l'absence de répétition à l'ancienne
cadence ; distinguer cette courte preuve matérielle du test temporel simulé.

</frozen-after-approval>

## Implementation Notes

- Base : `97ed6b3d11d45ff832e3bbfe5f697fd777bfcf52`. Les deux rapports de diagnostic déjà modifiés et les autres fichiers non suivis restent hors de ce changement, conformément au travail précédemment autorisé.
- L'ancien cache BMAD était inaccessible ; son dossier parent a été conservé sous `_bmad/render/bmad-build/electronic-db4ace1a006a.backup-20261104-095849`, puis le workflow a été régénéré. Aucun fichier de source du workflow modifié.
- Une seule finalité, aucun choix produit manquant ni action irréversible : route oneshot. Premier relevé après 30 s conservé pour éviter une attente initiale de 30 min ; péremption 90 min cohérente avec les trois périodes de l'existant et le plan balcon.
- Code : `lib/PlantSoilProtocol/src/PlantSoilProtocol.h` porte cadence/péremption communes ; `firmware/XiaoSoilDiagnostic/src/main.cpp` utilise ce garde pour le cycle P-MOS et l'émission, avec délai initial désormais distinct. Aucun changement du codec ni du pilotage du MOSFET.
- Tests : adapter les échéances des vrais TX/P-MOS/Heltec et du récepteur partagé ; vérifier explicitement les nouvelles valeurs, les frontières avant/à échéance, les rejets sans rafraîchissement et les compteurs circulaires. Réutiliser `tools/run_soil_lora_host_tests.py` et y intégrer le test partagé du protocole.
- Documentation : mettre à jour `docs/soil-pmos.md`, `docs/soil-lora.md` et la présentation du profil XIAO. Conserver les comptes rendus historiques et ajouter un rapport daté pour ce changement.
- Matériel connu avant préparation : XIAO COM8, Heltec COM5, antennes confirmées ; l'utilisateur avait confirmé USB et batterie ensemble. Ne pas flasher la XIAO avant confirmation de batterie isolée. Les programmes actuels sont conservés pour retour arrière.
- L'outil de capture était limité à 600 s, insuffisant pour trois cycles à 30 min : borne portée à 7200 s sans changer ses critères d'acceptation ni son interface ; tests de trois relevés espacés et des limites 3700/7200 s, rejet au-delà. La capture matérielle courte utilise le comparateur existant avec un minimum d'une paire, explicitement annoncé comme contrôle de démarrage.
- Vérification après corrections de revue : 22 scénarios hôte, 15 tests de capture, trois tests de variante batterie et six compilations réussis. Applications sauvegardées, binaires et empreintes dans `diagnostics/soil-30min-20261104.md`. Installation XIAO dépend encore de la confirmation de batterie isolée.
- Relecture finale : les cinq constats sont résolus, aucun report. Heltec installé, application seule à 0x10000, 351 600 octets, hash vérifié ; contrôle 40 s avec 40 lux valides et un paquet SOIL-01 valide de l'ancien programme XIAO. Nouveau programme XIAO non installé : confirmation de batterie isolée toujours attendue. Prochaine étape : ouverture COM8 à 1200 bauds puis fermeture pour entrer au bootloader COM7, `adafruit-nrfutil.py dfu serial -pkg <ZIP préparé> -p COM7 -b 115200 --singlebank` (application seule), puis capture du premier échange et contrôle de silence au-delà de l'ancienne cadence. Ne pas déclarer le changement terminé avant cette installation et sa vérification.
- Validation finale : utilisateur confirme deux USB, antennes et batterie isolée. ZIP préparé vérifié, application seule de 92 448 octets installée par DFU COM7, retour COM8. Capture 105,094 s : une paire exacte TX/RX, session 4029017078, séquence 1, premier relevé à uptime 31 126 ms, ADS 0x48, brut 21502, 2,687750 V ; aucun défaut/perte observée, 105 lux valides et aucune répétition pendant les 99,750 s suivantes. Le contrôle de bannière initial a été perdu par la purge de capture ; lecture USB séparée confirme la bannière 30 min et AUTO grille relâchée. Preuves originales conservées et bilan combiné accepté dans `diagnostics/soil-30min-validation-20261104.json`. AUTO laissé actif, batterie isolée. Les frontières 30/90 min sont testées avec horloge simulée, sans revendiquer un cycle matériel complet de trente minutes. Intent réalisé, statut `done`.

## Review Triage Log

- `medium`, corrigé : capture CLI par défaut de 105 s insuffisante pour trois paquets ; défaut porté à 5500 s, test avec départ juste après une émission et borne 7200 s maintenue.
- `medium`, corrigé : test du premier délai dérivé de `lastPowerCycle` aurait accepté une régression vers 30 min au démarrage ; échéance testée à partir de l'horloge enregistrée avant le vrai setup.
- `low`, corrigé : heures hôte demi-horaires et uptimes simulés courts incohérents ; uptimes alignés et écarts 1 801 000 ms vérifiés.
- `medium`, corrigé : absence de test des commandes AUTO répétées avec le nouveau garde ; scénario dédié, acquisitions immédiates, aucun TX avant 30 min, émission reprise à la frontière.
- `low`, corrigé : documentation mêlant preuves historiques et nouvelle cadence ; 22 scénarios/15 tests actuels et rapport daté ajoutés, références du 3 novembre explicitement historiques.
