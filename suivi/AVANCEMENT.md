# Suivi du portage — Endless Sky 0.11.3 → New 3DS

Fichier de travail (temporaire) : état d'avancement, décisions, problèmes ouverts.
Mis à jour au fil de la session.

## État global

| Étape | État |
| --- | --- |
| Dépôt, sous-module upstream (v0.11.3), overlay + patches, CMake/Docker | ✅ fait |
| Façade SDL2 (libctru), clavier logiciel, uuid, utsname | ✅ fait |
| Rendu citro3d (`ctr/Gfx`) : shaders réécrits, swizzle en TEV, polices | ✅ compile, partiellement vérifié en émulateur |
| Menus : enregistrement/rejeu des commandes (pas de rendu-vers-texture) | ✅ vérifié en émulateur (vue d'ensemble + loupe) |
| Streaming des textures (`ctr/TextureCache`, images.idx/pak) | ✅ compile, à vérifier visuellement |
| Audio : mini-OpenAL sur NDSP, sons IMA ADPCM | ✅ compile, non testé (pas de DSP dans l'émulateur) |
| Convertisseur d'assets (`tools/assets/convert.py`) | ✅ conversion complète OK : 3338 sprites, pack 57 Mo, total 79 Mo |
| HUD sur l'écran du bas + boutons tactiles | ✅ vérifié en émulateur (radar, état, PAUSE, INFO) |
| Parcours complet : nouveau pilote → nom (clavier Y) → achat vaisseau → décollage → vol → carte | ✅ vérifié en émulateur |
| Démarrage complet dans l'émulateur (mémoire New 3DS, 124 Mo) | ✅ menu principal affiché |
| CIA (mode New 3DS 124 Mo, 804 MHz, L2, cœur 2) | ✅ construit, exheader vérifié avec ctrtool ; pas testable dans Panda3DS |
| Test sur vraie console | ⏳ par l'utilisateur |

## Problèmes rencontrés et corrections

1. `std::filesystem` sur la SD : exceptions sur chemins absents, `stat` d'un dossier
   faux dans l'émulateur → patch `0003` : `opendir`/`readdir` + variantes `error_code`.
2. Cibles de rendu : doivent être en VRAM (2×3 Mo) et l'émulateur ne sait pas les relire
   → remplacé par l'enregistrement/rejeu de commandes (`Gfx::CommandList`).
3. Threads : l'ordonnanceur 3DS ne partage pas le temps à priorité égale →
   `TaskQueue` en overlay : threads libctru sur les cœurs 1/2, attente par `svcSleepThread`.
4. Priorité de thread > 0x3F → bornée.
5. Chargement des données : exception dans une lambda `noexcept` → `terminate`.
   Patch `0006` (log des erreurs de chargement) : c'était bien `std::bad_alloc`.
   L'émulateur ne donnait que 80 Mo à l'appli (heap 45 Mo). Réglé sur 124 Mo
   (mémoire New 3DS) : heap libre 73 Mo, budget textures 31 Mo, chargement OK.
   ⇒ les données demandent ~65-70 Mo de heap : il faut le mode mémoire étendu
   (CIA, ou .3dsx lancé depuis un titre qui donne 124 Mo).

6. Écran du bas noir sur les longs textes : le canevas était copié deux fois dans
   l'arène de sommets (un rejeu par écran) → arène pleine. Correction : copie unique
   par image (`copiedFrame`), et la police réserve le nombre exact de sommets.
   Pic mesuré : ~50k sommets / 98k.
7. Saisie du nom : `ConversationPanel` et `DialogPanel` lisent les caractères via
   `KeyDown`, pas via `SDL_TEXTINPUT` → Y ouvre toujours le clavier en menu ; le texte
   est envoyé en `TEXTINPUT` si un `Edit` a le focus, sinon en touches.
8. La loupe suit les panneaux : centrée sur un dialogue qui s'ouvre (à gauche pour les
   conversations), position restaurée quand il se ferme.

9. Mémoire mesurée : ~49 Mo de heap pour les données chargées. Nouveau partage :
   heap ≈ 64 Mo, le reste en mémoire linéaire (budget textures 31 → 41 Mo).
   Sous 52 Mo de heap libre, le jeu affiche « Not enough memory » (conseille le CIA).

10. Version publication : `tools/make-release.sh` → `dist/endless-sky.cia` (82 Mo) avec
    les données dans la romfs (option CMake `ES_ROMFS`). La romfs est prioritaire sur la SD.
    Vérifié en émulateur (version .3dsx avec romfs, aucune donnée sur la SD) : menus, vol.

Limite émulateur : pas de service `ir:rst` → ZL/ZR/C-Stick non testables (saut, atterrissage).

## Prochaines étapes

- [x] Émulateur en mode mémoire New 3DS, vérifier le chargement complet et le menu principal
- [x] Mesurer heap/linéaire réellement utilisés ; réduire si besoin
- [x] Vérifier le vol : monde en haut, HUD en bas, boutons tactiles
- [x] Vérifier une planète / boutique avec la loupe tactile
- [x] Construire un CIA avec mémoire étendue (makerom) et le documenter
- [x] Retirer les traces DEBUG de `main.cpp` avant la version finale
- [x] Mettre à jour le README (état, limitations)
- [ ] Test sur vraie New 3DS (par l'utilisateur) : CIA, son, ZL/ZR, C-Stick, performances
- [ ] Réactiver les mipmaps de la police si le rendu le permet
