# Shield DJ Lite

Lecteur MP3 et OGG pour console 3DS, pensé pour les modèles Old 3DS. Il lit les fichiers de la carte SD avec lecture aléatoire, réglage de vitesse et de pitch liés, basses, filtres et réverbération facultative. L’interface est disponible en français et en anglais. Un guide apparaît au premier lancement ; maintenir **SELECT** au démarrage le rouvre.

La [release v1.7.4](https://github.com/Shield-77/Shield-DJ-Lite/releases/tag/v1.7.4) distribue `ShieldDJLite.cia`, Title ID `000400000A210100`, version CIA interne **1123**. Cette version ajoute une réverbération longue facultative et corrige l’affichage du temps lorsque le pitch change. Les anciennes notes sont dans [CHANGELOG.md](CHANGELOG.md).

## Source et compilation

Le [code C de v1.7.4](source/) et les [en-têtes](include/) correspondent à la version du CIA publié. La copie publique utilise un écran inférieur SHIELD de remplacement et une bannière silencieuse : **le CIA officiel conserve son illustration créditée et son effet sonore d’origine**. Le fichier audio `S_Inkopolis_News.mp3`, inutilisé pour la compilation, reste uniquement dans la copie privée. Aucune de ces ressources privées n’est nécessaire pour compiler le source public.

Installer devkitPro/devkitARM, libctru, Tremor et libogg pour 3DS. Obtenir séparément `makerom` et `bannertool`, puis lancer :

```sh
make cia MAKEROM=/chemin/makerom BANNERTOOL=/chemin/bannertool
```

Le résultat est `build/ShieldDJLite.cia`. Son visuel inférieur et son son de bannière diffèrent du CIA officiel. Le code, la version interne et le Title ID restent ceux de v1.7.4. Les binaires des outils de construction ne sont pas redistribués ici.

## Licence et crédits

Le code original de Shield DJ Lite est sous [GNU GPL version 3 uniquement](LICENSE). La déclaration du projet figure dans [NOTICE.md](NOTICE.md). Les composants tiers gardent leurs propres licences : [minimp3 CC0](LICENSES/minimp3-CC0.txt), [Tremor](LICENSES/tremor-COPYING) et [libogg](LICENSES/libogg-COPYING). Le nom et les logos SHIELD identifient la publication officielle ; un fork doit indiquer son origine et ses changements sans se faire passer pour Shield-77.

L’illustration de Callie de l’écran inférieur du CIA officiel est créditée à **kamrikeen**. Elle n’est pas incluse dans ce dépôt source public ; aucune autorisation de redistribution de cette œuvre n’est documentée ici.
