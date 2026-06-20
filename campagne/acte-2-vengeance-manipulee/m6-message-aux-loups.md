# M6 — Message aux Loups

*ACTE II — La vengeance manipulée (M5–M11) · fausse piste, les Loups*

> Script fidèle à l'implémentation (`OBJS_M6`, `game_state.h`).

- **Entrée** — `MISSIONS[20]`, `STORY_SEQ[5]`. `isStory`.
- **Déclencheur** — téléphone **rouge** à la Planque (`campaignStep == 5`).
- **Prime** — **250 $**.
- **Échec** — aucun.

## Objectifs (séquence moteur)

### 1. `OBJ_GOTO` → Chinatown *(rayon 16)*
- **Narration** — *« Inconnu : envoie-leur un message. Va dans le quartier des
  Loups. »*
- **Action joueur** — rejoindre le marqueur à Chinatown (quartier des Loups).
- **Atteint** — *« Les voilà, accoudés au mur. Ils t'ont vu. »*

### 2. `OBJ_BEAT` → trois Loups *(3 × `EK_THUG`, `SP_PRESENT`, `taunt=1`)*
- **Narration** — *« Mets trois Loups au tapis. Qu'ils comprennent. »*
- **Script** — 3 gros bras posés et **visibles d'emblée**.
- **Face-à-face** (`taunt=1`, à pied) — à l'activation de l'objectif,
  `startTauntCut` fige le joueur le temps de deux répliques, keyées par titre
  *« Message aux Loups »* dans `ambushTauntLines` :
  - l1 — *« Un Loup, crâneur : t'es perdu, l'ami ? C'est pas ton quartier. »*
  - l2 — *« Un autre : regarde-le... il va comprendre. Chopez-le ! »*
  Puis les Loups chargent et la bagarre démarre.
- **Action joueur** — mettre les **3** au sol.
- **Complétion** — `enemiesAlive == 0`.
- **Atteint** — *« Ça devrait attirer leur attention. Inconnu : bien joué. »*

## Conséquence systémique (à la complétion du BEAT)
`applyMissionScriptFx` (keyé titre *« Message aux Loups »* + `done.type == OBJ_BEAT`) :
- force le niveau de recherche police à **2 étoiles** — `scriptForceWanted(2)` ;
- narre — *« Un temoin appelle les flics ! Seme-les ou file au Pay'n'Spray. »*

Comme la mission se **termine** sur ce BEAT, la chauffe se vit **en roue libre
après**, sans risque d'échec de mission : première leçon de la boucle
recherche → repeinture (Pay'n'Spray).

## Clôture
→ `finishMission()` : **+250 $**, **MISSION ACCOMPLIE**, `campaignStep → 6`.
