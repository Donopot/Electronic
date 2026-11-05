# Plant Station - Sondes de pots sur balcon

Plan de réalisation V1 - 27 octobre 2026

## 1. Résultat visé et architecture

Surveiller **3 à 5 pots** et savoir lesquels vérifier puis arroser, grâce à une sonde par pot et une page consultable sur téléphone. Le matériel du dossier V2 est déclaré disponible par l'utilisateur. La couverture Wi-Fi du balcon est incertaine : les sondes transmettent donc par **LoRa 868 MHz** à un **hub Heltec V3 intérieur**, alimenté en USB et connecté au Wi-Fi domestique.

```mermaid
flowchart LR
    P[3 à 5 pots : une sonde par pot] -->|LoRa 868 MHz| H[Hub Heltec V3 intérieur / USB]
    H -->|Wi-Fi domestique| T[Téléphone / page locale]
```

Chaque sonde comprend une XIAO nRF52840, un Wio-SX1262, une sonde capacitive de terre, un ADS1115 et une batterie protégée. La radio dispense de Wi-Fi sur le balcon. Le téléphone doit pouvoir joindre le réseau local du hub pour consulter la page.

**Première livraison :** un pot qui transmet une mesure exploitable au hub et un état d'arrosage visible sur téléphone. Valider ce montage avant de reproduire les sondes SOIL-02 et SOIL-03, puis éventuellement SOIL-04 et SOIL-05.

| Décision V1 | Choix retenu |
| --- | --- |
| Fréquence des relevés | Toutes les 30 minutes, intervalle initial ajustable |
| Informations utiles | Indice d'humidité relatif, tendance, conseil, âge du relevé, batterie |
| Arrosage | Manuel ; la sonde aide à décider quand vérifier et arroser |
| Météo du dossier V2 | Étape suivante : BMP180, SHT40 et VEML7700 après validation des pots |
| Consultation | Page locale sur le Wi-Fi intérieur ; alertes affichées sur cette page |

**Nature de la mesure.** La sonde produit un signal lié à l'humidité du substrat à son emplacement. L'indice 0-100 est propre au pot : il ne représente ni un pourcentage absolu d'eau ni une quantité en litres. Le montage ne calcule pas le volume à verser.

**État réel du projet.** Le dépôt documente un diagnostic Heltec V3 en réception à 868,1 MHz. Il reste à réaliser le firmware XIAO pour le sol et l'application du hub pour les plantes. Les prototypes météo ESP32-C3 / SX1278 existants utilisent 433,92 MHz ; ils ne rejoignent pas directement cette liaison 868 MHz. Aucun assemblage ni flash matériel n'est effectué par ce plan. [L2, L3]

<!-- saut-de-page -->

## 2. Préparer et câbler la première sonde

Sortir **un seul exemplaire** de chaque élément avant d'assembler les autres. Les références exactes de la sonde capacitive et du MOSFET restent à relever sur les pièces ; la déclaration « matériel du PDF » ne précise pas leur fabricant.

| Matériel déjà prévu | Prototype | Installation 3 / 5 pots |
| --- | --- | --- |
| Kit XIAO nRF52840 + Wio-SX1262 et antenne 868 MHz | 1 | 3 / 5 |
| Sonde capacitive analogique de terre | 1 | 3 / 5 |
| ADS1115 sur module I2C compatible 3,3 V | 1 | 3 / 5 |
| LiPo protégée 3,7 V, capacité cible 1000 mAh | 1 | 3 / 5 |
| P-MOS, 220 ohms, 100 kilo-ohms, découplage | 1 lot | 3 / 5 lots |
| Boîtier, passe-câble, connecteurs, étiquette | 1 lot | 3 / 5 lots |
| Heltec WiFi LoRa 32 V3 EU868, antenne, USB 5 V | 1 | 1 hub partagé |

### Câblage proposé pour le kit Wio V1.0 à connecteurs classiques

À comparer au schéma de la révision réellement possédée avant soudure. Les repères **D6/D7 sont ceux de la XIAO**, pas des numéros GPIO ESP32. Le bus I2C doit être déplacé explicitement dans le firmware. Les broches occupées par la radio ne sont pas disponibles pour les capteurs. [S1, S2]

| Départ | Arrivée / réglage |
| --- | --- |
| ADS1115 VDD / GND | XIAO 3V3 / GND ; ajouter 100 nF si absent du module |
| ADS1115 SDA / SCL | XIAO D6 / D7 ; vérifier les résistances de rappel à 3,3 V |
| ADS1115 ADDR | GND : adresse attendue 0x48 ; ALERT/RDY inutilisé en V1 |
| Sonde GND / AOUT | GND commun / ADS1115 A0 ; résistance série possible 1 kilo-ohm |
| Sonde VCC | Drain du P-MOS ; alimentation prévue 3,3 V |
| Source / grille du P-MOS | Source sur 3V3 ; grille vers D0 via 220 ohms |
| Rappel de grille | 100 kilo-ohms entre grille et source pour l'état OFF |
| LiPo + / - | BAT+ / BAT- après contrôle de la polarité et de la cellule |

### Correction de la commande D0 du dossier V2

D0 est partagé avec le bouton du kit. **Ne pas configurer D0 en sortie haute push-pull** si le bouton la relie à la masse. Pour commander le P-MOS : **ON = sortie basse ; OFF = entrée haute impédance ou sortie à drain ouvert relâchée**, avec le rappel de grille. Au démarrage, conserver D0 relâchée. Un appui sur le bouton peut allumer la sonde ; ne pas utiliser ce bouton pour une autre fonction en V1. Contrôler hors tension la connexion du bouton sur la révision possédée. [L1, S2]

Le P-MOS doit être passant avec une commande d'environ -3,3 V et avoir un brochage vérifié. Ne pas alimenter la sonde par un GPIO. Confirmer qu'elle fonctionne réellement à 3,3 V et que son signal reste entre GND et 3V3. Pour l'ADS1115, une plage de mesure ±4,096 V ne permet pas de dépasser ses rails d'alimentation. [S3]

<!-- saut-de-page -->

## 3. Cycle de mesure, radio et alimentation

### Un cycle toutes les 30 minutes

1. Réveiller la XIAO par temporisation en mode System ON à faible consommation. Le RTC interne ne réveille pas le nRF52840 depuis System OFF. [S4]
2. Alimenter la sonde avec D0 basse ; commencer avec 300 ms de stabilisation, puis mesurer le délai réellement nécessaire.
3. Lire 15 conversions ADS1115 en mode single-shot à 128 échantillons/s, A0 par rapport à GND, plage ±4,096 V. Conserver la médiane et un indicateur de dispersion ; rejeter un défaut de bus ou une entrée hors plage.
4. Relâcher D0 pour éteindre la sonde. L'ADS1115 reste alimenté et revient au repos entre conversions. Contrôler qu'aucun autre fil ne réalimente la sonde éteinte.
5. Relever la tension de batterie avec la procédure adaptée à la révision XIAO. Vérifier notamment l'avertissement Seeed concernant P0.14 pendant lecture et recharge ; ne pas reprendre aveuglément une séquence générique d'extinction. [S1]
6. Envoyer la valeur brute, la batterie et les indicateurs de qualité au hub. Recevoir un ACK court ; autoriser au maximum deux nouvelles tentatives espacées aléatoirement, puis retourner au repos.
7. Endormir la radio, désactiver les LED et les interfaces inutiles ; attendre le cycle suivant. Mesurer la consommation avec l'USB débranché.

Les réglages ADS1115 et les délais ci-dessus sont des **points de départ de conception**, à qualifier sur le montage réel.

### Réglages communs XIAO et Heltec

| Paramètre | Valeur initiale proposée |
| --- | --- |
| Liaison | LoRa P2P privée, 868,1 MHz, sans passerelle LoRaWAN |
| Modulation | SF7, BW 125 kHz, codage 4/5 |
| Trame | En-tête explicite, préambule 8 symboles, CRC activé |
| Synchronisation | Mot privé logique 0x12 ; vérifier la traduction par les pilotes |
| Puissance d'émission | Départ à +10 dBm conduits, puissance rayonnée à contrôler |
| Identifiants | SOIL-01 à SOIL-05 ; léger décalage aléatoire des émissions |

Le paramétrage de fréquence seul ne suffit pas : modulation, synchronisation, CRC et format de paquet doivent aussi correspondre. Le diagnostic RX Heltec actuel constitue une base d'essai ; l'ACK, le décodage des sondes et leur affichage restent à développer. [L3]

**Paquet à spécifier avant développement :** version, identifiant, session de démarrage, compteur, mesure brute ADC, tension batterie en mV, état capteur et tag d'authentification. Le hub horodate la réception, rejette les paquets incorrects et déduplique les retransmissions. Les ACK identifient session et compteur. CRC et mot de synchronisation ne suffisent pas à authentifier une sonde ; prévoir une clé locale et une protection anti-rejeu. Tout chiffrement authentifié éventuel doit garantir des nonces uniques malgré les redémarrages.

Pour 868-868,6 MHz, les sources française et européenne consultées indiquent 25 mW PAR et, dans le mode retenu sans autre technique d'accès, un coefficient d'utilisation maximal de 1 %. Comptabiliser aussi les répétitions et les ACK du hub. Ce plafond concerne la puissance rayonnée avec l'antenne réelle. [S5, S6]

**Batterie.** Objectif initial proposé : un mois entre recharges, à mesurer. Estimation : autonomie en jours = capacité utile en mAh / (24 × courant moyen en mA). Une autonomie de plusieurs mois n'est pas encore établie. Recharger au sec, à l'intérieur, selon les températures autorisées par le fabricant de la cellule ; vérifier polarité et protection. Protéger la batterie du soleil et de l'eau.

<!-- saut-de-page -->

## 4. Déterminer quand arroser chaque pot

### Installation et étalonnage dans le terreau réel

1. Étiqueter le pot et la sonde. Noter plante, substrat, taille du pot, profondeur et position ; conserver cette position pendant les essais.
2. Placer la partie sensible dans une zone représentative des racines, en respectant la limite d'insertion du capteur. Éviter paroi, fond, poche d'air et arrivée directe d'eau. Garder connecteurs, batterie et électronique hors du terreau.
3. Lors d'un arrosage normal, laisser égoutter et attendre que le signal se stabilise. Enregistrer plusieurs relevés pour définir la référence humide **R_humide**.
4. Au cours du séchage normal, contrôler le terreau et le poids du pot. Quand l'arrosage devient approprié pour cette plante, relever **R_sec_utile** avant d'arroser. Ne pas attendre que la plante se flétrisse.
5. Répéter sur deux ou trois cycles, sans déplacer la sonde. Le seuil dépend de la plante et du substrat ; les plantes tolérantes à la sécheresse demandent un réglage différent. [S7]

L'essai à l'air libre sert au contrôle électrique. Il ne remplace pas ces observations en pot. Un changement de terreau, de sonde ou de position impose de refaire l'étalonnage.

### Indice relatif et seuil d'arrosage

```text
I = 100 × (R_mesure - R_sec_utile) / (R_humide - R_sec_utile)
```

Cette formule accepte une réponse croissante ou décroissante du capteur. Conserver le signal brut et l'indice non borné ; borner seulement l'affichage à 0-100. Si les références sont presque identiques par rapport au bruit mesuré, déclarer la calibration invalide au lieu de diviser ou de donner un conseil.

Avec ces références, **0 correspond au point d'arrosage observé**, et 100 à la référence humide après égouttage. Première règle proposée : déclencher « À arroser » à **I ≤ 0 sur deux relevés valides consécutifs**, espacés de 30 minutes. Ajuster ensuite le seuil par pot si les observations le demandent. Le conseil peut donc arriver 30 à 60 minutes après un franchissement durable ; adapter la cadence aux pots qui sèchent vite.

| État affiché | Règle proposée |
| --- | --- |
| À calibrer | Références absentes ou trop proches ; aucun conseil automatique |
| Défaut de sonde | Acquisition invalide ; ne pas transformer l'erreur en indice 0 |
| Données anciennes | Aucun relevé valide depuis 90 minutes ou plus ; conseil suspendu |
| À arroser | Deux nouvelles mesures sous le seuil du pot ; afficher l'heure |
| Humidité suffisante | Mesures récentes au-dessus du seuil ; réévaluer au prochain relevé |

**Débranchement du capteur analogique :** une entrée flottante peut produire une valeur plausible. Valider ce cas sur la sonde exacte ; prévoir un rappel et une plage de plausibilité adaptés si nécessaire. Sans détection fiable, afficher « mesure à vérifier » plutôt que prétendre reconnaître tous les débranchements.

Pour éviter les bascules répétées, lever l'alerte d'arrosage seulement après une nouvelle mesure dépassant le seuil de **10 points d'indice** ; marge initiale à ajuster au bruit. Réinitialiser la confirmation après une erreur ou une longue interruption. « Batterie faible » est un avertissement indépendant, basé sur la cellule et sa chute de tension mesurées.

Après arrosage, vérifier la remontée du signal sur les relevés suivants. Si elle manque, contrôler position, câblage et répartition de l'eau avant de recommander un nouvel arrosage. Marquer « J'ai arrosé » dans l'interface reste une action manuelle ; un simple changement de signal ne prouve pas à lui seul un arrosage.

<!-- saut-de-page -->

## 5. Page téléphone et travail logiciel

### Une carte par plante

Exemple fictif d'affichage, sans mesure réelle :

| Basilic / SOIL-01 | Géranium / SOIL-02 | Pot 3 / SOIL-03 |
| --- | --- | --- |
| À arroser | Humidité suffisante | Données anciennes |
| Indice 0/100, en baisse | Indice 63/100, stable | Indice actuel indisponible |
| Reçu il y a 5 min | Reçu il y a 12 min | Dernier relevé il y a 2 h |
| Batterie 3,85 V | Batterie 3,91 V | Vérifier sonde / batterie |

L'interface affiche en priorité le **conseil et la fraîcheur** du relevé, puis l'indice, sa tendance et la batterie. Elle explique l'indice relatif ; elle ne présente pas les valeurs comme un pourcentage absolu d'eau. Afficher la tension de batterie, plutôt qu'un pourcentage de charge non caractérisé.

Fonctions V1 : nommer les pots, enregistrer les deux références, ajuster le seuil, consulter une courbe et la date du dernier arrosage déclaré, exporter les valeurs brutes. Viser sept jours d'historique à 30 minutes : **336 mesures par sonde**, soit **1680 pour cinq pots**. Dimensionner le stockage selon la taille réelle des enregistrements et grouper les écritures en flash.

Les alertes sont visibles quand on consulte la page. Une notification sur téléphone verrouillé ou hors domicile demanderait un service supplémentaire ; elle ne fait pas partie de cette première réalisation.

### Lots de développement à réaliser

| Lot | Contenu concret | Validation attendue |
| --- | --- | --- |
| F1 - Sonde locale | Profil XIAO, I2C D6/D7, ADS1115, commande D0 relâchable, journal brut | Mesure répétable et alimentation sonde réellement coupée |
| F2 - Liaison | Pilote SX1262, paramètres communs, trame versionnée, compteur, ACK bornés, authentification | Valeur brute identique au départ et à l'arrivée ; doublons filtrés |
| F3 - Hub plantes | Réception continue, fiches par identifiant, horodatage, calibration, sauvegarde | SOIL-01 distinguée de SOIL-02 ; paramètres conservés au redémarrage |
| F4 - Page locale | Cartes, graphique, états, réglages, export, accès Wi-Fi intérieur | Consultation et étalonnage depuis un téléphone |
| F5 - Autonomie | Temporisation RTC, sommeil radio/ADC, LED, mesure batterie, surveillance erreurs | Réveils autonomes et courant moyen mesuré sur batterie |

Les chemins futurs proposés sont `firmware/PlantSoilNode/` et `firmware/PlantHub/`, avec protocole partagé. Ils restent à créer lors du développement. Le diagnostic Heltec existant sert de référence matérielle et radio ; il ne fournit pas encore la page plantes. Les seuils de péremption de la station météo actuelle ne conviennent pas aux nouvelles sondes espacées de 30 minutes. [L2, L3]

Les réglages d'écriture doivent être protégés. Garder le hub sur le réseau domestique et conserver un export des références. Au redémarrage sans heure fiable, signaler l'incertitude temporelle et attendre une nouvelle réception avant de présenter une ancienne valeur sauvegardée comme actuelle.

<!-- saut-de-page -->

## 6. Ordre de fabrication et critères de passage

Les étapes suivent les preuves obtenues. Le délai dépend surtout des cycles naturels de séchage ; sept jours d'essais ne remplacent pas deux ou trois arrosages observés.

| Étape | Action | On passe à la suite quand... |
| --- | --- | --- |
| P0 - Atelier | Identifier les révisions, broches, polarités et antennes ; étiqueter SOIL-01 | Le câblage est documenté et contrôlé hors tension |
| P1 - Radio seule | Émettre une trame de test XIAO vers Heltec, d'abord au bureau puis au balcon | Des trames complètes sont reçues avec paramètres identiques |
| P2 - Mesure locale | Brancher ADS1115 et sonde ; tester alimentation, extinction et signal | Humide/sec se distinguent et l'état OFF ne réalimente pas la sonde |
| P3 - Mesure reçue | Transmettre brut, batterie et défauts ; vérifier ACK et déduplication | Le hub reçoit les bonnes valeurs et signale un défaut simulé |
| P4 - Téléphone | Ajouter page, deux références et états ; tester une coupure Wi-Fi | La page fonctionne sur le réseau intérieur et les données anciennes sont signalées |
| P5 - Un pot dehors | Monter le boîtier, calibrer, suivre les arrosages et mesurer le courant | Mesures utiles sur plusieurs cycles, protection et autonomie acceptables |
| P6 - Trois pots | Reproduire deux sondes, chacune avec son identifiant et sa calibration | Les trois pots donnent des états indépendants et cohérents |
| P7 - Cinq pots | Ajouter deux sondes après la campagne à trois pots | Pas de confusion d'identifiants ni de perte durable supplémentaire |

### Essais essentiels avant duplication

- **Électricité :** polarité, 3V3 stable, sortie de sonde dans les rails, ADS1115 à 0x48 ; D0 relâchée au repos et bouton sans conflit de sortie.
- **Fiabilité radio :** campagne proposée de 48 h au placement réel, fenêtre/porte dans la situation habituelle, puis sept jours avec boîtiers. À 30 minutes, 48 h représentent environ 96 cycles par sonde. Comparer aux journaux TX ; objectif initial proposé d'au moins 95 % de cycles livrés après répétitions, sans trou de 90 minutes. Ce sont des objectifs à vérifier.
- **Pannes :** couper une sonde, débrancher le capteur, envoyer un doublon, redémarrer le hub et couper le Wi-Fi. Une absence de donnée ne doit jamais apparaître comme « terre sèche » ou comme une mesure actuelle.
- **Protection extérieure :** électronique fixée au-dessus du terreau, boîtier à l'ombre, passe-câble vers le bas et boucle anti-ruissellement. Tester d'abord le boîtier vide avec un témoin sec, puis surveiller la condensation en service. Un boîtier assemblé n'acquiert pas automatiquement un indice IP certifié.
- **Autonomie et utilité :** mesurer sommeil, réveil et pics radio ; vérifier la remontée après arrosage et le seuil propre à chaque plante. Objectif initial d'un mois entre recharges, à confirmer par mesure.

**Premier montage à préparer :** une XIAO + Wio-SX1262 avec son antenne 868 MHz, un ADS1115, une sonde capacitive et le Heltec sur USB. Commencer les essais sur alimentation USB au bureau ; intégrer ensuite la batterie et la commande d'économie, avant les essais extérieurs. Photographier et noter le câblage avant toute duplication.

<!-- saut-de-page -->

## 7. Fiche de suivi et références

### À remplir pour chaque pot

| Champ | Valeur à consigner |
| --- | --- |
| Identifiant / nom du pot / espèce | SOIL-__ / ... / ... |
| Carte et révision / référence sonde / MOSFET | ... / ... / ... |
| Terreau, taille du pot, position et profondeur | ... |
| Batterie : capacité, protection, polarité vérifiée | ... |
| Référence humide après égouttage / date | ... points ADC / ... |
| Référence sec utile avant arrosage / date | ... points ADC / ... |
| Seuil d'alerte / marge / intervalle | ... / ... / ... |
| Bruit mesuré / délai de stabilisation | ... / ... |
| Courant moyen / estimation d'autonomie | ... mA / ... jours |
| Réception au balcon / test boîtier | ... / ... |
| Cycles d'arrosage observés / décision de duplication | ... / ... |

### Sources locales

- **[L1]** Dossier fourni : `C:/Users/Anwender/Downloads/Plant_Station_LoRa_Plan_Complet_V2.pdf`, édition du 23 octobre 2026. Base matérielle et projet global ; les étapes ci-dessus recentrent la première réalisation sur 3 à 5 pots.
- **[L2]** État du dépôt : `docs/project-overview.md`, daté du 27 octobre 2026. Distingue les essais 433,92 MHz et le diagnostic Heltec 868,1 MHz.
- **[L3]** Diagnostic du hub : `firmware/PlantHubDiagnostic/README.md` et `platformio.ini`. Réception et veille documentées ; aucun firmware de sonde de terre attesté.

### Documentation publique consultée le 27 octobre 2026

- **[S1] Seeed - XIAO nRF52840.** Brochage, variantes de cœur Arduino, mesure batterie et avertissement P0.14 : [documentation constructeur](https://wiki.seeedstudio.com/XIAO_BLE/).
- **[S2] Seeed - kit Wio-SX1262.** [Présentation du kit](https://wiki.seeedstudio.com/xiao_nrf52840%26_wio_SX1262_kit_for_meshtastic/) et [schéma Wio V1.0](https://files.seeedstudio.com/products/113010003/Wio-SX1262%20for%20XIAO%20V1.0.pdf). Comparer avec les cartes possédées.
- **[S3] Texas Instruments - ADS1115.** Alimentation, limites d'entrée, plage programmable et conversions single-shot : [fiche technique](https://www.ti.com/lit/ds/symlink/ads1115.pdf).
- **[S4] Nordic - alimentation du nRF52840.** Modes System ON/OFF et réveils : [spécification POWER](https://docs.nordicsemi.com/r/bundle/ps_nrf52840/page/power.html).
- **[S5] Légifrance - décision ARCEP 2023-1412.** Tableau des dispositifs à courte portée : [texte français](https://www.legifrance.gouv.fr/jorf/article_jo/JORFARTI000048119817).
- **[S6] Union européenne - décision 2025/105.** Annexe, ligne 48, 868-868,6 MHz : [texte européen](https://eur-lex.europa.eu/legal-content/EN/TXT/PDF/?uri=CELEX%3A32025D0105).
- **[S7] Royal Horticultural Society.** Contrôle d'humidité des pots et différences entre plantes : [guide d'arrosage des contenants](https://www.rhs.org.uk/container-gardening/how-to-water-containers).

Les règles d'alerte, les durées de stabilisation et les objectifs de campagne sont des propositions de ce plan. L'autonomie, l'étanchéité et la justesse du conseil d'arrosage seront établies sur les pièces et les pots réels.
