# Essai d'humidité — 3 novembre 2026

## Résultat

**La lecture ADC fonctionne ; la mesure d'humidité n'est pas validée par
cet essai.** Les conditions ont été confirmées par l'utilisateur après chaque
manipulation. Aucun firmware ni câblage modifié pendant la comparaison.

| Condition | Durée de capture | Mesures | Médiane A0 | Minimum–maximum |
| --- | --- | --- | --- | --- |
| Terre humide, premier relevé | 20 s | 20 | 1,906625 V | 1,906250–1,907750 V |
| Sonde dans l'air | 30 s | 29 | 1,910000 V | 1,908500–1,911000 V |
| Retour dans la terre humide | 30 s | 30 | 1,914938 V | 1,909000–1,917125 V |

**79 conversions, aucune erreur de communication**, couverture temporelle
valide dans les trois captures. Port COM8, adresse 0x49, canal A0/GND,
plage ±4,096 V, pas de conversion 125 µV.

## Interprétation

L'écart entre l'air et le premier relevé humide est de +3,375 mV. Le second
relevé humide diffère du premier de +8,313 mV : il ne retrouve pas la référence
initiale. Dans l'air, la médiane des cinq premiers points est 1,908625 V,
contre 1,910375 V pour les cinq derniers, montrant une évolution pendant
la capture. Ignorer les dix premiers points des captures de 30 secondes ne
rétablit pas le retour à la référence humide : médianes 1,910125 V dans l'air
et 1,915500 V après remise en terre.

Ces observations ne permettent pas d'établir une séparation reproductible
entre air et terre humide. Elles ne prouvent pas que la sonde est défectueuse :
la position, le contact avec le sol, un temps de stabilisation plus long,
des résidus humides, l'alimentation et le raccordement analogique restent
à examiner. La référence exacte de la sonde n'est pas connue. Aucun seuil
constructeur de sensibilité n'est appliqué et aucun pourcentage d'humidité
n'est calculé.

Pour poursuivre le diagnostic, identifier la sonde et vérifier sur une photo
le chemin VCC/GND/AOUT et le raccordement à A0. Une mesure au multimètre de
VCC et d'AOUT par rapport à GND permettrait ensuite de comparer le signal
réel de la sonde à celui reçu par l'ADC. Ne pas modifier les fils sous tension.

## Preuves

- `humidite-reference-20261103-154447.log` et `.json` : terre humide initiale,
  début 15 h 44 min 47 s, heure de Paris.
- `humidite-air-20261103-154549.log` et `.json` : air, début 15 h 45 min 49 s.
- `humidite-retour-humide-20261103-154747.log` et `.json` : retour humide,
  début 15 h 47 min 47 s.
- `humidite-comparaison-20261103.json` : statistiques calculées depuis les
  trois captures, conditions et conclusion conservées.

Le XIAO est laissé sur le diagnostic USB existant. Le dernier état physique
confirmé est la sonde remise dans la terre humide.
