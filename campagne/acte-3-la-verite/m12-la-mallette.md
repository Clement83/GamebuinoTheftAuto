# M12 — La mallette

*ACTE III — La vérité (M12–M15) · Sarah, les preuves*

> Script fidèle à l'implémentation (`OBJS_M12`, `game_state.h`). La « mallette »
> est **purement narrative** : pas d'objet à ramasser au sol (pas de
> `OBJ_PICKUP` dans le moteur). On la « récupère » en survivant au combat puis en
> quittant la zone.

- **Entrée** — `MISSIONS[26]`, `STORY_SEQ[11]`. `isStory`.
- **Déclencheur** — téléphone **rouge** à la Planque (`campaignStep == 11`).
- **Prime** — **350 $**.
- **Échec** — aucun.

## Objectifs (séquence moteur)

### 1. `OBJ_GOTO` → Chinatown *(rayon 14)*
- **Narration** — *« Sarah (numéro inconnu) : je peux prouver ce que Rico a dit.
  Un parking, à Chinatown. »*
- **Action joueur** — rejoindre le marqueur (le parking) à Chinatown.
- **Atteint** — *« Un homme mort, une mallette près du corps. Mais tu n'es pas
  seul... »*

### 2. `OBJ_KILL` → les nettoyeurs *(2 × `EK_GUNNER`, `SP_AMBUSH`)*
- **Narration** — *« Des nettoyeurs viennent pour la mallette. Prends-les de
  vitesse. »*
- **Script** — `enemyCount == 2`, aucune cible nommée ; 2 **tireurs** posés mais
  **passifs** jusqu'à l'approche, puis tirent. KILL rempli quand tous à terre.
- **Action joueur** — neutraliser les 2 tireurs.
- **Complétion** — `enemiesAlive == 0`.
- **Atteint** — *« Ramasse la mallette et file. »*

### 3. `OBJ_GOTO` → Planque *(rayon 14)*
- **Narration** — *« Rapporte la mallette à la Planque. »*
- **Action joueur** — rejoindre le marqueur à la Planque.
- **Atteint** — *« Sarah : c'est bien ce que je craignais. Il faut qu'on se
  voie. »*

## Clôture
→ `finishMission()` : **+350 $**, **MISSION ACCOMPLIE**, `campaignStep → 12`.
