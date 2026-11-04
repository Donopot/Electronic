---
title: 'Ajouter SOIL-03 et afficher les trois sondes'
type: 'feature'
created: '2026-11-04'
status: 'done'
route: 'oneshot'
review_loop_iteration: 0
context: []
---

<frozen-after-approval reason="troisième montage branché et préparation confirmée par l'utilisateur">

## Intent

L'utilisateur a branché sa troisième sonde, montage complet identique aux deux
premières avec AO3401 et rappel 10 kΩ. Antenne raccordée et batterie isolée
confirmées ; SOIL-01 et SOIL-02 restent alimentées sur batterie. Installer la
nouvelle carte comme SOIL-03 et étendre la réception et l'affichage du Heltec
aux trois sondes sans reprogrammer les deux premières.

Conserver PS v1 de vingt octets, réglages radio, broches et commandes P-MOS.
Accepter les nœuds 1 à 3, rejeter les autres ; conserver un état par nœud,
ses sessions, compteurs, défauts et péremptions sans croisement. Les cycles
restent espacés de trente minutes et les mesures périment individuellement
après quatre-vingt-dix minutes. Premiers relevés à 30/35/40 s pour SOIL-01/02/03.
Ce décalage ne garantit pas la livraison et n'ajoute ni ACK ni répétition.
L'ancien programme des deux premières sondes reste compatible.

L'écran 128×64 affiche en même temps lux, NON AUTH, trois lignes identifiées
SOIL-01/02/03 avec tension arrondie à trois décimales et âge en minutes, puis
état radio. ATTENTE, DEFAUT ADS ou PERIMEE remplacent la tension du seul nœud
concerné. Les bruts et tensions complètes restent dans les journaux USB.
Conserver les contrôles PRG et les profils historiques lux/JSON.

Créer le profil XIAO `xiao_soil_lora_pmos_node3`, identifiant 3 par compilation,
sans changer l'identifiant par défaut des profils existants. Étendre codec,
captures, tests du vrai firmware et vérification des ELF : appariement par
nœud/session/compteur, minimum par nœud, trafic des autres sondes enregistré
séparément, identifiant attendu vérifié au contrôle matériel.

Compiler les cinq profils XIAO et les trois Heltec, conserver les applications
disponibles, puis installer uniquement la nouvelle XIAO et le Heltec. Contrôler
une première paire réelle SOIL-03, l'identité USB, le mode AUTO et l'absence
de répétition à l'ancienne cadence ; observer les premières sondes si elles
émettent pendant cette fenêtre. Distinguer ces observations des tests à
horloge simulée et de l'affichage testé. Aucun pourcentage d'humidité,
étalonnage, mesure d'autonomie ou nouvelle veille matérielle ajoutés.

</frozen-after-approval>

## Implementation Notes

- Base : `f16a5d95a406bba9f3b10494d94fb8dd90118e85`. Les deux rapports historiques déjà modifiés et les autres fichiers non suivis préexistants restent hors de ce changement, dans la continuité autorisée du projet.
- Finalité unique, extension limitée de la banque existante et du profil 2 vers le profil 3 ; aucun choix produit manquant, aucune opération irréversible ajoutée : route oneshot. Le passage à une ligne par sonde est nécessaire pour trois valeurs sur 128×64, précision complète conservée sur USB.
- Nouvelle carte COM12, VID/PID 239A:810B, numéro USB `537B36EB9D8F46F1`. Vérifier ce numéro avant DFU. SOIL-01 `4A1D3D9FBC2F6C9C` et SOIL-02 `2E1B165998772BC5` ne sont pas reflashées. Heltec COM5 ; sauvegardes locales prévues avant remplacement.
- Réutiliser `ReceiverBank` et ses tableaux dimensionnés par `kNodeCount`, porter la borne à 3 et les regex de capture à 01/02/03. Adapter les tests d'identifiant inconnu à 4, ajouter indépendance du troisième, ses défauts/reprises et sa présence avec trafic de fond 1/2.
- XIAO : profil additionnel, délai initial 40 s ; tester les trois profils P-MOS avec le même vrai code et des échéances indépendantes du calcul de production. Vérifier les cinq ELF avec le contrôle batterie existant.
- OLED : sept lignes aux ordonnées 0/9/18/27/36/45/54, troisième ligne NON AUTH, puis les trois sondes et la radio. Contrôler coordonnées, police, largeur et hauteur, états individuels et valeurs arrondies sans perte des bruts USB.
- Documentation : nouveau guide trois sondes, anciens guides pointant vers cette extension, exemples compilant les cinq profils avant tests ELF. Revue indépendante puis validation matérielle avant statut done.
- Préparation terminée : 49 scénarios hôte, 23 tests de capture, trois tests batterie sur cinq ELF et huit compilations réussis. Un ancien test de capture a exposé une échéance murale de 30 ms fragile sous compilation concurrente ; horloge rendue déterministe, échec initial expliqué et preuves finales dans `diagnostics/soil03-20261104.md`. Bootloader de la nouvelle carte identifié sur COM13 ; sauvegarde Heltec conservée.
- Relecture finale : aucun constat restant. Heltec installé, application seule de 352 112 octets à 0x10000, hash vérifié ; SOIL-03 application seule de 92 596 octets via COM13, retour COM14. Capture 105,063 s acceptée : paire exacte du nœud 3, session 534338323, séquence 1, 2,712 V, brut 21696, ADS 0x48, sans défaut/perte observée, 105 lux valides et aucune répétition pendant les 92,703 s suivant la première. Bannière SOIL-03 / premier 40 s et AUTO grille relâchée vérifiés séparément. Aucun paquet 1/2 dans cette fenêtre ; compatibilité et indépendance testées en simulation, sans reflash des deux premières. Bilan `diagnostics/soil03-validation-20261104.json`, rapport `diagnostics/soil03-20261104.md`. SOIL-03 laissée en AUTO/USB, batterie isolée. Intent réalisé.

## Review Triage Log

- `medium`, corrigé : délai de 30 ms dépendant de l'ordonnancement dans le test de deux lecteurs ; horloge simulée expire seulement après consommation des deux entrées. Suite finale passée.
- `low`, corrigé : preuve d'échec initial sans explication ; compte rendu distingue l'échec initial de la correction et du journal final à 23 tests réussis.
- `medium`, corrigé : frontière précise de péremption SOIL-03 absente de l'intégration ; assertions juste avant/à 90 min, autres états inchangés.
- `medium`, corrigé : compteur circulaire du troisième non entrelacé ; trois nœuds à FFFFFFFF puis zéro, rejet ancien préservant valeur/âge, nouvelle session d'un seul nœud vérifiés.
- `medium`, corrigé : minimum de correspondances du troisième non exercé ; nœuds 1/2 complets mais troisième insuffisant refusés, puis trois complets acceptés.
- `medium`, corrigé : rejet attribué SOIL-03 non exercé ; ignoré comme trafic de fond et conservé comme erreur lorsque le nœud 3 est suivi.
- `low`, corrigé : message PASS deux sondes pour scénario trois ; message exact trois sondes dans le journal final.
- `medium`, corrigé : géométrie non vérifiée aux limites ; valeurs -0,001/3,300 V, âge 89m, lux en cours/erreur et coordonnées/police/bornes contrôlés.
- `low`, corrigé : documentation affichant MESURE PERIMEE au lieu de PERIMEE ; guides corrigés.
- `low`, corrigé : variante documentée commune à trois profils au lieu de cinq ; compte corrigé.
- `false` : compilations Heltec et tests ELF manquaient pendant la revue car ils étaient encore en cours, sans déclaration d'installation prête ; les trois profils Heltec et cinq ELF sont maintenant vérifiés avant installation, journaux dans le rapport.
