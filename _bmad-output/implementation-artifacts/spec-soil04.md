---
title: 'Ajouter SOIL-04 et afficher les quatre sondes'
type: 'feature'
created: '2026-11-04'
status: 'done'
route: 'oneshot'
review_loop_iteration: 1
context: []
---

<frozen-after-approval reason="quatrième montage branché et préparation confirmée par l'utilisateur">

## Intent

L'utilisateur a branché un quatrième montage identique, AO3401/rappel 10 kΩ,
antenne raccordée et batterie isolée pour l'installation USB. Les trois
premières sondes restent sur batterie. Installer la nouvelle carte comme
SOIL-04, mettre à jour le Heltec pour les quatre sondes et conserver les
programmes des trois premières sans reflash.

Garder PS v1 de vingt octets, PHY, câblage, sécurité D0 et commandes USB.
Accepter les nœuds 1 à 4, rejeter les autres ; conserver sessions, compteurs,
valeurs, défauts et péremptions indépendants. Cycles de trente minutes et
expiration individuelle à quatre-vingt-dix minutes inchangés. Premiers cycles
à 30/35/40/45 s pour SOIL-01/02/03/04, sans garantie de livraison ni ACK ajouté.
Le profil dédié `xiao_soil_lora_pmos_node4` sélectionne l'identifiant 4 ; les
profils existants gardent leur identité et leur délai initial.

Le Heltec 128×64 affiche huit lignes de huit pixels : titre/PRG, lux, NON AUTH,
quatre sondes, puis état radio. Chaque ligne de sonde contient identifiant,
tension à trois décimales et âge en minutes, ou ATTENTE / DEFAUT ADS / PERIMEE.
Garder bruts et précision complète sur USB, contrôles PRG et profils lux/JSON.

Étendre tests du codec, de la banque, du vrai radio/main/OLED et des captures
au quatrième : indépendance, compteur circulaire, rejet ancien, défaut/reprise,
frontière exacte de péremption, minimum par nœud, trafic de fond et rejets
attribués. Vérifier la géométrie jusqu'à la dernière ligne et avec les valeurs
extrêmes. Compiler six profils XIAO et trois Heltec, inspecter les six ELF pour
la variante batterie, puis sauvegarder les applications disponibles et
installer uniquement le Heltec et la nouvelle XIAO identifiée par son USB.

Exiger une première paire TX/RX réelle du nœud 4, `compared_nodes == [4]`, sa
bannière SOIL-04 / premier 45 s et son mode AUTO. Observer l'absence de
répétition à l'ancienne cadence et les autres sondes si elles émettent dans
la fenêtre. Distinguer les observations matérielles des tests simulés ; ne
pas revendiquer un étalonnage d'humidité, une période réelle complète de
trente minutes, une consommation ou autonomie mesurées, ni une nouvelle veille.

</frozen-after-approval>

## Implementation Notes

- Base : `685da119ee398927717c6428f8dbb50ed6bc2c54`. Les deux rapports historiques déjà modifiés et les autres fichiers non suivis préexistants restent hors du changement, dans la continuité autorisée du projet.
- Extension limitée de la banque existante et du profil 3, finalité unique, aucun choix produit manquant ni opération irréversible ajoutée : route oneshot. Quatre valeurs tiennent sur huit lignes à police 6×8 ; aucune pagination nécessaire.
- Nouvelle carte COM15, VID/PID 239A:810B, numéro USB `0690EC20846A82A8`. Vérifier cette identité avant DFU ; Heltec COM5. Aucun reflash des trois premières sondes.
- Réutiliser ReceiverBank et les tableaux dimensionnés par kNodeCount, porter la borne à 4 et les regex à 01–04 ; tester l'identifiant inconnu 5. La formule de délai initial existante donne 45 s au quatrième.
- Créer le profil `xiao_soil_lora_pmos_node4` depuis le profil P-MOS de base. Tester les quatre profils P-MOS, les six ELF et les recettes complètes de compilation ; conserver les profils historiques.
- Répartir les lignes OLED à 0/8/16/24/32/40/48/56, hauteur 8, largeur bornée. Étendre les tests d'états, d'expiration et de reprise aux quatre, y compris compteur circulaire et capture avec les trois autres comme trafic de fond.
- Nouveau guide quatre sondes et renvois depuis les anciens, documentation des délais et profils exacte. Revue indépendante et validation matérielle avant statut done.

## Review Triage Log

- 1 — low / patch : renvoi radio encore à deux nœuds ; corrigé à quatre dans `docs/soil-lora.md`.
- 2 — low / patch : recette locale incomplète ; variables d’environnement ajoutées au guide quatre, même PlatformIO local pour les deux compilations, chemin esptool séparé pour l’installation.
- 3 — medium / patch : démarrage court impossible à reproduire avec le minimum CLI de trois ; recette capture 105 s puis comparaison interne `minimum=1`, état complet/sans erreur et identité `[4]` exigés ; limite de cadence explicitée.
- 4 — medium / patch : profil DFU non explicite ; paquet `node4`, numéro USB, VID/PID et port à vérifier documentés, application Heltec seule à 0x10000.
- 5 — low / patch : succès P-MOS indifférenciés ; étiquette profil/scénario ajoutée avant chaque exécution, 66 étiquettes dans le journal final.
- 6 — low / patch : provenance des résultats absente ; manifeste commun avec révision de base complète, commandes, empreintes des sources/binaries et journaux.
- 7 — medium / patch : bannière non vérifiée par les tests ; nouveau scénario pour les quatre profils vérifiant identité, délai affiché, AUTO, grille OFF et absence de TX initial.
- 8 — medium / patch : débordement du compteur 4 limité à la banque ; vrai radio/main/OLED vérifie FFFFFFFF→0, nouvelle valeur, rejet ancien et âge préservé.
- 9 — medium / patch : défaut 4 remplacé avant sa péremption ; vrai radio/main/OLED vérifie DEFAUT ADS à 90 min−1 ms puis PERIMEE à 90 min, les trois autres rafraîchis et isolés.
- 10 — medium / patch : trafic de fond limité aux succès ; test capture inclut défauts ADC et rejets doublon/ancien des trois autres, douze éléments conservés sans invalider SOIL-04.

- Vérifications finales logicielles : 66 scénarios hôte, 26 tests de capture, 3 contrôles batterie des six ELF ; six compilations XIAO et trois Heltec réussies. Manifeste `diagnostics/soil04-verification-manifest-20261104.json`. Les seuls derniers changements après compilation concernent tests, outils de test et documentation.

- Revue des corrections terminée : dix constats initiaux résolus, aucun restant ni différé.
- Installation application seule réussie Heltec COM5 et SOIL-04 COM16→COM17 (même numéro USB), trois premières non reflashées. Première ouverture COM17 trop précoce refusée puis capture suivante réussie avant le premier cycle.
- Validation matérielle acceptée : une paire node4/session3214405458/seq1, ADS0x48, brut21664, 2,708000 V, bannière45s/AUTO, grille relâchée avant TX, 105 lux valides, aucun nouveau TX durant102,625s. Aucun autre nœud observé dans105,094s ; attendre leurs prochains cycles. Pas de cadence réelle complète ni d’autonomie revendiquée. Compte rendu `diagnostics/soil04-20261104.md`.
