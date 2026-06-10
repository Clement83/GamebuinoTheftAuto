# Voitures IA : solidité, éjection, état de fuite

Date : 2026-06-10 — branche `feat/voitures-destructibles-explosifs`.

Suite du dev « véhicules qui peuvent blesser ». Corrige le bug du joueur coincé
sous une voiture et ajoute un comportement de fuite au trafic.

## Comportements visibles cibles

1. **Voiture à l'arrêt OU très lente = solide, sans dégât.** Si le joueur à pied
   chevauche une voiture qui n'a pas (assez) roulé cette frame (`blocked`, plus
   de conducteur, ou vitesse < seuil), on le **repousse hors de la boîte** ; 0
   cœur, pas de chute. Plus de blocage « sous » la voiture.

2. **Écrasement par une voiture qui roule (normale) → arrêt + conducteur sort +
   éjection.** Voiture en mouvement (vitesse ≥ seuil) et non en fuite qui touche
   le joueur → elle s'immobilise (`c.driver = false`), le conducteur descend en
   PNJ paniqué (`aiEjectDriver`), le joueur est **projeté** (`knockdownPlayer` +
   poussée recalée via `findFootSpot`), **−1 cœur**.

3. **État de FUITE des voitures.** Une voiture en fuite roule plus vite, ne
   freine plus (ni piétons ni joueur) et **écrase en continuant** (−1 cœur +
   chute pour le joueur ; `deadPed` pour les PNJ). Timer décroissant → retour
   normal.

4. **Les voitures normales freinent pour les piétons** (pas seulement le joueur) :
   un piéton debout dans la voie arrête une voiture normale. Une voiture en
   fuite/bélier ignore ce test (→ « ne font plus attention aux passants »).

## Implémentation (gta.ino, firmware-only — pas de parité .py)

### Données
- `AiCar` gagne `bool fleeing; uint16_t fleeTimer;` (init à `false`/`0` dans
  `aiRespawnCar` et le reset l.720). `AiCar` est absent de `ai.py` → aucune
  rupture de parité, host tests inchangés.

### Constantes (près des autres consts voiture/IA)
- `CAR_FLEE_FRAMES` (~durée de fuite, ex. 90).
- `CAR_FLEE_SPEED` (> `AI_CAR_SPEED`, ex. 1.3).
- `CAR_RUNOVER_SPEED2` (seuil vitesse² d'écrasement, < `AI_CAR_SPEED²`=0.64,
  ex. 0.30 — au-dessus = écrase, en-dessous = solide).
- `CAR_EJECT_PUSH` (px de projection du joueur, ex. 10).
- Rayon d'alarme : réutiliser `GUNSHOT_PANIC_RANGE` / `KILL_PANIC_RANGE` /
  `BOOM_PANIC_RADIUS`.

### Helpers
- `startCarFlee(AiCar &c)` : si `!c.isPolice`, `c.fleeing = true; c.fleeTimer =
  CAR_FLEE_FRAMES;`.
- `alarmCarsAround(int sx, int sy, int radius)` : boucle `aiCars`, `startCarFlee`
  si actif & dans le rayon. Branché aux mêmes sites que `startPanic` piétons :
  tir joueur (l.1632), meurtre (l.1220), explosion (l.1348).
- Poussée joueur hors boîte (réutilise la résolution px/py de la collision
  voiture↔voiture l.1742) et éjection (poussée + `findFootSpot`).

### Boucle de mise à jour des voitures IA (l.1690-1751)
- Capturer `ox0,oy0 = c.x,c.y` avant le pas ; après, `moved2 = dx²+dy²`.
- `ignorePeople = c.fleeing || ram`.
- `blocked` : exclut le joueur si `ignorePeople` ; ajoute un test « piéton dans
  la voie » si `!ignorePeople` (forme identique au test épave).
- Décrémenter `fleeTimer`, vitesse de pas = `CAR_FLEE_SPEED` si `fleeing` sinon
  `AI_CAR_SPEED`.
- Collision joueur à pied (remplace l'appel `hitPlayerByCar` inconditionnel) :
  - `moved2 < CAR_RUNOVER_SPEED2` → solide : repousse, 0 dégât.
  - sinon si `ignorePeople` → écrase + continue (−1 cœur, chute, poussée).
  - sinon → arrêt + conducteur sort + éjection (−1 cœur, chute, poussée).
- Piétons : tués (`deadPed`) seulement si la voiture roule **et** `ignorePeople`
  (sinon elle a freiné, pas de kill).
- Voiture percutée par la caisse du joueur (l.1740) → `startCarFlee`.

### Verif
- Compile arduino-cli (flag `FOLDER_NAME`), surveiller la flash.
- Host tests `ai`/`wanted` : doivent rester verts (logique inchangée côté .py).
- Test final sur Gamebuino par l'utilisateur.
