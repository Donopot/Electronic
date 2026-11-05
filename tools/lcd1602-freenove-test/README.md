# Test LCD seul — sequence Freenove / LiquidCrystal_I2C

Ce projet separe le test de l'ecran du firmware BMP180 + LCD. Il reproduit la
sequence `lcd.init()`, `lcd.backlight()` et `lcd.print()` de l'exemple Freenove,
avec SDA=GPIO3, SCL=GPIO2 et test des adresses 0x3F puis 0x27. Si ces adresses
ne repondent pas, un scan des autres adresses non reservees 0x08..0x77 est
effectue, avec abandon apres trois erreurs de bus consecutives. Un adaptateur
trouve dans les plages PCF8574 est utilise pour le test d'affichage. Les broches du
BMP180 ne sont pas utilisees. La bibliotheque est LiquidCrystal_I2C 1.1.4.

## Condition electrique avant essai

L'utilisateur a mesure environ 5 V sur SCL et 4,7 V sur SDA. Ces valeurs depassent
les niveaux specifies pour les GPIO de l'ESP32-C3. Adapter les niveaux ou utiliser
une configuration documentee donnant des lignes I2C compatibles 3,3 V avant
de televerser et d'executer ce test. L'alimentation et les niveaux I2C du module
doivent etre distingues.

## Essai effectue sur GPIO3/GPIO2

Apres passage temporaire de VCC sur 3V3, l'utilisateur rapporte 3,32 V sur SDA
et sur SCL. Le test a ete televerse sur COM3. Une capture au redemarrage USB
confirme un NACK a 0x3F et a 0x27 ; l'initialisation LCD est abandonnee, et
l'utilisateur voit uniquement le retroeclairage. Journal depuis la racine :
`diagnostics/lcd-only-at-3v32-startup.log`.

## Comparaison avec les broches ayant fonctionne pour le BMP180

La variante `lcd_test_known_pins` utilise SDA=GPIO4 et SCL=GPIO1. Avant son
televersement, debrancher l'USB, deconnecter temporairement le BMP180 et deplacer
SDA/SCL du LCD vers GPIO4/GPIO1. Garder VCC LCD sur 3V3 pour ce diagnostic.
Cette variante a ete televersee et testee : NACK a 0x3F et 0x27, puis aucun
peripherique au scan complet. Journal : `diagnostics/lcd-only-gpio4-1-full-scan.log`.

L'utilisateur a ensuite precise que SCL est physiquement sur **GPIO3**.
La variante `lcd_test_gpio4_3` correspond donc a SDA=GPIO4 et SCL=GPIO3,
avec BMP180 temporairement deconnecte et VCC LCD sur 3V3. Les tests anterieurs
sur GPIO4/GPIO1 ne permettent pas de conclure sur ce nouveau raccordement.
La variante GPIO4/GPIO3 a ensuite ete compilee, televersee et testee : NACK
a 0x3F et 0x27, puis scan complet 0x08..0x77 sans peripherique detecte et sans
erreur de bus. Journal : `diagnostics/lcd-only-gpio4-3-at-3v3.log`.
La carte contient actuellement ce test LCD seul sur GPIO4/GPIO3.

Sources :
- [Freenove ESP32](https://docs.freenove.com/projects/fnk0079/en/latest/fnk0079/codes/ESP32/ESP32_C/1_LCD1602.html)
- [Schema Freenove : LCD alimente en 5 V, ESP32-WROVER](https://docs.freenove.com/projects/fnk0079/en/latest/_images/Chapter01_051.png)
- [ESP32-C3, caracteristiques electriques, section 5.4](https://documentation.espressif.com/esp32-c3_datasheet_en.html)

## Commandes depuis la racine du depot

```powershell
pio run --project-dir tools/lcd1602-freenove-test
# Uniquement apres correction et verification des niveaux electriques :
pio run --project-dir tools/lcd1602-freenove-test --target upload --upload-port COM3
# Variante GPIO4/GPIO1 apres deplacement des fils :
pio run --project-dir tools/lcd1602-freenove-test -e lcd_test_known_pins --target upload --upload-port COM3
```

Le moniteur serie est a 115200 bauds. L'ACK est teste directement avec Wire,
avant tout appel a la bibliotheque LCD. Un echec d'ACK est donc distinct de la
sequence d'initialisation de l'afficheur. La lisibilite et le contraste restent
a verifier sur l'ecran.
