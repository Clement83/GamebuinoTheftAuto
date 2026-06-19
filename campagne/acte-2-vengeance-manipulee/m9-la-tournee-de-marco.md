# M9 — La tournée de Marco *(callback émotionnel)*

*ACTE II — La vengeance manipulée (M5–M11) · fausse piste, les Loups*

> Script fidèle à l'implémentation (`OBJS_M9`, `game_state.h`). Reprend les
> lieux de M2 (la tournée), désormais sans Marco.

- **Entrée** — `MISSIONS[23]`, `STORY_SEQ[8]`. `isStory`.
- **Déclencheur** — téléphone **rouge** à la Planque (`campaignStep == 8`).
- **Prime** — **300 $**.
- **Échec sélectif (règle racket)** — les stops coopératifs (Commerces, Le Bar)
  posent un **client** (`EV_CLIENT`). **Le tuer** → **MISSION RATÉE**
  (*« On rackette, on ne tue pas les clients ! »*). Même mécanique qu'en M2.

## Objectifs (séquence moteur)

### 1. `OBJ_GOTO` → Les Commerces *(rayon 14)*
- **Narration** — *« Tony : reprends la tournée de Marco aux Commerces. C'est toi,
  le patron, maintenant. »*
- **Action joueur** — rejoindre le marqueur aux Commerces.
- **Atteint** — *« Un commerçant : c'est toi qui passes, désormais ? ...Désolé,
  pour Marco. »*

### 2. `OBJ_BEAT` → des Loups *(3 × `EK_THUG`, `SP_PRESENT`)*
- **Narration** — *« Des Loups sont venus se servir. Renvoie-les chez eux. »*
- **Script** — 3 gros bras visibles d'emblée, qui foncent.
- **Action joueur** — les mettre au tapis.
- **Complétion** — `enemiesAlive == 0`.
- **Atteint** — *« Pour toi, Marco. »*

### 3. `OBJ_GOTO` → Le Bar *(rayon 14)*
- **Narration** — *« Le vieux du Bar a vu quelque chose. Va l'écouter. »*
- **Action joueur** — rejoindre le marqueur au Bar.
- **Atteint** — *« Le vieux : un type chic donnait des ordres aux Loups... Va
  savoir qui. »* (semence du vrai commanditaire)

## Clôture
→ `finishMission()` : **+300 $**, **MISSION ACCOMPLIE**, `campaignStep → 9`.
