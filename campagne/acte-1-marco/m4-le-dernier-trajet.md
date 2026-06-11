# M4 — Le dernier trajet *(pivot tragique)*

*ACTE I — Marco (M1–M4) · tutoriel déguisé*

- **Appel** — *Marco : « Après aujourd'hui, je quitte peut-être la ville. »*
- **Lieux** — Le Garage (prendre Marco), **Chantier**.
- **Objectifs**
  1. `OBJ_ENTER_CAR` → caisse au Garage. *« Prends la caisse. »*
  2. `OBJ_ESCORT` `requireCar` → Le Garage prendre Marco → `EV_ALLY_JOIN`.
     *« Marco : encore un dernier rendez-vous. »*
  3. `OBJ_GOTO` `requireCar` → **Chantier**. *« Conduis-moi au Chantier. »*
  4. À l'arrivée : `EV_REVEAL` (PNJ mystérieux) → court dialogue → **BANG** →
     `EV_ALLY_DIE` : *« C'était un piège… »* Le tueur s'enfuit.
  5. `OBJ_KILL` → poursuivre et tuer le **tueur** (IA `missionChaseStep`).
- **Événements** — `EV_ALLY_JOIN`, `EV_REVEAL`, `EV_ALLY_DIE`.
- **Fin** — `EV_CALL` : le tél sonne. *Inconnu : « Tu veux savoir pourquoi il
  est mort ? Réponds au prochain appel. »*
- **Échec** — si le tueur s'échappe (hors map) : **échec** → retour Planque.
- **Prime** — 250 $.
- **Améliorations** — reprend exactement la structure de l'ancienne
  « Mauvaise affaire » (Marco passager → mort → tueur qui fonce), désormais
  **dans la trame** au lieu d'une cabine bleue.
