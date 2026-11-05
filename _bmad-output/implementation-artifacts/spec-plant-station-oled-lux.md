---
title: 'Plant Station — afficher les lux sur OLED Heltec'
created: '2026-10-28'
status: 'done'
route: 'oneshot'
context: []
---

<frozen-after-approval>
## Intent

L'utilisateur signale l'écran noir après téléversement du diagnostic VEML7700. Ajouter et installer l'affichage OLED du titre Plant Station, des lux actualisés chaque seconde et d'un état explicite lorsque la mesure est indisponible. Garder le VEML7700 sur GPIO4/5 et ses sorties série. Utiliser un second contrôleur I2C pour l'OLED Heltec V3 : GPIO17/18, adresse0x3C, reset21 et Vext36 actif bas, déjà éprouvés dans PlantHubDiagnostic. L'absence ou une panne OLED ne doit pas arrêter les acquisitions ; ne pas afficher une ancienne valeur après erreur capteur. Le signalement et le téléversement précédent autorisent la correction sur cette même carte.
</frozen-after-approval>

## Implementation Notes

Deux profils compilés et tests hôte exécutés avec succès : RAM18984, flash297101 par profil. Le vrai module OLED et le vrai main sont exercés : 62 JSON contrôlés sur scénarios normal, wrap, écran absent et option PLANT_OLED=0 ; test séparé de panne/reprise, séquence GPIO36/21 et uptime 64 bits. Le BIN texte v1.0.0 est conservé dans .tools/private/plantstation-veml-v1.0.0-pre-oled-20261028.bin.

Réutiliser Adafruit SSD1306 2.5.13, activer l'affichage par défaut et prévoir une option de compilation pour le désactiver. Petit module d'affichage indépendant ; titre, une décimale en lux et uptime. Vérifier builds texte/JSON, tests natifs de rendu et d'indépendance des bus, puis téléversement et capture série attestant OLED présent et acquisitions conservées. La lisibilité physique devra être confirmée par l'utilisateur. Conserver une copie du BIN texte v1.0.0 avant remplacement ; flash complète antérieure déjà sauvegardée.

## Review Triage Log

- medium — NACK OLED désactivait définitivement le rendu : reprise bornée à 10 s ajoutée, avec réinitialisation du seul bus1 et test de retour suivi d'erreur capteur sans lux périmé.
- low — Uptime écran 32 bits repartait à zéro : compteur accumulé 64 bits réutilisé ; test du texte après wrap.
- medium — Mocks GPIO silencieux ne vérifiaient pas le câblage interne : appels GPIO/délais enregistrés et séquence Vext/reset exacte contrôlée.
- medium — Option PLANT_OLED=0 non vérifiée : second exécutable compilé avec option0, aucune commande écran/GPIO ni événement OLED, acquisitions conservées.
- low — README décrivait encore la version sans OLED : documentation actualisée, option et deux bus expliqués ; preuves v1.0.0 conservées comme historique.

Téléversement texte1.1.0 réussi ; OLED0x3C initialisé, 14 acquisitions sans erreur chaque1000ms. Rapport : diagnostics/plant-station-oled-20261028.md. Aucun travail différé, ni commit créé dans ce dépôt sans historique.
