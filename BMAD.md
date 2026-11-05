# BMAD appliqué à Electronic

Date : 27 octobre 2026. Parcours : projet existant, remise à plat de la connaissance
et planification d'améliorations incrémentales.

BMAD officiel **6.12.4**, modules `core` et `bmm`, est installé dans `_bmad/`.
Les 29 skills Codex sont dans `.agents/skills/`. Les échanges et les documents
sont configurés en français, y compris dans la surcharge d'équipe
`_bmad/custom/config.toml`. Les paramètres personnels sont ignorés par Git.

Sources de la méthode : [dépôt officiel](https://github.com/bmad-code-org/BMAD-METHOD),
[branche stable utilisée](https://github.com/bmad-code-org/BMAD-METHOD/tree/V6.12).
La version est fixée pour rendre cette adoption reproductible.

## Livrables et ordre de lecture

| Étape | Livrable | Résultat |
| --- | --- | --- |
| Analyse | [État du projet](docs/project-overview.md) | Fonctions, preuves, limites et sujets à préciser |
| Produit | [PRD](./_bmad-output/planning-artifacts/prd.md) | Exigences reconstituées et objectifs proposés |
| Architecture | [Architecture](./_bmad-output/planning-artifacts/architecture.md) | Composants, contrats et décisions de conservation |
| Planification | [Epics et stories](./_bmad-output/planning-artifacts/epics.md) | Deux epics, six stories avec critères d'acceptation |
| Vérification | [Stratégie de test](./_bmad-output/planning-artifacts/test-strategy.md) | Contrôles logiciels et campagnes matérielles distinctes |
| Passage au développement | [Readiness](./_bmad-output/planning-artifacts/implementation-readiness.md) | Évaluation technique et dépendances externes |
| Suivi | [Sprint](./_bmad-output/implementation-artifacts/sprint-status.yaml) | État produit par le script officiel BMAD |
| Premier résultat | [Story 1.1](./_bmad-output/implementation-artifacts/1-1-socle-bmad-et-reference-verifiee.md) | Adoption et référence vérifiées |
| Preuves du jour | [Validation](./_bmad-output/implementation-artifacts/validation-2026-10-27.md) | Tests et trois compilations |

Les exigences existantes sont déduites du dépôt, pas d'un entretien produit.
Les priorités futures et les seuils de campagne sont des propositions de travail,
pas des engagements déjà approuvés. Aucun nouveau besoin Plant Station n'est inventé.

## Utilisation dans Codex

Dans un chat ouvert sur ce projet, invoquer le skill par son nom :

- `bmad-help` : s'orienter à partir des documents et de l'état du projet.
- `bmad-sprint-planning` : lire ou rafraîchir le suivi.
- `bmad-build` : réaliser une story précise, en commençant par la story 1.2.
- `bmad-review` : examiner une réalisation et ses preuves.
- `bmad-retrospective` : tirer les enseignements à la fin d'un epic.

Les skills nouvellement installés peuvent nécessiter un nouveau chat pour être
chargés par Codex. Exemple de prochaine demande :
« Utilise bmad-build pour la story 1.2 de `_bmad-output/planning-artifacts/epics.md`. »

Pour régénérer le suivi depuis la racine :

```powershell
$stamp = [TimeZoneInfo]::ConvertTimeBySystemTimeZoneId([DateTimeOffset]::UtcNow, 'Romance Standard Time').ToString('MM-dd-yyyy HH:mm')
uv run .agents/skills/bmad-sprint-planning/scripts/sprint_plan.py generate --epic-file _bmad-output/planning-artifacts/epics.md --status-file _bmad-output/implementation-artifacts/sprint-status.yaml --stories-dir _bmad-output/implementation-artifacts --project Electronic --date $stamp
uv run .agents/skills/bmad-sprint-planning/scripts/sprint_plan.py validate --status-file _bmad-output/implementation-artifacts/sprint-status.yaml
```

Le générateur préserve les statuts existants. L'heure du suivi est celle de Paris.
Les journaux matériels existants conservent leur horodatage d'origine.

## Cadence de travail

Pour chaque changement : choisir une story, vérifier ses prérequis, lire ses
sources, réaliser le changement, exécuter les vérifications pertinentes, conserver
les preuves et mettre à jour son statut. Une story matérielle reste ouverte
tant que sa campagne n'a pas été exécutée. Une compilation seule ne valide
ni la portée LoRa, ni le fonctionnement sur batterie.

Les fonctionnalités observées avant l'adoption ne sont pas recréées comme des
stories fictivement terminées. Seule la story 1.1 clôt le travail réalisé ici.
La suite du backlog est proposée ; elle n'est pas exécutée par cette adoption.

## Réinstallation

Sur un poste disposant de Node.js, npm et uv :

```powershell
npx bmad-method@6.12.4 install --directory . --modules bmm --tools codex --communication-language French --document-output-language French --output-folder _bmad-output --no-shims --yes
```

Sur ce poste, npm n'était pas disponible dans le runtime initial. Le paquet a été
installé avec le pnpm fourni par le runtime dans `.tools/bmad-bootstrap/`, puis
son installateur officiel a été exécuté. Ce répertoire de téléchargement reste
ignoré ; `_bmad/`, les skills et les livrables constituent l'installation du projet.

Le dépôt n'a pas encore de commit. Les fichiers initiaux ont été conservés et
aucun commit, téléversement sur carte ou essai matériel n'a été effectué ici.
