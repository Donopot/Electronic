# Nouvel essai d'humidité — 3 novembre 2026, 15 h 53

L'utilisateur confirme une première position dans la terre humide, puis une
seconde dans l'air après retrait de la terre sur la lame. Aucun changement
de firmware ni de câblage effectué par l'agent pendant cet essai.

| Condition | Durée | Mesures | Médiane A0 | Minimum–maximum |
| --- | --- | --- | --- | --- |
| Terre humide | 30 s | 30 | 1,910625 V | 1,909250–1,912625 V |
| Air | 30 s | 29 | 1,915250 V | 1,914625–1,916375 V |

59 conversions, aucune erreur, couverture valide. COM8, adresse 0x49.
Les captures débutent à 15 h 53 min 06 s et 15 h 54 min 28 s, heure de Paris.

L'écart des médianes air–humide est de **4,625 mV**. Il reste inférieur à
l'écart de 8,313 mV observé entre les deux relevés humides du premier essai.
Une petite différence est donc observée, mais elle ne suffit pas à établir
une réponse reproductible à l'humidité avec les données disponibles.
La médiane des cinq premiers points humides est 1,909500 V contre
1,911750 V pour les cinq derniers, montrant aussi une évolution pendant
une même capture.

La communication et la lecture ADC sont fonctionnelles ; le bon fonctionnement
de la sonde d'humidité reste à confirmer. Aucun pourcentage n'est calculé.
Le prochain contrôle utile porte sur la référence du capteur, son alimentation
et le raccordement AOUT–A0, avec une photo lisible puis, si disponible,
une mesure au multimètre. Ces données ne prouvent pas une panne du capteur.

Preuves :

- `humidite-reessai-humide-20261103-155306.log` et `.json`.
- `humidite-reessai-air-20261103-155428.log` et `.json`.
- `humidite-reessai-20261103-155306.json` : statistiques et conclusion.
- [Premier essai](humidite-test-20261103.md).

Dernier état confirmé : sonde laissée dans l'air ; XIAO sur diagnostic USB.
