---
title: 'Ajouter la deuxième sonde SOIL-02 au Heltec'
type: 'feature'
created: '2026-11-04'
status: 'done'
route: 'oneshot'
review_loop_iteration: 0
context: []
---

<frozen-after-approval reason="montage de la deuxième sonde annoncé et préparation confirmée par l'utilisateur">

## Intent

L'utilisateur a assemblé et branché une deuxième sonde identique à la première :
XIAO nRF52840, Wio-SX1262, ADS1115, sonde capacitive et AO3401 sur D0 avec rappel
10 kΩ. Il confirme antenne raccordée et batterie déconnectée pour l'installation
USB ; SOIL-01 reste alimentée sur batterie. Installer ce montage comme SOIL-02
et permettre au Heltec de recevoir et afficher les deux sondes séparément.

Conserver la trame v1 de vingt octets et son octet de nœud existant : accepter
uniquement les identifiants 1 et 2, encoder l'identité réelle dans chaque paquet
et chaque journal. Le Heltec conserve par nœud la session, le compteur, la
valeur et son âge ; un nouveau paquet, un défaut, un rejet ou une expiration
d'une sonde ne modifie pas l'autre. L'ancien programme SOIL-01 reste compatible
sans reflash. Les nœuds inconnus et les paquets invalides restent rejetés.

Garder trente minutes entre cycles et quatre-vingt-dix minutes de péremption
individuelle. SOIL-01 conserve son premier relevé à 30 s ; SOIL-02 commence à
35 s pour décaler les départs si les cartes démarrent ensemble. Ne pas promettre
de garantie de livraison : ni ACK ni répétition ne sont ajoutés. Conserver les
broches, le PHY, le contrôle D0 sans HIGH, la médiane/stabilisation, les commandes
USB et la variante batterie. Un profil dédié sélectionne SOIL-02 ; les profils
existants gardent SOIL-01 par défaut.

Sur l'OLED, conserver lux, état radio et indication NON AUTH. Montrer les deux
identifiants avec leur tension et leur âge en minutes, ou leur état individuel
ATTENTE / DEFAUT ADS / MESURE PERIMEE. Les bruts restent disponibles dans les
journaux USB. Aucun pourcentage ou nouvel étalonnage n'est introduit.

Les captures comparent par nœud/session/compteur et traitent les réceptions
valides des autres nœuds comme trafic de fond séparé, sans fausse perte ni
appariement croisé. Conserver les preuves brutes, les rejets et erreurs.
Vérifier le codec compatible, l'indépendance des états/compteurs/expirations,
l'affichage et les scénarios de capture avec les vrais modules hôte ; compiler
les profils affectés, sauvegarder les applications disponibles, installer le
Heltec et uniquement la nouvelle XIAO. Contrôler réellement SOIL-02 et sa
réception, puis observer SOIL-01 si elle émet pendant la fenêtre de contrôle,
en distinguant les preuves matérielles des scénarios simulés.

</frozen-after-approval>

## Implementation Notes

- Base : `d474aa0209f2fbdb33776e56edca3d7cf995c07f`. Les deux rapports historiques modifiés et autres fichiers non suivis préexistants sont conservés hors de ce changement, dans la continuité autorisée du projet.
- Finalité unique, pas de décision produit manquante ni d'opération irréversible ajoutée ; extension de l'octet de nœud déjà présent et de l'affichage existant. Route oneshot. Aucun reflash SOIL-01 prévu.
- Nouvelle XIAO : COM9 initial, VID/PID 239A:810B, numéro USB `2E1B165998772BC5`, distinct de SOIL-01 `4A1D3D9FBC2F6C9C`. Lecture passive 4 s sans octet reçu ; touch 1200 réussit, bootloader Seeed 2886:0045 sur COM10. Pas de volume amovible observé ; sauvegarde série privée dans `.tools/private/soil02-initial-20261104/`. Heltec et binaire SOIL-01 sauvegardés dans `.tools/private/before-soil02-20261104/`.
- Protocole : `lib/PlantSoilProtocol/src/PlantSoilProtocol.h`, ajouter identité dans Packet et validation 1/2, banque de récepteurs isolés, journaux dynamiques. Même disposition sur l'air, v1 conservée ; le numéro existant de SOIL-01 est inchangé.
- XIAO : `platformio.ini` et `src/main.cpp`, identifiant de compilation par défaut 1 et profil `xiao_soil_lora_pmos_node2`, délai initial 35 s pour le second. Étendre les tests du vrai code au profil 2 et les vérifications ELF/variante à quatre profils.
- Heltec : `SoilRadio.*`, `main.cpp` et `OledDisplay.*`, router vers la banque et rafraîchir les deux états ; deux lignes par sonde sur 128×64, volts/âge, lux et radio conservés. Les défauts masquent seulement la tension de la sonde concernée.
- Capture : recherche déléguée en lecture seule, terminée ; clef `(node, session, seq)`, nœuds suivis déduits des TX USB, autres RX valides enregistrés séparément. Préserver les critères stricts pour les nœuds suivis et les erreurs non attribuables. Ajouter les cas de sessions/compteurs identiques entre sondes et de trafic de fond.
- Vérifier les limites 30/90 min et la sécurité GPIO existantes, les paquets SOIL-01 historiques, l'acceptation SOIL-02 et le rejet SOIL-03, les pertes ciblées et l'absence de rafraîchissement de l'autre nœud. Documenter la nouvelle cible et la disposition OLED avant revue indépendante et installation.
- Réalisation et revue terminées : 35 scénarios hôte, 20 tests de capture, trois tests batterie/ELF et sept compilations réussis. Dix constats corrigés, relecture sans constat restant. Heltec installé (352 160 octets, hash vérifié), puis application seule SOIL-02 (92 596 octets) sur COM10, retour COM11 ; aucun reflash SOIL-01. Capture de démarrage en cours avec première paire exacte SOIL-02 à 2,698 V. Rapport et empreintes dans `diagnostics/soil02-20261104.md`.
- Validation finale : capture 105 s acceptée, `compared_nodes == [2]`, une paire TX/RX exacte SOIL-02, session 3231110779, séquence 1, brut 21584, 2,698 V, ADS 0x48, sans erreur/perte observée et 105 lux valides. Aucune répétition pendant 95,796 s après la première. Bannière USB distincte SOIL-02 / premier 35 s et AUTO grille relâchée vérifiés. Aucun paquet SOIL-01 durant cette courte fenêtre ; la compatibilité et l'indépendance sont prouvées par tests, sans prétendre avoir observé les deux cartes dans cette fenêtre. Bilan `diagnostics/soil02-validation-20261104.json`, rapport `diagnostics/soil02-20261104.md`. Batterie SOIL-02 isolée, USB et AUTO laissés actifs ; SOIL-01 inchangée sur batterie. Intent réalisé.

## Review Triage Log

- `medium`, corrigé : rejets doublon/ancien compteur sans nœud, faux échec possible en trafic de fond ; nœud validé ajouté à ces deux rejets et filtre de capture limité aux rejets attribués des autres nœuds. Les rejets non attribuables restent des erreurs.
- `medium`, corrigé : erreur TX dépourvue d'identité ; helper interne de journal avec SOIL_NODE_ID, contexte de nœud ajouté aux états P-MOS, assertions sur erreurs des deux profils. Format historique des lignes ADC conservé.
- `low`, corrigé : recette de vérification ne compilant que le profil 2 avant inspection de quatre ELF ; commande complète des quatre profils documentée.
- `low`, corrigé : README Heltec adjacent décrivant encore l'ancien affichage unique brut/volts ; description remplacée et portée lux texte/JSON clarifiée.
- `medium`, corrigé dans le protocole de vérification : accepter un profil par défaut sur la mauvaise carte ne doit pas valider l'identité SOIL-02 ; exiger la bannière SOIL-02 et `compared_nodes == [2]` dans le contrôle matériel, documenté explicitement.
- `medium`, corrigé : minimum de correspondances global à tous les nœuds ; comptes `valid_matched_by_node`, minimum vérifié individuellement, test quatre correspondances totales insuffisantes lorsqu'elles sont réparties deux par nœud.
- `medium`, corrigé : double OLED ignorant coordonnées/police ; géométrie capturée et sept rangées exactes, largeur, hauteur, police et séparation des deux valeurs vérifiées.
- `medium`, corrigé : absence de reprise radio avec deux états peuplés ; échec de relance, reprise, sessions/compteurs et âges conservés vérifiés dans le vrai chemin main/radio/OLED.
- `medium`, corrigé : défaut SOIL-02 non exercé dans l'intégration ; défaut puis reprise ajoutés, valeur SOIL-01 conservée.
- `medium`, corrigé : compteur circulaire/rejet ancien non entrelacés dans la banque ; cas indépendants des deux nœuds, âges non rafraîchis par les rejets et nouvelle session d'un seul nœud vérifiés.
