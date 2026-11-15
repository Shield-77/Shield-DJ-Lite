## v1.7.4 — CIA 1123

- Réverbération longue facultative, désactivée par défaut.
- Affichage du temps corrigé lorsque le pitch change.
- Title ID conservé : `000400000A210100`.

## v1.7.2

- Résonance de Concert, Cathédrale, Extra et Sur mesure prolongée sans augmenter l'intensité réglée par l'utilisateur.
- Sur le test impulsionnel, la traîne d'Extra au-dessus de −60 dB passe d'environ 0,65 s à 1,30 s. La protection des pointes de la v1.7.1 est conservée.
- Aucun changement aux décodeurs MP3/OGG, aux commandes ou à l'interface. CIA 1121, Title ID conservé : `000400000A210100`.

## v1.7.1

- Ajout du filtre Classique, avec une courbe douce mesurée.
- Courbes des filtres rendues plus distinctes, sans amplification supplémentaire au-dessus du niveau d'entrée. Le mode Aucun reste neutre.
- Réverbération ajustée et protection des pointes sur la voie avec effet ; les quatre profils et le mode sur mesure restent disponibles.
- Zone tactile de la barre de réverbération agrandie, sans recouvrir la barre de recherche dans le morceau.
- CIA 1120, Title ID conservé : `000400000A210100`.

## v1.6.10-audio-test (expérimental, non publié)

- Le profil de basses OFF désactive réellement le biquad NDSP ; les profils +6, +9, +16 et +22 dB restent identiques.
- Le compteur de trames audio décodées et son affichage ont été retirés pour libérer de la place à l'écran inférieur.
- Aucun changement au décodage MP3/OGG, au boost audio, à la réverbération ni aux fondus.
- Le risque de modification d'un buffer NDSP déjà en file à la fin d'une piste reste à examiner sur matériel ; aucun changement spéculatif n'a été appliqué à ce cycle.

## v1.6.3

- Added a +16 dB bass profile between Boost (+9 dB) and Extreme (+22 dB), selectable with Y during playback.

## v1.6.2

- Replaced the lower-screen startup image with the provided credited artwork; the original source image is preserved and scaled to 320 × 240 without distortion.

# Shield DJ Lite by Yanis — historique des versions

## v1.5.1

- Un tour complet du Circle Pad vers la droite verrouille les boutons L/R ; un tour vers la gauche les déverrouille. Les déplacements ordinaires du joystick n'ont aucun autre effet.
- L'état du verrouillage est affiché sur l'écran supérieur. Un retour au centre est nécessaire avant un autre geste pour éviter les activations involontaires.

## v1.5.0

- Transition douce de 150 ms à la fin puis au début des pistes MP3 et OGG, y compris lorsque X répète le morceau en cours.
- Aucun second décodeur ni gros tampon ajouté : le fondu est appliqué aux blocs PCM existants pour rester adapté à la Old 3DS.

## v1.4.9

- Le pitch et la vitesse avancent maintenant de 0,02× par appui, avec un affichage à deux décimales cohérent (par exemple 1,30 → 1,32 → 1,34). Les bornes restent 0,25× et 1,75×.

## v1.4.8

- Shuffle actif en permanence. X active ou coupe la répétition de la piste en cours pendant la lecture.
- POWER retrouve son comportement système normal ; l'app ne l'intercepte plus.

## v1.4.7

- HOME parcourt OFF, 150 % et 200 % pour le boost audio. Chaque bloc PCM est limité selon son pic avant l'envoi au DSP.
- Un appui bref POWER détecté pendant la lecture active ou coupe la répétition de la piste actuelle. L'app ne bloque pas l'extinction prolongée de la console.

## v1.4.6

- Pitch toujours réglé par pas de 0,025×, affiché avec deux décimales pour simplifier la lecture.
- Volume de base relevé de 0,75 à 1,00. HOME active ou coupe un boost de 150 % maximum pour les enregistrements faibles ; les pics sont limités par bloc avant l'envoi audio.

## v1.4.5

- Le Circle Pad ne commande plus la navigation, la luminosité ni le pitch ; seule la croix directionnelle agit.
- Gauche/droite change le pitch et la vitesse de 0,025× par appui. Un appui prolongé répète après 350 ms, puis toutes les 160 ms, jusqu'au relâchement. La valeur affichée possède trois décimales.
- La plage de vitesse reste de 0,25× à 1,75×. L'identifiant CIA est inchangé pour mettre à jour Shield DJ Lite.

## v1.4.4
- Renommage de l’édition Lite en Shield DJ Lite ; le titre ID reste inchangé pour permettre la mise à jour de l’application installée.

## v1.4.1
- Renommage visible en Shield Player : écran d’accueil, titre CIA, logo et bannière.
- Compteur de lecture déplacé juste au-dessus de l’espace SD ; son actualisation ne redessine plus toute l’interface chaque seconde.

## v1.4.0
- Ajout du profil de basses DOUX (+9 dB), entre CHAUD (+4,5 dB) et BOOST (+14,5 dB).

## v1.3.15
- Réverbération renforcée : Concert plus ample, Cathédrale et Extra avec une décroissance progressivement plus longue.

## v1.3.14
- Ajout de la progression de lecture au format `MM:SS/MM:SS` (durée OGG exacte ; durée MP3 exacte si le fichier contient un compteur Xing, sinon estimée).
- Réverbération adoucie par une troisième étape de diffusion, sans changer le niveau du signal direct ni les profils.

## v1.3.13
- La luminosité choisie dans le lecteur est maintenant enregistrée comme réglage système : le HOME Menu conserve le même niveau à la fermeture.

## v1.3.12
- Retrait de la jauge d’intensité afin de préserver les ressources de l’Old 3DS.

## v1.3.11
- La jauge affiche l’intensité moyenne RMS plutôt que les crêtes, pour mieux varier avec les morceaux.

## v1.3.10
- Flush graphique après chaque mise à jour partielle de la jauge pour éviter les restes d’image.

## v1.3.9
- Correction de la sélection de console lors du rafraîchissement de la jauge pour éviter les artefacts d’affichage.

## v1.3.8
- La jauge ne redessine plus les deux écrans : seule sa ligne se rafraîchit, jusqu’à 25 fois par seconde.

## v1.3.7
- Ajout d'une jauge de crête PCM en lecture, mesurée passivement avant le filtre Bass Boost matériel.

## v1.3.6
- Correction de la restauration de luminosité pour restaurer la valeur HOME lue au lancement, sans valeur de secours 3/5.
- Retrait de l’appel APT qui forçait la transition vers HOME et pouvait bloquer lors de l’utilisation de POWER.

## v1.3.5
- Suppression des profils Studio et Pièce, peu audibles ; SELECT parcourt maintenant NON, Concert, Cathédrale et EXTRA.

## v1.3.4
- Ajout du profil de réverbération EXTRA après Cathédrale, avec une queue plus longue et un retour renforcé.

## v1.3.3
- Réverbération : conserve davantage le volume direct et réduit l’atténuation ressentie.

## v1.3.2
- Correction du filtre de diffusion de la réverbération 3DS pour un rendu plus régulier et moins brouillon.

## v1.3.1
- Reverb nettement renforcée sur les quatre profils, surtout Concert et Cathédrale.

## v1.3.0
- Ajout de quatre profils de réverbération (Studio, Pièce, Concert, Cathédrale), sélectionnés avec SELECT.
- Traitement PCM à buffers fixes, conçu pour limiter la charge sur Old 3DS.

## v1.2.31
- Le bouton POWER traite désormais la transition APT pour afficher le menu d’alimentation ; HOME reste bloqué et START quitte l’app.

## v1.2.30
- Haut/Bas règlent de nouveau la luminosité dans le lecteur.
- À la sortie, l’app demande au service LCD de restaurer la configuration du HOME, sans utiliser la valeur de secours 3/5 sur Old 3DS.
- START reste le bouton de sortie ; SELECT ne quitte pas l’application.

## v1.2.29
- Garde-fou empêchant une valeur de luminosité de secours de modifier le réglage HOME ; remplacé en v1.2.30 par la restauration de configuration LCD.

## v1.2.28
- Renommage de l’application en ShieldPro77 Player by Yanis dans le menu HOME et l’interface.

## v1.2.27
- START quitte maintenant l’application. SELECT est libéré pour une future fonction.

## v1.2.26
- Suppression du raccourci START qui lançait la lecture aléatoire du dossier.
- Correction des indications de commandes affichées à l’écran.

## v1.2.25
- Alerte rouge de batterie faible à 20 % ou moins.
- L’alerte disparaît au-dessus de 20 % et lorsque le chargeur est branché.

## v1.2.24
- La bibliothèque masque les dossiers qui ne contiennent pas directement de fichier MP3 ou OGG.

## v1.2.23
- Restauration progressive de la luminosité système à la fermeture du lecteur pour éviter un flash.
- La luminosité mémorisée par la console n’est pas modifiée.

## v1.2.22
- Espacement augmenté entre les commandes affichées pendant la lecture pour améliorer leur lisibilité.

## v1.2.21
- Correction d’un calcul de tampon OGG qui pouvait corrompre la fin des blocs audio et provoquer des glitches.

## v1.2.20
- La capacité totale et l’espace libre de la carte SD sont mesurés une seule fois au lancement.

## v1.2.19
- Affichage de l’espace SD libre et total en bas de l’écran supérieur.
- Correction liée à la transition HOME déclenchée par un appui bref sur POWER, qui pouvait geler la console.

## v1.2.18
- Plage de pitch et de vitesse étendue de 0,25× à 1,75×.
- Retour à quatre tampons audio pour réduire les glitches signalés sur OGG.

## v1.2.17
- Lecture OGG plus robuste : récupération après certaines erreurs isolées de paquet Vorbis.
- Marge audio accrue pour réduire les microcoupures sur Old 3DS.

## v1.2.16
- Ajout de la lecture des fichiers OGG Vorbis.
- Les OGG prennent en charge le pitch/vitesse, les profils de basses et la lecture clapet fermé.

## v1.2.15
- Ajout du profil de basses EXTREME (+22 dB), sélectionnable avec Y pendant la lecture.

## v1.2.14
- Égalisation des basses remaniée avec un filtre « low shelf » plus large.
- Écart renforcé entre les profils de basses.

## v1.2.13
- Affichage de la température de la batterie et ajout d’une alerte thermique.
- Seuil d’alerte réglé à 42 °C sur Old 2DS et à 45 °C par défaut sur les autres modèles.
- Texte d’aide clarifié : « Branche un casque avant d’activer le sleep mode. »

## v1.2.12
- Interface de sélection simplifiée : les indications de touches qui encombraient la liste sont masquées.

## v1.2.11
- Liste des morceaux plus facile à parcourir : six entrées affichées à la fois et espacement accru.
- Sélection du morceau mise en évidence plus clairement.

## v1.2.10
- Correction du rafraîchissement de l’interface en quittant l’écran de lecture.
- Noms de fichiers longs raccourcis pour éviter qu’ils débordent de l’écran.

## v1.2.9
- Ajustements de l’affichage de la liste des morceaux.

## v1.2.8
- Alerte thermique à partir de 45 °C sur les modèles autres que la 2DS Old.
- Température actualisée toutes les 15 secondes.

## v1.2.7
- Affichage de la température de la batterie en bas à gauche de l’écran inférieur.
- Première actualisation périodique de la température, toutes les cinq minutes.

## Fonctionnalités présentes depuis la v1.2.6
- Lecture de MP3 depuis la carte SD, avec lecture aléatoire du dossier.
- Lecture continue lorsque la console est fermée, avec sortie audio sur casque.
- Réglage du pitch synchronisé avec la vitesse de lecture.
- Réglage de la luminosité depuis le lecteur.
- Profils d’amplification des basses.
- Indicateur de batterie par paliers de 5 %.

> La température affichée est celle de la batterie. Les seuils thermiques sont des avertissements configurés par l’application ; ils ne mesurent pas la température du processeur.
## v1.7.0

- SELECT ouvre et referme les Options sans interrompre la lecture. B revient à la page précédente.
- Six filtres sélectionnables, dont « Aucun » qui n'ajoute aucun traitement ; Bass Boost reste indépendant.
- La barre tactile de réverbération choisit les profils 1 à 4, ou une intensité libre de 0 à 100 % si « Sur mesure » est activé.
- Les zones tactiles de réverbération et de recherche dans la piste sont distinctes ; le changement s'applique au relâchement.
- La version est visible après Shield-77 sur l'écran supérieur. Le Title ID reste `000400000A210100`.
- Le guide décrit le nouveau menu en français et en anglais.


