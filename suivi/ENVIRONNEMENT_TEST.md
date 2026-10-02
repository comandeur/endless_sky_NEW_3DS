# Environnement de test (session cloud)

- Toolchain : image Docker `devkitpro/devkitarm` (devkitARM r68, libctru 2.7, citro3d 1.7.1).
- Build : `tools/docker-build.sh` → `build/endless-sky.3dsx`.
- Assets : `tools/convert-assets.sh` (ou `convert.py` avec `--tex3ds`/`--masktool`).
- Émulateur : Panda3DS compilé depuis les sources (GCC, frontend SDL, serveur HTTP),
  sous Xvfb (`DISPLAY=:99`). Correctifs locaux apportés à l'émulateur :
  - `PANDA3DS_ENABLE_HTTP_SERVER` en PUBLIC (sinon disposition d'objet incohérente → crash) ;
  - endpoints `/touch?x=&y=` et `/circle?x=&y=` pour simuler le tactile et le stick ;
  - affichage des registres invités (PC/LR) sur accès mémoire invalide ;
  - trace de la pile sur `svcUnmapMemoryBlock`.
- SD virtuelle : `~/.local/share/Alber/endless-sky/SDMC/3ds/endless-sky/`.
- Pilotage : `curl localhost:1234/screen` (capture PNG 400×480), `/input?A=1`, `/touch`, `/circle`.
- Journal du jeu : `config/errors.txt` sur la SD ; erreurs fatales : `fatal-error.txt`.
- Symbolisation : `arm-none-eabi-addr2line -f -C -i -e build/endless-sky.elf <adresse>`.

Limites de l'émulateur (pas des bugs du portage) : pas de DSP (pas de son), pas de relecture
des cibles de rendu, `stat` des dossiers incorrect, mémoire appli 80 Mo par défaut.
