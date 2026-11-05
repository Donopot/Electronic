# Plant Station — bienvenue chez vous

Votre kit suit quatre plantes. Le petit robot sur le Heltec donne un aperçu ;
le téléphone affiche les dernières mesures et leur historique.

## Première connexion avec Android

1. Installez l’application **Mes plantes** fournie avec le kit. Gardez le
   téléphone connecté au Wi-Fi de la maison et activez Bluetooth.
2. Allumez le Heltec. Dans l’app, saisissez le nom du Wi-Fi domestique, ou
   touchez « Utiliser le Wi-Fi de ce téléphone » pour le préremplir.
   Android demande la localisation pour lire ce nom ; vous pouvez refuser et
   le saisir vous-même. Le kit utilise le **Wi-Fi 2,4 GHz**.
3. Touchez « Associer mon kit avec son QR » et scannez l’écran **ANDROID**
   du Heltec depuis l’app. Autorisez Bluetooth et la caméra au moment demandé.
   Si le QR est trop petit, ajustez la distance ou utilisez « Saisir le nom
   et le code du kit ». Le code comporte deux lignes de huit caractères,
   à réunir sans espace. Gardez ce code privé.
4. Saisissez une fois le mot de passe du Wi-Fi de la maison. L’app ne peut pas
   lire le mot de passe mémorisé par Android. Touchez « Connecter le kit ».
   **Le téléphone reste sur son Wi-Fi domestique.**
5. Attendez « Votre jardin est prêt » : le kit a confirmé sa connexion et sa
   sauvegarde. Touchez **Ouvrir mes plantes** pour afficher le site local.
   Ce bouton reste disponible lors des prochaines ouvertures de l’app.

Un mot de passe erroné laisse l’ancien réseau enregistré ; corrigez-le puis
réessayez. Si Bluetooth est interrompu ou l’app quittée, rouvrez l’association
pour vérifier et reprendre. L’app n’annonce jamais la réussite au simple envoi.
Les mots de passe et le code ne sont pas conservés dans l’app.

## Secours avec le navigateur

1. Un appui court sur PRG après le QR Android affiche le **QR Wi-Fi du kit**.
   Scannez-le avec l’appareil photo du téléphone, ou rejoignez manuellement
   `PLANT-…` avec la clé affichée. Restez sur ce réseau même s’il annonce
   « pas d’Internet ».
2. Ouvrez l’assistant proposé, sinon appuyez encore brièvement sur PRG pour
   le QR du lien, ou ouvrez **http://192.168.4.1/** dans le navigateur.
3. Choisissez ou saisissez le Wi-Fi de la maison et son mot de passe. Attendez
   la confirmation, puis rejoignez le Wi-Fi de la maison et ouvrez le lien local.

Le portail peut annuler un essai avant sa confirmation. Après réussite, son
bouton de fermeture ou un délai de cinq minutes ferme la configuration.

## Une autre maison, une autre box

Maintenez **PRG pendant huit secondes, puis relâchez**. À trois secondes,
l’écran propose la veille : continuez à maintenir jusqu’à la confirmation
de configuration. Le QR Android du kit réapparaît ; reprenez les étapes
ci-dessus. Les noms des plantes et l’historique restent conservés.

Une coupure temporaire de la box garde son réseau enregistré et déclenche une
reconnexion automatique. Après trente secondes sans connexion, le Wi-Fi du
kit permet aussi de rouvrir l’assistant.

## Les boutons et les plantes

- **PRG court** : visage, détails des sondes, QR Android, QR Wi-Fi puis lien de secours si
  l’assistant est ouvert.
- **PRG trois secondes puis relâcher** : mettre le Heltec en veille.
- **PRG huit secondes puis relâcher** : configurer le Wi-Fi.
- **RST** : redémarrer, notamment après la veille.

Les sondes transmettent toutes les trente minutes. Après redémarrage, les
valeurs courantes attendent de nouveaux messages ; les anciennes mesures
restent dans l’historique. « En attente », « défaut ADC » et « périmée »
signalent des situations distinctes.

Le calibrage se fera plus tard : « à étalonner » est donc normal. Une tension
n’est pas un pourcentage d’humidité. La batterie des sondes apparaît comme
« non communiqué » : les sondes actuelles n’en transmettent pas la mesure.
