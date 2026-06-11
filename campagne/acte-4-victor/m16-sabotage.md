# M16 — Sabotage *(intègre le broyeur de La Casse)*

*ACTE IV — Victor (M16–M18 + épilogue)*

- **Appel** — *Tony : « On va lui faire mal au portefeuille. »*
- **Lieux** — rue chic (repère) → **La Casse**.
- **Objectifs**
  1. `OBJ_STEAL_CAR` voiture de luxe #1 ; `OBJ_GOTO requireCar` → **La Casse** →
     la **broyer** (déclenche `SEQ_CRUSH` existant). *« Une de moins. »*
  2. `OBJ_STEAL_CAR` voiture de luxe #2 ; rebelote → broyeur. *« Tony : ça va le
     rendre fou. »*
- **Échec** — aucun (si la caisse est détruite avant la Casse, re-spawn de la
  cible). **Prime** — 550 $.
- **Améliorations** — concrétise ton idée *« va le faire broyer à la casse »* :
  on **réutilise la grue/broyeur déjà livré** (cf. POI.md §4) comme **objectif
  de sabotage**, pas seulement comme vente. Belle synergie avec l'existant.
