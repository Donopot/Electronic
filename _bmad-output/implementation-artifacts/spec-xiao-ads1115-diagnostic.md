---
title: 'Diagnostic USB XIAO et ADS1115'
type: 'feature'
created: '2026-11-03'
status: 'done'
route: 'oneshot'
review_loop_iteration: 0
context: []
---

<frozen-after-approval reason="intention utilisateur">

## Intent

Vérifier le montage réalisé par l'utilisateur : ADS1115 alimenté en 3,3 V,
SDA sur XIAO D6, SCL sur D7, ADDR à GND, sonde analogique sur A0.
Installer si nécessaire un diagnostic USB autonome, relever les réponses I2C
et les conversions brutes de A0. Distinguer la présence d'un périphérique I2C,
une conversion valide et la validation physique de la sonde.

</frozen-after-approval>

## Implementation Notes

- Autorisation : « fais le » après proposition explicite du test I2C et A0.
- Le dépôt initial est entièrement non suivi et sans commit ; conserver tous
  les fichiers existants et limiter les nouveaux fichiers au diagnostic.
- Le firmware USB observé ressemble à Meshtastic. Identifier et sauvegarder
  sa configuration dans `.tools/private/` avant remplacement ; sauvegarder
  le contenu courant via le disque UF2 si ce chargeur l'expose.
- Nouveau projet autonome prévu : `firmware/XiaoSoilDiagnostic/` ; ne pas
  changer les profils ESP32 et Heltec existants. D0 et la radio restent au repos.
- Lecture single-shot A0/GND, plage ±4,096 V, 128 échantillons/s, délais
  bornés. Signaler un bus bloqué, NACK, timeout et mesure hors des rails 0–3,3 V.
- Aucun pourcentage d'humidité sans étalonnage physique. La réussite logicielle
  ne prouve ni le brochage du MOSFET ni le bon contact de la sonde dans le sol.
- Préférer les commandes et sources officielles ; garder les captures dans
  `diagnostics/`. Un besoin d'intervention sur RESET est signalé si nécessaire.
- Premier test matériel : le scan acquitte uniquement 0x49, pas 0x48.
  Adapter la sélection à un unique candidat 0x48–0x4B et signaler l'écart ADDR,
  sans modifier le câblage. Si plusieurs candidats répondent, ne rien configurer.
- Le module USBPrinting de Cura retenait COM8 après une première capture.
  Diagnostic confirmé par son journal ; l'utilisateur a fermé Cura et libéré
  le port. Aucun paramètre Cura modifié.
- Validation finale : 25 conversions sur 25 secondes, aucune erreur,
  A0 de 1,906250 à 1,907875 V, codes 15250–15263. Le contrôle des registres
  est compatible avec un ADS1115 ; le type exact et la réaction physique
  à l'humidité demandent toujours le modèle et un essai dans le substrat.
- Le XIAO reste sur le diagnostic USB, port COM8. Meshtastic 2.6.2.31c0e8f
  et son export sont conservés dans `.tools/private/`. Aucune transmission
  LoRa, batterie ni P-MOS testés dans cette opération.
- Compilation Seeed réussie (55 956 octets flash, 7 148 octets RAM), syntaxe
  Python vérifiée ; le garde-fou refusant un journal `.json` a été exécuté.

## Review Triage Log

- medium — Messages perdus lors d'une ouverture USB tardive : corrigé par
  réannonce à chaque nouvelle connexion observée ; bannière reçue au test final.
- medium — Collision journal/résumé si `--output` finit en `.json` : argument
  refusé avant ouverture du fichier, vérifié par exécution.
- medium — Résumé ancien possible après interruption : exceptions série,
  système, ligne trop longue et conversion numérique enregistrées dans un
  résumé courant avec `completed: false`. Cas COM8 indisponible vérifié.
- medium — Succès possible avec une seule mesure puis silence : couverture
  et délai maximal de cinq secondes contrôlés ; mesure malformée rejetée.
  Capture réelle : délai maximal 1,063 seconde, couverture valide.
- low — Quelques codes négatifs près de zéro peuvent venir de l'offset :
  libellé distinct pour huit codes maximum et absence d'affirmation sur VDD réel.
- medium — Expression numérique permissive pouvant accepter `1..2` avant
  échec de conversion : expression décimale stricte et capture de ValueError.
- Tous les constats retenus ont été corrigés ; aucune action différée.
