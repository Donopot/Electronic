# Variante locale XIAO nRF52840

`Seeed_XIAO_nRF52840/variant.h` et `variant.cpp` proviennent du paquet
`framework-arduinoadafruitnrf52` 1.10101.0 (cœur Seeed 1.1.1), sélectionné
par la plateforme Seeed figée au commit
`a6fd41debbbcda13b5340fff50c9092ed3289194`.

Empreintes SHA-256 des fichiers amont avant modification :

- `variant.h` : `a74818f54005de7f6002b6e2b143686edb9d99a6b98411f469a50960c9d31a0d`
- `variant.cpp` : `e4293e433980627dfe871a5c46115462774ec36a4efbf61fd1f025e7c66dab90`

L'unique changement fonctionnel est dans `initVariant()` : précharger
`VBAT_ENABLE` à LOW puis configurer la sortie. P0.14 reste ainsi bas dès
l'initialisation de la variante, conformément à la
[FAQ Seeed sur la recharge](https://wiki.seeedstudio.com/XIAO_BLE/#q3-what-are-the-considerations-when-using-xiao-nrf52840-sense-for-battery-charging).
La table des broches, les LED, QSPI et `PIN_CHARGING_CURRENT` en entrée sans
rappel restent identiques à l'amont. Ce dernier réglage correspond au
courant faible annoncé d'environ 50 mA ; il n'a pas été mesuré.

`board_build.variants_dir = variants` sélectionne ces fichiers pour les cinq
profils de ce projet seulement. Aucun paquet partagé n'est modifié. Lors
d'une mise à jour du cœur, comparer de nouveau cette copie à la variante
constructeur.

Cette correction concerne l'application à partir de `initVariant()`.
Le bootloader et la période précédant l'application ne sont ni modifiés ni
validés. Elle ne prouve pas la protection de la batterie, la polarité de ses
fils, le courant réel, la température ou le bon déroulement d'une recharge.
Le premier essai batterie se fait USB débranché ; ne pas lancer une mise à
jour/DFU ou une recharge USB avec la batterie raccordée pendant cet essai.

Vérification depuis la racine : Python, `uv` dans le PATH et les paquets
PlatformIO locaux sont requis. Le test hôte utilise `ziglang==0.13.0` via
`uv` ; le premier lancement peut nécessiter son téléchargement. Compiler les
cinq profils avant le test : celui-ci inspecte aussi leurs ELF avec le
`nm`/`objdump` ARM du paquet PlatformIO. Cette vérification est adaptée au
compilateur et à la plateforme figés ci-dessus.

```powershell
$env:PYTHONPATH = Join-Path $PWD '.tools/xiao-diagnostic-python'
$env:PLATFORMIO_CORE_DIR = Join-Path $PWD '.tools/platformio'
python -m platformio run --project-dir firmware/XiaoSoilDiagnostic -e xiao_soil_diagnostic -e xiao_soil_lora_tx -e xiao_soil_lora_pmos -e xiao_soil_lora_pmos_node2 -e xiao_soil_lora_pmos_node3
python tests/test_xiao_battery_variant.py
# Même suite avec découverte unittest :
python -m unittest discover -s tests -p test_xiao_battery_variant.py -v
```
