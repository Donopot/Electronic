---
title: 'Préparer le premier essai batterie de SOIL-01'
type: 'bugfix'
created: '2026-11-03'
status: 'done'
route: 'oneshot'
review_loop_iteration: 0
context: []
---

<frozen-after-approval reason="intention utilisateur : installer la batterie du plan">

## Intent

Le premier essai batterie du XIAO nRF52840 doit conserver la mesure ADS1115 et la transmission LoRa existantes. Le cœur Seeed sélectionné met actuellement READ_BAT_ENABLE/P0.14 à HIGH ; la FAQ constructeur déconseille cet état pendant la recharge. Corriger cette initialisation à sa source dans une variante locale au projet, avec LOW préchargé avant la sortie, et conserver le réglage de recharge faible courant, environ 50 mA annoncé par Seeed. Ne pas modifier les paquets globaux, le bootloader, le protocole ni le Heltec. Vérifier les deux profils XIAO, le binaire lié et la transmission USB avant de guider le raccordement BAT−/BAT+ confirmé sur la photo. Ne pas présenter cette correction comme une validation de la cellule, de sa protection ou de la recharge réelle ; le bootloader et l'intervalle antérieur à l'application ne sont pas couverts.

</frozen-after-approval>

## Implementation Notes

- Base Git : `39748c6abefd3bd9f2d2b1ff70e90982d51fcc2d`. L'état préexistant du dossier (diagnostics, documents et prototypes non liés) est conservé conformément à la continuité de la session ; seuls les fichiers de cette correction seront enregistrés.
- Investigation : le constructeur nRF PlatformIO prend en charge `board_build.variants_dir`. Copier la variante Seeed 1.1.1, garder son nom, sa table de broches et toutes ses initialisations hors READ_BAT_ENABLE. Le réglage doit intervenir avant `setup()`, dans le vrai `initVariant()`.
- Essai de non-régression prévu : deux compilations carte ; test hôte du vrai `initVariant()` et de l'ordre GPIO ; vérification du symbole/binaire ; capture d'au moins deux transmissions sur USB après installation. La batterie reste débranchée pendant cette vérification.
- Polarité photographiée : dos de carte face à l'utilisateur, USB en haut, deux rectangles à droite : BAT− supérieur, BAT+ inférieur. Vérification de la polarité électrique de la batterie et de sa protection toujours nécessaire avant raccordement.
- Réalisation : variante locale et sélection commune aux deux profils ; en-tête amont identique, unique changement fonctionnel LOW avant OUTPUT. Documentation de provenance/limites et suite de trois tests hôte/configuration/ELF ajoutées.
- Les deux profils compilent. Trois tests réussissent via découverte unittest. Application seule installée via DFU ; deux mesures valides transmises/reçues à l'identique à environ 2,686 V sur USB. Détail et limite du seuil de capture général dans `diagnostics/xiao-batterie-20261103.md`.
- L'utilisateur confirme batterie protégée et multimètre. Le test matériel sur batterie et la recharge ne font pas partie des preuves acquises ; mesure de polarité demandée avant raccordement.
- Contrôle utilisateur reçu ensuite : tension de 3,29 V avec les pointes sur les fils de même couleur, sans signe négatif signalé. Raccordement physique et test sur batterie restent à effectuer par l'utilisateur, USB débranché.

## Review Triage Log

- `low`, corrigé : la suite initiale ne participait pas à la découverte unittest ; ajout de `BatteryVariantTest`, trois cas découverts et exécutés.
- `medium`, corrigé : les sorties conservées étaient attendues via leurs propres constantes ; valeurs QSPI/LED indépendantes 25/11/13/12 dans la trace.
- `medium`, corrigé : test hôte seul insuffisant pour prouver la sélection au lien ; contrôle configuration et désassemblage des deux ELF ajoutés, exécutés avec succès.
- `low`, corrigé : prérequis uv/Zig/compilation des deux profils manquants ; ils sont explicités dans la documentation.
- Relecture indépendante ciblée : les quatre constats sont résolus, aucun report.
