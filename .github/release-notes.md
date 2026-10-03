Endless Sky 0.11.3 pour **New 3DS / New 2DS XL** (custom firmware Luma3DS).

### Installation

1. Télécharger **`endless-sky.cia`** ci-dessous (environ 82 Mo : le jeu et toutes ses données).
2. Le copier sur la carte SD (par exemple à la racine).
3. Sur la console : **FBI** → *SD* → `endless-sky.cia` → *Install CIA*.
4. Lancer *Endless Sky* depuis le menu HOME.

Le `.cia` peut ensuite être supprimé de la carte SD. Les sauvegardes sont dans
`sdmc:/3ds/endless-sky/config/` et sont conservées lors des mises à jour.

Pour le son, il faut le fichier `sdmc:/3ds/dspfirm.cdc` (extrait une fois avec *DSP1*).

`endless-sky.3dsx` est la même chose pour le Homebrew Launcher ; il peut manquer de
mémoire selon le lanceur, le CIA est recommandé.

### En cas de problème

Joindre `sdmc:/3ds/endless-sky/config/errors.txt`, `sdmc:/3ds/endless-sky/fatal-error.txt`
(s'il existe) et les fichiers de `sdmc:/luma/dumps/arm11/`.
