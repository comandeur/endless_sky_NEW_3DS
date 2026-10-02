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
| HUD sur l'écran du bas + boutons tactiles | ✅ code écrit, non vu en émulateur |
| Démarrage complet dans l'émulateur | 🔧 en cours (voir problèmes) |
| CIA (mémoire étendue 124/178 Mo) | ⏳ à faire |
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
   Cause probable : manque de mémoire (l'émulateur ne donne que 80 Mo à l'appli).
   → prochaine étape : émulateur réglé sur 124 Mo (New 3DS), et mesurer la mémoire.

## Prochaines étapes

- [ ] Émulateur en mode mémoire New 3DS, vérifier le chargement complet et le menu principal
- [ ] Mesurer heap/linéaire réellement utilisés ; réduire si besoin
- [ ] Vérifier le vol : monde en haut, HUD en bas, boutons tactiles
- [ ] Vérifier une planète / boutique avec la loupe tactile
- [ ] Construire un CIA avec mémoire étendue (makerom) et le documenter
- [ ] Retirer les traces DEBUG de `main.cpp` avant la version finale
- [ ] Mettre à jour le README (état, limitations)
