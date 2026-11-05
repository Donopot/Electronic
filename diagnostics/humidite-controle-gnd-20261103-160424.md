# Réponse de la sonde confirmée — 3 novembre 2026, 16 h 04

## Résultat

**Une réponse nette au passage de l'air à la terre humide est observée.**
Le convertisseur acquitte maintenant **0x48**, adresse attendue avec ADDR
reliée à la masse. 59 conversions ont été reçues sans erreur.

| Condition confirmée par l'utilisateur | Mesures | Médiane A0 | Minimum–maximum | Code brut médian |
| --- | --- | --- | --- | --- |
| Air | 29 | 2,695625 V | 2,693625–2,701375 V | 21565 |
| Terre humide | 30 | 1,060375 V | 1,058875–1,060750 V | 8483 |

Écart des médianes : **1,635250 V**. Les plages observées sont nettement
séparées. La médiane humide est stable sur les 30 secondes de capture ;
la mesure dans l'air présente une évolution de quelques millivolts,
très inférieure à l'écart entre les deux conditions.

## Contexte et limites

Ce nouvel essai suit l'explication du raccordement commun des masses du XIAO,
de l'ADS1115, de la sonde et d'ADDR. L'adresse était précédemment 0x49 et les
mesures restaient autour de 1,91 V dans les différentes conditions.
Le nouveau scan trouve uniquement 0x48. Le recâblage exact n'est pas documenté
par une photo : ces observations ne suffisent pas à identifier précisément
le défaut électrique initial.

La réponse physique air/terre humide est confirmée pour cet essai. Aucun
pourcentage absolu d'eau, seuil d'arrosage ni indice étalonné du pot n'est
établi. La référence dans l'air ne constitue pas une référence de terre
sèche utile pour l'arrosage. Les essais antérieurs sont conservés comme
historique du diagnostic et ne servent pas d'étalonnage.

Firmware inchangé : diagnostic USB, COM8, A0/GND, plage ±4,096 V,
128 échantillons/s, pas de conversion 125 µV. D0 reste relâchée et aucune
transmission radio n'est initialisée. Les deux captures ont une couverture
temporelle valide et durent chacune 30 secondes.

## Preuves

- `humidite-air-apres-controle-gnd-20261103-160424.log` et `.json` : début
  à 16 h 04 min 24 s, heure de Paris.
- `humidite-humide-apres-controle-gnd-20261103-160521.log` et `.json` : début
  à 16 h 05 min 21 s.
- `humidite-controle-gnd-20261103-160424.json` : statistiques de comparaison,
  réponse observée et absence d'étalonnage explicitement enregistrées.

Les fichiers JSON de capture gardent leur champ automatique
`physical_sensor_validated: false` : une capture isolée ne connaît pas le
milieu réel de la sonde. C'est la comparaison des deux conditions confirmées
par l'utilisateur qui établit ici la réponse physique.

Dernier état confirmé : sonde dans la terre humide ; XIAO sur diagnostic USB.
