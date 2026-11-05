# SOIL-01 : premier contrôle radio depuis le balcon

## Résultat

**Court essai réussi : quatre mesures consécutives reçues sur le Heltec**,
XIAO sur batterie seule au balcon. Aucune erreur de capture ni défaut ADC
signalé. Ce résultat ne constitue pas une campagne de fiabilité prolongée.

## Conditions

- Emplacement au balcon confirmé par l'utilisateur, électronique maintenue
  au sec selon la consigne ; Heltec en intérieur sur USB au PC.
- Porte/fenêtre demandée en position habituelle ; aucun relevé de distance,
  d'orientation ou de position exacte des obstacles.
- Sonde **dans l'air**, confirmé pendant la capture ; cette mesure est un
  contrôle électrique et ne définit pas le seuil d'arrosage d'un pot.
- XIAO absent de l'inventaire USB avant et après la capture ; Heltec COM5,
  DTR et RTS inactifs. Aucun flash ni réinitialisation pendant cet essai.
- Début : **3 novembre 2026 à 18:13:06, heure de Paris**. Durée : **102,406 s**.
- Firmware de diagnostic existant, sans P-MOS ni optimisation d'autonomie.

## Mesures reçues

Même session **4236450037**, adresse ADC **0x48**, état `ok` :

| Séquence | Réception depuis début capture | Brut | Tension de sonde |
| --- | --- | --- | --- |
| 14 | 10,484 s | 21071 | 2,633875 V |
| 15 | 41,109 s | 21065 | 2,633125 V |
| 16 | 71,750 s | 21052 | 2,631500 V |
| 17 | 102,406 s | 21051 | 2,631375 V |

Cadence observée : **30,625 à 30,656 s**. Aucun numéro manquant entre les
quatre messages observés. Le Heltec a également produit 102 mesures lux
valides. Sans capture TX simultanée, aucun taux de perte global n'est calculé ;
le RSSI n'est pas mesuré par ce journal. La position dans l'air et la durée
courte ne permettent pas de conclure sur l'humidité du pot ou l'autonomie.

Preuves : `soil-balcon-20261103-171306Z.log` et
`soil-balcon-20261103-171306Z.json`, bilan `accepted: true`.

## Prochaine étape

Préparer l'économie d'énergie : identifier la référence et le brochage du
P-MOS avant câblage, conserver la règle D0 relâchée au repos et basse pour
alimenter la sonde, puis adapter les cycles et les seuils de données anciennes
du hub. L'autonomie sera ensuite mesurée ; elle n'est pas déduite du présent
test. Garder les composants à l'intérieur et au sec pendant cette préparation.
