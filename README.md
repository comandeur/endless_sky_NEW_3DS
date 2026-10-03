# Endless Sky pour New Nintendo 3DS

Portage homebrew d'[Endless Sky](https://github.com/endless-sky/endless-sky) **0.11.3**
(la dernière version) pour New 3DS / New 2DS XL modée, qui tire parti des deux écrans.

> **État : expérimental.** Le jeu compile et a été testé de bout en bout dans un
> émulateur (Panda3DS, mode mémoire New 3DS) : nouveau pilote, achat d'un vaisseau,
> décollage, vol, carte stellaire, menus avec la loupe tactile. Il n'a pas encore été
> essayé sur une vraie console. Les retours (logs, captures, plantages) sont les
> bienvenus : voir [Signaler un problème](#signaler-un-problème).

*English summary: a homebrew port of Endless Sky 0.11.3 to the New Nintendo 3DS,
using citro3d for rendering, NDSP for audio, and both screens: the game world on the
top screen and the HUD with touch controls on the bottom screen in flight; an overview
on the top screen and a touch-enabled magnifier on the bottom screen in menus.*

## Les deux écrans

**En vol**

| Écran du haut | Écran du bas (tactile) |
| --- | --- |
| L'espace, les vaisseaux, les réticules de ciblage et les messages | Le HUD : radar, état du vaisseau (boucliers, coque, énergie, chaleur, carburant), cible, escorte, munitions |
| | Boutons tactiles : INFO, LOG (journal), CLOAK (camouflage), GATHER (rassembler la flotte), PAUSE, >> (accéléré) |

**Dans les menus** (planètes, boutiques, carte, journal, préférences...)

Les menus du jeu sont conçus pour un écran de 1024×768. Ils sont dessinés sur un
canevas de cette taille :

- l'écran du haut affiche le canevas entier en réduction (avec un cadre jaune qui
  montre la zone agrandie et un point blanc pour le dernier toucher) ;
- l'écran du bas est une **loupe** sur une partie du canevas, à taille lisible, et
  c'est elle que l'on touche avec le stylet : ce que l'on voit est ce que l'on touche.

Le stick (Circle Pad) déplace la loupe. Son grossissement se règle avec le
**C-Stick gauche / droite** ou **Select** (6 niveaux, de 125 % à 50 %) ; le choix est
mémorisé.

## Commandes

**En vol** (les touches reprennent les raccourcis clavier du jeu sur PC, donc les
aides du jeu affichent le nom des boutons 3DS) :

| Bouton | Action |
| --- | --- |
| Circle Pad haut / bas | Poussée / demi-tour |
| Circle Pad gauche / droite | Tourner |
| A | Tir des armes principales |
| X | Tir de l'arme secondaire |
| Y | Choisir l'arme secondaire |
| B | Postcombustion |
| L | Cibler le vaisseau suivant |
| R | Cibler l'ennemi le plus proche |
| ZL | Atterrir |
| ZR | Saut hyperspatial |
| Croix haut | Héler le vaisseau ciblé |
| Croix bas | Aborder le vaisseau ciblé |
| Croix gauche | Scanner le vaisseau ciblé |
| Croix droite | Lancer / rappeler les chasseurs |
| C-Stick haut / bas | Zoom avant / arrière |
| Start | Menu principal |
| Select | Carte stellaire |

**Dans les menus** :

| Bouton | Action |
| --- | --- |
| Écran tactile | Souris (toucher = clic, glisser = faire glisser) |
| Circle Pad | Déplacer la loupe (elle se centre d'elle-même sur les fenêtres qui s'ouvrent) |
| Croix | Flèches du clavier |
| A | Entrée |
| B ou Start | Échap (retour) |
| X | Tab |
| Y | Clavier virtuel quand il y a un champ de texte (nom du pilote, nom du vaisseau...) ; recherche dans la carte |
| L (maintenu) | Maj (acheter/vendre par 5, etc.) |
| R (maintenu) | Ctrl (acheter/vendre par 20, etc.) |
| ZL / ZR | Page précédente / suivante |
| C-Stick haut / bas | Molette de la souris |
| C-Stick gauche / droite | Grossissement de la loupe (moins / plus) |
| Select | Grossissement de la loupe (cycle) |

## Installation

Il faut une **New 3DS ou New 2DS XL** avec un custom firmware (Luma3DS) et **FBI**.
Pour avoir du son, le firmware DSP doit avoir été extrait une fois avec l'outil *DSP1*
(fichier `sdmc:/3ds/dspfirm.cdc`) ; c'est déjà le cas sur la plupart des consoles modées.

### Version publication (recommandée) : un seul fichier

`endless-sky.cia` (environ 82 Mo) contient le jeu **et** ses données. Il est construit
automatiquement par GitHub à chaque modification : le télécharger depuis la page
**Releases** du dépôt (version « latest »).

1. Copier `endless-sky.cia` n'importe où sur la carte SD (par exemple à la racine).
2. Sur la console, ouvrir **FBI** → *SD* → `endless-sky.cia` → *Install CIA*.
3. Lancer *Endless Sky* depuis le menu HOME.

Le fichier `.cia` peut ensuite être supprimé de la carte SD. Pour mettre à jour,
installer le nouveau CIA par-dessus : les sauvegardes sont conservées.

Pour produire ce fichier : `tools/make-release.sh` (résultat dans `dist/`).

### Version développement : programme et données séparés

Pratique quand on recompile souvent : le programme fait 3 Mo, les données ne bougent pas.

1. Copier **le contenu** de `assets-out/endless-sky/` dans `sdmc:/3ds/endless-sky/`
   (il doit y avoir par exemple `sdmc:/3ds/endless-sky/images.pak`).
2. Installer `build/endless-sky.cia` avec FBI, ou copier `build/endless-sky.3dsx`
   dans `sdmc:/3ds/` et le lancer depuis le Homebrew Launcher.

Si un CIA contient des données, il utilise toujours les siennes, même si d'autres
données sont présentes sur la carte SD.

**Pourquoi le CIA ?** Les données du jeu occupent environ 50 Mo de mémoire une fois
chargées, plus la mémoire des textures. Le CIA demande le mode mémoire de la New 3DS
(124 Mo pour le jeu), le processeur à 804 MHz et le cache L2. Un `.3dsx` reçoit la
mémoire du titre qui héberge le Homebrew Launcher, qui peut être trop faible : dans ce
cas le jeu s'arrête au démarrage avec le message *Not enough memory*.

Les sauvegardes et préférences sont dans `sdmc:/3ds/endless-sky/config/` (les
sauvegardes du jeu PC sont compatibles : il suffit de copier les fichiers de
`saves/`). Le CIA et le `.3dsx` utilisent les mêmes données et les mêmes sauvegardes.

## Compilation

Le seul prérequis est **Docker** (ou une installation de devkitPro avec devkitARM,
libctru, citro3d et les portlibs 3DS libpng, libjpeg-turbo, libmad, flac, zlib,
minizip).

```sh
git clone --recursive https://github.com/comandeur/endless_sky_NEW_3DS.git
cd endless_sky_NEW_3DS

# Le jeu : produit build/endless-sky.3dsx et build/endless-sky.cia
tools/docker-build.sh

# Les données du jeu (images, sons, textes) : produit assets-out/endless-sky/
# Compter 15 à 30 minutes.
tools/convert-assets.sh
```

Sans Docker, avec devkitPro installé :

```sh
cmake -B build -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/3DS.cmake
cmake --build build
python3 tools/assets/convert.py    # nécessite Pillow, numpy, tex3ds et g++
```

Le CIA n'est produit que si `makerom` et `bannertool` sont dans le `PATH` (l'image
Docker de `tools/docker/` les compile). Ses réglages sont dans
`port/meta/endless-sky.rsf`.

## Comment fonctionne le portage

Le code du jeu n'est **pas copié** dans ce dépôt : il vient du dépôt officiel, en
sous-module (`upstream/`, épinglé sur la version 0.11.3). Le script
`tools/prepare-source.sh` assemble les sources compilées :

1. les sources d'Endless Sky (`upstream/source`) ;
2. quelques petits correctifs (`port/patches/*.patch`) ;
3. des fichiers qui remplacent complètement leur version d'origine
   (`port/overlay/source/`), essentiellement le rendu ;
4. la couche 3DS (`port/src3ds/`, compilée dans `ctr/`).

| Partie | Desktop | 3DS |
| --- | --- | --- |
| Rendu | OpenGL 3 + shaders GLSL | citro3d : un vertex shader PICA200 (`port/shaders/es.v.pica`), le reste en combineurs de texture (`ctr/Gfx.cpp`). Les couleurs des factions (« swizzle ») sont un produit matriciel fait en trois étages de combineurs. |
| Textures | toutes chargées au démarrage (~2 Go de VRAM sur PC) | chargées à la demande depuis `images.pak`, avec un cache LRU dans la mémoire linéaire (`ctr/TextureCache.cpp`) |
| Images | PNG/JPEG décodés au lancement | converties à l'avance en textures ETC1A4/RGBA8 compressées LZ11, réduites selon leur usage (`tools/assets/convert.py`) |
| Masques de collision | calculés au lancement | précalculés avec le code du jeu (`tools/assets/masktool.cpp`) |
| Fenêtre, entrées | SDL2 | sous-ensemble de SDL2 réimplémenté sur libctru (`port/compat/SDL2`, `ctr/Input.cpp`, `ctr/SDLShim.cpp`) |
| Audio | OpenAL | sous-ensemble d'OpenAL réimplémenté avec un mixeur logiciel sur NDSP (`ctr/OpenAL.cpp`) ; sons en IMA ADPCM 22 kHz (5 Mo au lieu de 80 Mo en mémoire) |
| Deux écrans | — | `ctr/Display.cpp` + `port/data/interfaces.txt` (disposition du HUD) |

### Mettre à jour vers une nouvelle version d'Endless Sky

```sh
cd upstream && git fetch --tags && git checkout v0.X.Y && cd ..
tools/docker-build.sh
```

Si un correctif de `port/patches` ne s'applique plus, `prepare-source.sh` l'indique.
Les fichiers de `port/overlay` sont à comparer avec les nouvelles versions de leurs
originaux.

## Limitations connues

- Pas de flou de mouvement (désactivé), et les contours des vaisseaux dans le HUD
  sont des silhouettes colorées au lieu d'un vrai filtre de Sobel.
- Les plugins sont lus, mais leurs images (PNG/JPEG) sont décodées sur la console :
  c'est lent, et les images AVIF ne sont pas prises en charge.
- La loupe des menus demande de se déplacer dans les grands écrans (boutiques).
- Quelques aides du jeu citent des touches sans équivalent sur la 3DS (F1 ; pour F,
  la recherche dans la carte, utiliser Y).
- La fréquence d'images en vol dépend du nombre de vaisseaux : le jeu dessine moins
  d'images quand il est en retard, mais la simulation reste à vitesse normale.

## Signaler un problème

Le jeu écrit ses erreurs dans `sdmc:/3ds/endless-sky/config/errors.txt`. En cas de
plantage, Luma3DS crée un rapport dans `sdmc:/luma/dumps/arm11/`. Joignez ces
fichiers (et si possible une photo des écrans) à votre rapport. L'option
« Show CPU / GPU load » des préférences affiche le temps de calcul et la mémoire
utilisée.

## Licence

Endless Sky est un logiciel libre sous licence GNU GPL v3 ou ultérieure, comme ce
portage. Les images et sons du jeu ont leurs propres licences, détaillées dans
`upstream/copyright`.
