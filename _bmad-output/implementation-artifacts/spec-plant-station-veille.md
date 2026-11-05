---
title: 'Plant Station — restaurer la veille PRG sans perdre OLED et luminosité'
created: '2026-10-28'
status: 'done'
route: 'oneshot'
context: []
---

<frozen-after-approval>
## Intent

L'utilisateur a perdu la fonction de veille après remplacement du diagnostic Heltec RX. Restaurer son fonctionnement précédent dans le firmware Plant Station actuel : PRG maintenu 3 secondes puis relâché déclenche la veille profonde, appui court sans effet, RST avec PRG relâché redémarre. Conserver OLED et mesures VEML7700. Afficher la demande de relâcher PRG puis arrêter proprement acquisitions, capteur, écran et LED ; maintenir les broches d'extinction pendant le sommeil. Aucun réveil automatique ni réveil par PRG seul. Réutiliser la politique de l'ancien PlantHubDiagnostic sans initialiser la radio pour ce diagnostic. Installer et vérifier sur la même carte autorisée.
</frozen-after-approval>

## Implementation Notes

Première compilation texte/JSON et tests réussis. Revue indépendante : ajustements de deux bornes du bouton (premier appui pendant initialisation et relâchement à exactement3s), ajout des variantes capteur absent au boot/bus non initialisé et des erreurs de relecture ALS_SD. Vérification RST de bout en bout prévue sur la carte réelle via tools/check_plant_station_sleep.py.

Bouton anti-rebond40ms, armé uniquement après relâchement initial, échéances résistantes au wrap. Sommeil VEML7700 via ALS_SD et readback, sans bloquer le sommeil en cas de capteur absent ; OLED OFF et Vext coupé, SX1262 maintenu en RESET sans initialisation radio, LED éteinte, GPIO tenus puis libérés au redémarrage. Tests natifs du bouton et des séquences, builds texte/JSON, vérification matérielle DTR(PRG)/RTS(RST) avec silence en veille et reprise des lux/OLED. Garder le BIN précédent. Ne pas prétendre avoir mesuré la consommation.

## Review Triage Log

- medium — Premier appui ignoré si aucun tour relâché après begin : armement d'après l'état relâché échantillonné au démarrage, test ajouté.
- medium — Durée3s décalée par debounce et relâchement prioritaire : départ au début de l'appui accepté, durée gelée au début du relâchement ; tests3s exact et2999ms.
- false — Absence de contrôle bout en bout du retour RST : le script matériel ajouté après lancement de la revue réalise sommeil, PRG seul puis RST, vérifie OLED et nouvelles mesures ; son exécution est requise avant livraison.
- medium — Absence au boot et échec bus non couverts : deux scénarios exécutables ajoutés au vrai main.
- medium — Relecture shutdown non couverte : bit resté clair, lecture courte et request échoué testés, sommeil du hub toujours exigé.
- low — Documentation interaction manquante pendant revue : section PRG/RST et commande de vérification ajoutées, protocole manuel complété.

Vérification finale : builds et tests natifs PASS, upload1.2.0 réussi, test matériel DTR/RTS PASS (court/long/release, sommeil capteur, silence, PRG seul, RST et reprise OLED/lux). Carte laissée active. Preuve : diagnostics/plant-station-veille-20261028.md. Aucun travail différé ni commit.
