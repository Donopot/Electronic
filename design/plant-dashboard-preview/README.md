# Aperçu Plant Station

Ouvrir `index.html` dans un navigateur, ou lancer depuis la racine :

```powershell
python -m http.server 8788 --bind 127.0.0.1 --directory design/plant-dashboard-preview
```

Puis ouvrir [l’aperçu local](http://127.0.0.1:8788/).

La page permet d’essayer quatre situations (sèches, satisfaisantes, sans signal,
avant calibrage), de sélectionner une plante, changer la période et personnaliser
les noms pour l’aperçu. Les trois vues du Heltec sont simulées sur un canevas
de 128×64. Navigation tactile et clavier, sans ressource externe.

Toutes les données sont fictives et indiquées comme telles. Les courbes sont
illustratives, pas des relevés enregistrés. Les changements de noms ne sont pas
enregistrés. Le cadre « QR » réserve sa future place : aucun faux code scannable
n’est présenté comme adresse réelle du Heltec.

Le site embarqué, la connexion au Wi-Fi domestique et l’historique réel de
30 jours sont définis dans la spécification. Aucun flash ni calibrage des sondes
n’a été effectué pour créer cet aperçu.
