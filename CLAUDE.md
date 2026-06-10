# Notes projet pour Claude

**Lis d'abord [README.md](README.md)** : il décrit la structure complète du
projet, la chaîne de build/flash, les tests, et le rôle de chaque dossier.

## En bref

Demake top-down GTA pour **Gamebuino META** (SAMD21 / Cortex-M0+, écran 80×64,
flash et RAM très limitées). Le firmware vit dans `gta/` ; les données
(`gta/*_data.cpp` et certains `.h`) sont **générées** par `tools/build_*.py` —
édite la source (PNG, `assets/tileset.csv`, `city/city.txt`), pas la sortie.

## Règles importantes

- **Parité .h / .py** : `gta/engine.h`↔`tools/engine.py` et
  `gta/ai.h`↔`tools/ai.py` doivent rester en parité. Les `host_test/` (g++
  natif) verrouillent ce comportement — lance-les après toute modif de logique.
- **Contraintes embarquées** : pas de gros tableaux en RAM, éviter le flottant
  par pixel, blitter des sprites pré-rendus plutôt que recalculer. La flash est
  limitée (~47 % utilisée) ; surveille la taille à la compilation.
- **Ne pas éditer** les fichiers marqués « généré par tools/… — NE PAS éditer ».
- **Build** : flag `FOLDER_NAME` obligatoire (dossier `gta` < 4 chars). Commande
  exacte dans le README.
- **Aperçus** : le client n'affiche pas les images inline ; écris les PNG de
  debug dans `previews/`.
