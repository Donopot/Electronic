# Validation de référence du 27 octobre 2026

Exécution locale sur les sources présentes. Aucun firmware source modifié
pour l'adoption BMAD ; aucun téléversement ou accès série.

## Tests existants

| Contrôle | Résultat | Portée |
| --- | --- | --- |
| `node tests/dashboard_state_test.cjs` | PASS, code 0 | Données distantes, limite de péremption, mode local et perte réseau |
| Compilation `tests/compensation_test.cpp` | PASS, code 0 | Assertions de compensation et rejets BMP180 |
| Compilation `tests/lora_register_validation_test.cpp` | PASS, code 0 | Assertions des registres attendus et des configurations rejetées |

Compilateur RISC-V local, options `-std=gnu++14 -Wall -Wextra -Werror -c`.
Les C++ sont des tests par assertions constantes, sans binaire exécuté.

## Compilations PlatformIO

| Cible | Résultat | RAM utilisée | Application utilisée |
| --- | --- | --- | --- |
| `enviroterminal_local_web_meteo_tx` | SUCCESS | 63 220 octets | 1 566 826 octets / 2 097 152 |
| `firmware/WeatherReceiver` | SUCCESS | 62 956 octets | 1 553 322 octets / 2 097 152 |
| `firmware/PlantHubDiagnostic` | SUCCESS | 21 068 octets | 350 829 octets / 3 342 336 |

Plateforme Espressif32 6.12.0 ; builds séquentiels avec les outils de `.tools/`.
Le premier lancement restreint ne pouvait pas lire le paquet PlatformIO local ;
la relance avec les permissions nécessaires a réussi pour les trois projets.

## Installation et cohérence BMAD

Installateur officiel 6.12.4 terminé avec succès, core et bmm, intégration Codex.
Le générateur officiel du sprint et sa validation structurale ont réussi :
`valid=true`, aucun problème, aucune entrée illégale ou non reconnue.
Le suivi compte deux epics et six stories : une terminée et cinq en backlog.
La recommandation officielle est de commencer la story 1.2 avec `bmad-build`.
La configuration effective résout `French` pour les échanges et les documents.
Les liens locaux de neuf documents ont été vérifiés : aucun lien cassé.

## Ce que ces résultats ne mesurent pas

État des cartes installées, stabilité radio durable, portée, autonomie,
courant de veille, lisibilité du LCD et connexion actuelle des appareils.
Les rapports matériels du 26 octobre restent des preuves historiques.
