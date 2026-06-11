# Campagne Acte I (M1–M4) — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Brancher le téléphone rouge séquentiel et les 4 missions de l'Acte I (Marco) sur le moteur de missions existant, avec écran d'échec et progression `campaignStep`.

**Architecture:** Logique pure (types d'objectifs `OBJ_TALK`/`OBJ_SUBDUE`, flag `isStory`) ajoutée dans `gta/mission.h` et verrouillée par les tests host g++. La glue (`gta/gta.ino`) ajoute `campaignStep`, une table `STORY_SEQ`, le déclenchement/réarmement du téléphone rouge, l'overlay d'échec, et généralise le spawn d'entités (Marco, tueur, cible de SUBDUE) pour qu'il soit piloté par les champs de l'objectif au lieu d'un index de mission codé en dur.

**Tech Stack:** C++11 (Arduino/SAMD21 + tests host g++), Python (générateurs, non touchés ici).

---

## Contexte (déjà en place — ne pas refaire)

- Moteur `mission.h` : `OBJ_GOTO/ENTER_CAR/KILL/BEAT/SURVIVE`, IA fuite/poursuite, `missionObjectiveDone`, `missionAdvance`, `missionTimedOut`.
- `gta.ino` : `MISSIONS[]` (16 entrées, index 0..15), `PHONES[]` (cabines bleues), 1 téléphone rouge `STORY_PHONES[0]` près du POI **Planque** (ne sonne pas), narration `narrate()`, `startMission()`, `missionProgress()`, `enterObjective()`, `buildMissionRuntime()`, `failMission()`.
- POIs générés disponibles via `findPoi(...)` : `"Le Garage"`, `"Commerces"`, `"Chantier"`, `"Les Quais"`, `"Planque"`, etc. **Aucune création de POI nécessaire.**
- Le swap §9 (pizza remplace la cabine Marco) est déjà fait.

## Conventions de build/test (exécuter depuis la racine du dépôt)

- **Tests host (logique pure)** :
  ```
  g++ -std=c++11 -I gta tools/host_test/test_mission_host.cpp -o /tmp/mh && /tmp/mh
  ```
  Attendu : `OK : ...` et code retour 0.
- **Compilation device (vérifie que la glue compile + taille flash)** :
  ```
  arduino-cli compile --fqbn gamebuino:samd:gamebuino_meta_native \
    --build-property 'compiler.cpp.extra_flags=-DFOLDER_NAME="GTADEMAKE"' gta
  ```
  Attendu : compilation OK, noter le `%` de flash affiché.

## File Structure

- `gta/mission.h` — **modifié** : 2 nouveaux `ObjType` (`OBJ_TALK`, `OBJ_SUBDUE`), champ `MissionState.subdueCount`, champ `MissionDef.isStory`, branches dans `missionObjectiveDone`. Logique pure uniquement.
- `tools/host_test/test_mission_host.cpp` — **modifié** : tests des 2 nouveaux objectifs + enchaînement type M1.
- `gta/gta.ino` — **modifié** : `campaignStep`, `STORY_SEQ[]`, `storyMissionActive`, `missionFailedTimer`, contenu missions M1/M2/M3 + M4 (réusinage d'`OBJS_DEAL`), généralisation de `enterObjective`, gestion SUBDUE en combat, sonnerie/réarmement du téléphone rouge, overlay d'échec, échec M4.

---

## Phase 1 — Moteur pur (mission.h + host tests, TDD)

### Task 1: OBJ_TALK (proximité à pied)

**Files:**
- Modify: `gta/mission.h` (enum `ObjType` ~ligne 146 ; `missionObjectiveDone` ~ligne 204)
- Test: `tools/host_test/test_mission_host.cpp`

- [ ] **Step 1: Écrire le test qui échoue**

Ajouter dans `main()` de `tools/host_test/test_mission_host.cpp`, juste après le bloc « Objectif SURVIVE » (après la ligne `check("SURVIVE jamais timeout", ...)`):

```cpp
  // --- Objectif TALK : valide a pied dans le petit rayon (comme GOTO sans voiture) ---
  {
    Objective o = { OBJ_TALK, 100, 100, 6, false, EV_NONE, nullptr, "parle" };
    MissionState s = {};
    s.actorCx = 103; s.actorCy = 100;        // 3 px -> dans rayon 6
    check("TALK dans rayon", missionObjectiveDone(o, s));
    s.actorCx = 130; s.actorCy = 100;        // 30 px -> hors rayon
    check("TALK hors rayon", !missionObjectiveDone(o, s));
  }
```

- [ ] **Step 2: Lancer le test, vérifier l'échec de compilation**

Run: `g++ -std=c++11 -I gta tools/host_test/test_mission_host.cpp -o /tmp/mh && /tmp/mh`
Expected: erreur de compilation `'OBJ_TALK' was not declared in this scope`.

- [ ] **Step 3: Ajouter `OBJ_TALK` à l'enum**

Dans `gta/mission.h`, dans `enum ObjType`, ajouter après `OBJ_SURVIVE = 4,` :

```cpp
  OBJ_TALK      = 5,  // s'approcher d'un PNJ nomme (a pied, petit rayon) : auto-dialogue
```

- [ ] **Step 4: Ajouter la branche dans `missionObjectiveDone`**

Dans `gta/mission.h`, dans le `switch (o.type)` de `missionObjectiveDone`, ajouter après le `case OBJ_SURVIVE:` :

```cpp
    case OBJ_TALK: {                        // proximite a pied (jamais requireCar)
      long dx = s.actorCx - o.x, dy = s.actorCy - o.y;
      return dx * dx + dy * dy <= (long)o.radius * o.radius;
    }
```

- [ ] **Step 5: Lancer le test, vérifier qu'il passe**

Run: `g++ -std=c++11 -I gta tools/host_test/test_mission_host.cpp -o /tmp/mh && /tmp/mh`
Expected: `OK : ...`, code retour 0.

- [ ] **Step 6: Commit**

```bash
git add gta/mission.h tools/host_test/test_mission_host.cpp
git commit -m "feat(mission): OBJ_TALK (proximite a pied) + test host"
```

### Task 2: OBJ_SUBDUE + MissionState.subdueCount

**Files:**
- Modify: `gta/mission.h` (enum `ObjType` ; struct `MissionState` ~ligne 194 ; `missionObjectiveDone`)
- Test: `tools/host_test/test_mission_host.cpp`

- [ ] **Step 1: Écrire le test qui échoue**

Ajouter dans `main()` juste après le bloc TALK de la Task 1 :

```cpp
  // --- Objectif SUBDUE : valide quand la cible nommee a pris `count` coups ---
  {
    Objective o = { OBJ_SUBDUE, 0, 0, 0, false, EV_NONE, nullptr, "mate", nullptr, 3, 0 };
    MissionState s = {};
    s.subdueCount = 2;
    check("SUBDUE 2/3 -> non", !missionObjectiveDone(o, s));
    s.subdueCount = 3;
    check("SUBDUE 3/3 -> oui", missionObjectiveDone(o, s));
  }
```

- [ ] **Step 2: Lancer le test, vérifier l'échec**

Run: `g++ -std=c++11 -I gta tools/host_test/test_mission_host.cpp -o /tmp/mh && /tmp/mh`
Expected: erreur de compilation (`OBJ_SUBDUE` / `subdueCount` non déclarés).

- [ ] **Step 3: Ajouter le type et le champ d'état**

Dans `gta/mission.h`, dans `enum ObjType`, après la ligne `OBJ_TALK = 5,` :

```cpp
  OBJ_SUBDUE    = 6,  // frapper une cible nommee `count` fois : elle cede (ne meurt pas)
```

Dans `struct MissionState`, ajouter après `int beatCount;` :

```cpp
  int      subdueCount;   // coups portes a la cible de SUBDUE depuis le debut de l'objectif
```

- [ ] **Step 4: Ajouter la branche dans `missionObjectiveDone`**

Dans le `switch`, après le `case OBJ_TALK:` :

```cpp
    case OBJ_SUBDUE:    return s.subdueCount >= (int)o.count;
```

- [ ] **Step 5: Lancer le test, vérifier qu'il passe**

Run: `g++ -std=c++11 -I gta tools/host_test/test_mission_host.cpp -o /tmp/mh && /tmp/mh`
Expected: `OK : ...`, code retour 0.

- [ ] **Step 6: Commit**

```bash
git add gta/mission.h tools/host_test/test_mission_host.cpp
git commit -m "feat(mission): OBJ_SUBDUE + MissionState.subdueCount + test host"
```

### Task 3: Flag `isStory` sur MissionDef + test d'enchaînement type M1

**Files:**
- Modify: `gta/mission.h` (struct `MissionDef` ~ligne 178)
- Test: `tools/host_test/test_mission_host.cpp`

- [ ] **Step 1: Écrire le test qui échoue**

Ajouter dans `main()` après le bloc « Enchainement » existant (après le `check("advance3 -> fin mission", ...)`):

```cpp
  // --- Enchainement type M1 : GOTO->TALK(JOIN)->GOTO(car)->GOTO(car), isStory ---
  {
    static const Objective objs[] = {
      { OBJ_GOTO, 10, 10, 8, false, EV_NONE,       nullptr, "va au garage" },
      { OBJ_TALK, 10, 10, 6, false, EV_MARCO_JOIN, nullptr, "parle a Marco" },
      { OBJ_GOTO, 90, 90, 12, true, EV_NONE,       nullptr, "roule aux quais" },
      { OBJ_GOTO, 95, 95, 12, true, EV_NONE,       nullptr, "livre" },
    };
    MissionDef def = { "M1", objs, 4, 120, true };   // reward 120, isStory true
    check("isStory lu", def.isStory);
    MissionRun run = { 0, 0, true };
    check("M1 a0 GOTO", missionAdvance(run, def) == EV_NONE && run.step == 1);
    check("M1 a1 TALK->JOIN", missionAdvance(run, def) == EV_MARCO_JOIN && run.step == 2);
    check("M1 a2 GOTO car", missionAdvance(run, def) == EV_NONE && run.step == 3 && run.active);
    check("M1 a3 fin", missionAdvance(run, def) == EV_NONE && run.step == 4 && !run.active);
  }
```

- [ ] **Step 2: Lancer le test, vérifier l'échec**

Run: `g++ -std=c++11 -I gta tools/host_test/test_mission_host.cpp -o /tmp/mh && /tmp/mh`
Expected: erreur de compilation (`MissionDef` n'a pas de membre `isStory`).

- [ ] **Step 3: Ajouter le champ `isStory`**

Dans `gta/mission.h`, dans `struct MissionDef`, ajouter après `int16_t reward;` (en fin de struct, pour préserver les init agrégat existantes) :

```cpp
  bool isStory;    // mission de trame principale (téléphone rouge) ? (false = secondaire)
```

- [ ] **Step 4: Lancer le test, vérifier qu'il passe**

Run: `g++ -std=c++11 -I gta tools/host_test/test_mission_host.cpp -o /tmp/mh && /tmp/mh`
Expected: `OK : ...`, code retour 0.

- [ ] **Step 5: Commit**

```bash
git add gta/mission.h tools/host_test/test_mission_host.cpp
git commit -m "feat(mission): flag isStory sur MissionDef + test enchainement M1"
```

---

## Phase 2 — Généralisation du spawn (glue, build-verified)

> Objectif : remplacer les conditions `missionRun.def == MISSION_DEAL` codées en dur par des règles pilotées par les champs de l'objectif, afin que M1 (TALK + escorte) et M4 (escorte + tueur) marchent sans nouveau code par mission.

### Task 4: Drapeau `killerChase` + spawn piloté par les champs

**Files:**
- Modify: `gta/gta.ino` (`enterObjective` ~ligne 2254 ; `missionProgress` ~ligne 2307 ; `startMission` ~ligne 2276 ; bloc état runtime ~ligne 649)

- [ ] **Step 1: Ajouter l'état runtime `killerChase` et `objSubdue`**

Dans `gta/gta.ino`, après la ligne `static uint16_t objElapsed = 0;` (~ligne 651) :

```cpp
static int      objSubdue = 0;            // coups portes a la cible de SUBDUE (objectif courant)
static bool     killerChase = false;      // le prochain OBJ_KILL spawne un TUEUR qui fonce (post EV_MARCO_DIE)
```

- [ ] **Step 2: Généraliser `enterObjective` (spawn par champ d'objectif)**

Remplacer le corps de `enterObjective()` (lignes ~2255–2273) par :

```cpp
static void enterObjective() {
  const MissionDef &def = curDef;
  const Objective &o = def.objectives[missionRun.step];
  narrate(o.text);
  objBeat = 0; objElapsed = 0; objSubdue = 0;   // compteurs propres a cet objectif
  if (o.event == EV_MARCO_JOIN) {
    marcoWaiting = true;                          // Marco debout, attend la prise (TALK ou GOTO)
  }
  if (o.type == OBJ_ENTER_CAR) {
    mCar.x = o.x; mCar.y = o.y; mCar.angle = 0.0f; mCar.vx = 0.0f; mCar.vy = 0.0f;
    mCarActive = true;
  } else if (o.type == OBJ_KILL && !target.active) {
    if (killerChase) spawnTargetAt(o.x, o.y);     // tueur (post mort de Marco) : fonce
    else             spawnTargetWanderNear(o.x, o.y);  // PNJ qui erre/fuit (Joe, debiteur)
  } else if (o.type == OBJ_SUBDUE && !target.active) {
    spawnTargetAt(o.x, o.y);                       // cible nommee, immobile (le commercant)
    target.chase = false;
  } else if (o.type == OBJ_SURVIVE) {
    int pcx = driving ? (int)car.x : playerX + PLAYER_W / 2;
    int pcy = driving ? (int)car.y : playerY + PLAYER_H / 2;
    spawnTargetAt(pcx, pcy);
  }
}
```

Note : `marcoWaiting` est posé via `o.event == EV_MARCO_JOIN`, ce qui couvre M1 (TALK obj 1) et M4 (GOTO obj 1). `drawMarco` lit `curObjs[1]` : garder l'objectif « Marco » à l'index 1 dans M1 et M4 (cf. Tasks 11/13).

- [ ] **Step 3: Poser/relâcher `killerChase` dans `missionProgress` et `startMission`**

Dans `startMission()` (~ligne 2276), après `mCarActive = false;` ajouter :

```cpp
  killerChase = false;
```

Dans `missionProgress()` (~ligne 2327), dans la branche `else if (ev == EV_MARCO_DIE)`, ajouter `killerChase = true;` :

```cpp
  } else if (ev == EV_MARCO_DIE) {
    marcoAboard = false; killerChase = true;
    const Objective &k = def.objectives[missionRun.step];  // KILL : coords du tueur
    targetDownX = k.x; targetDownY = k.y; targetDownTimer = PED_DOWN_FRAMES;
  }
```

- [ ] **Step 4: Compiler le device, vérifier OK**

Run: `arduino-cli compile --fqbn gamebuino:samd:gamebuino_meta_native --build-property 'compiler.cpp.extra_flags=-DFOLDER_NAME="GTADEMAKE"' gta`
Expected: compilation réussie. Noter le `%` flash.

- [ ] **Step 5: Commit**

```bash
git add gta/gta.ino
git commit -m "refactor(mission): spawn pilote par les champs d'objectif (retire le hardcode MISSION_DEAL)"
```

### Task 5: Coup de SUBDUE en combat (la cible cède au lieu de mourir)

**Files:**
- Modify: `gta/gta.ino` (frappe de la cible de mission ~ligne 1849 ; `missionProgress` remplissage `MissionState` ~ligne 2310)

- [ ] **Step 1: Brancher `subdueCount` dans l'instantané d'état**

Dans `missionProgress()`, après `s.beatCount = objBeat;` (~ligne 2316) ajouter :

```cpp
  s.subdueCount = objSubdue;
```

- [ ] **Step 2: Détourner la frappe de la cible vers SUBDUE**

Remplacer le bloc « La cible de mission est frappable » (lignes ~1848–1851) par :

```cpp
  // La cible de mission est frappable comme un pieton (toujours dans le cone).
  if (missionRun.active && target.active &&
      combatInCone(target.x, target.y, pcx, pcy, playerDir, wd.reach, wd.side)) {
    if (curObjs[missionRun.step].type == OBJ_SUBDUE) {
      objSubdue++;                          // elle cede, elle ne meurt pas
      gb.sound.tone(150, 50);
    } else {
      killTarget(pcx, pcy);
    }
  }
```

- [ ] **Step 3: Empêcher la cible de SUBDUE de bouger**

Dans `missionUpdate()` (~ligne 2345), au tout début après le garde `if (!missionRun.active || !target.active) return;`, ajouter :

```cpp
  if (curObjs[missionRun.step].type == OBJ_SUBDUE) { return; }   // cible immobile (le commercant)
```

- [ ] **Step 4: Compiler le device**

Run: `arduino-cli compile --fqbn gamebuino:samd:gamebuino_meta_native --build-property 'compiler.cpp.extra_flags=-DFOLDER_NAME="GTADEMAKE"' gta`
Expected: compilation réussie.

- [ ] **Step 5: Commit**

```bash
git add gta/gta.ino
git commit -m "feat(mission): coups de SUBDUE (la cible cede au lieu de mourir)"
```

---

## Phase 3 — Socle campagne : téléphone rouge séquentiel (glue)

### Task 6: `campaignStep`, `STORY_SEQ`, sonnerie + décrochage du téléphone rouge

**Files:**
- Modify: `gta/gta.ino` (état runtime ~ligne 638 ; `MISSIONS[]`/contenu — table `STORY_SEQ` ; `drawPhones` ~ligne 2611 ; gestion du A sur le rouge ~ligne 2293 ; `buildMissionRuntime` ~ligne 2241 ; `missionProgress` fin ~ligne 2333)

- [ ] **Step 1: Ajouter l'état de campagne**

Dans `gta/gta.ino`, après `static MissionRun missionRun = { 0, 0, false };` (~ligne 639) :

```cpp
static uint8_t campaignStep = 0;          // prochaine mission de trame (0..3 = M1..M4 ; 4 = Acte I fini)
static bool    storyMissionActive = false;// la mission en cours est-elle une mission de trame ?
```

- [ ] **Step 2: Définir `STORY_SEQ` (indices des missions de trame dans MISSIONS[])**

> Les entrées M1/M2/M3 sont ajoutées à `MISSIONS[]` aux Tasks 9/10/11 ; M4 réutilise l'index 1 (`MISSION_DEAL`) réusiné en Task 12. Pour permettre la compilation incrémentale, on pose d'abord la table pointant 4× sur l'index 1, puis on corrige aux tasks de contenu.

Après la définition de `NUM_MISSIONS` (~ligne 547), ajouter :

```cpp
// Sequence des missions de trame : campaignStep -> index dans MISSIONS[].
// (M1/M2/M3 ajoutees en fin de MISSIONS[] ; M4 = MISSION_DEAL reusine.)
#define ACT1_LAST 4
static const uint8_t STORY_SEQ[ACT1_LAST] = { MISSION_DEAL, MISSION_DEAL, MISSION_DEAL, MISSION_DEAL };
```

- [ ] **Step 3: Copier `isStory` dans `buildMissionRuntime`**

Dans `buildMissionRuntime()`, après `curDef.reward = src.reward;` (~ligne 2251) :

```cpp
  curDef.isStory = src.isStory;
```

- [ ] **Step 4: Faire sonner le téléphone rouge**

Dans `drawPhones()` (~ligne 2611), remplacer la boucle des story phones par :

```cpp
  bool storyRings = (campaignStep < ACT1_LAST) && !missionRun.active;
  for (int i = 0; i < NUM_STORY_PHONES; i++)
    drawPhoneBooth(camX, camY, storyPx[i], storyPy[i], PHONE_BODY_STORY, storyRings);
```

- [ ] **Step 5: Décrocher le rouge démarre la mission de trame courante**

Dans la gestion du bouton A, remplacer le bloc « Cabine rouge de la campagne » (lignes ~2293–2302) par :

```cpp
      // Cabine rouge de la campagne : decroche -> mission de trame courante.
      if (!answered && !missionRun.active) {
        long dS = (long)(pcx - storyPx[0]) * (pcx - storyPx[0])
                + (long)(pcy - storyPy[0]) * (pcy - storyPy[0]);
        if (dS <= (long)PHONE_REACH * PHONE_REACH) {
          if (campaignStep < ACT1_LAST) {
            storyMissionActive = true;
            startMission(STORY_SEQ[campaignStep]);
          } else {
            narrate("Le telephone reste muet.");
            gb.sound.playOK();
          }
          answered = true;
        }
      }
```

- [ ] **Step 6: Avancer `campaignStep` à la complétion d'une mission de trame**

Dans `missionProgress()`, dans la branche de fin de mission (`else { ... prime ... }` ~ligne 2333), remplacer par :

```cpp
  else {                                           // mission terminee : prime en $
    if (def.reward > 0) {
      playerMoney += def.reward;
      char msg[20]; snprintf(msg, sizeof(msg), "Mission ! +$%d", (int)def.reward);
      narrate(msg);
    }
    gb.sound.tone(988, 60); gb.sound.playOK();     // cha-ching de fin de mission
    if (storyMissionActive) {                      // progression de la trame
      storyMissionActive = false;
      if (campaignStep < ACT1_LAST) campaignStep++;
      if (campaignStep >= ACT1_LAST) narrate("Marco n'est plus. Quelque chose a change.");
    }
  }
```

Aussi : remettre `storyMissionActive = false;` dans `startMission()` n'est PAS souhaitable (on le pose juste avant l'appel). Mais dans les chemins d'échec (`failMission`, `reviveCommon`) il faut le relâcher — voir Task 7.

- [ ] **Step 7: Compiler le device**

Run: `arduino-cli compile --fqbn gamebuino:samd:gamebuino_meta_native --build-property 'compiler.cpp.extra_flags=-DFOLDER_NAME="GTADEMAKE"' gta`
Expected: compilation réussie. (À ce stade les 4 missions de trame pointent toutes sur M4/DEAL ; corrigé en Phase 5.)

- [ ] **Step 8: Commit**

```bash
git add gta/gta.ino
git commit -m "feat(campagne): telephone rouge sequentiel + campaignStep (RAM)"
```

---

## Phase 4 — Écran d'échec (glue)

### Task 7: Overlay « MISSION ECHOUEE » + relâche de l'état de trame

**Files:**
- Modify: `gta/gta.ino` (état ~ligne 640 ; `failMission` ~ligne 2287 ; `reviveCommon` ~ligne 1405 ; boucle de rendu — appel de l'overlay ; gel d'input)

- [ ] **Step 1: Ajouter le timer d'échec**

Après `static bool storyMissionActive = false;` (Task 6 step 1) :

```cpp
static uint16_t missionFailedTimer = 0;   // >0 : overlay plein ecran "MISSION ECHOUEE"
static const uint16_t MISSION_FAIL_FRAMES = 50;  // ~2 s a 25 fps
```

- [ ] **Step 2: `failMission` arme l'overlay et relâche la trame**

Remplacer `failMission()` (~ligne 2287) par :

```cpp
static void failMission(const char *msg) {
  narrate(msg);
  missionRun.active = false;
  target.active = false; marcoWaiting = false; marcoAboard = false;
  mCarActive = false; carIsMission = false;
  killerChase = false; storyMissionActive = false;   // campaignStep inchange -> on rejoue la mission
  missionFailedTimer = MISSION_FAIL_FRAMES;
  gb.sound.playCancel();
}
```

- [ ] **Step 3: Relâcher la trame aussi en cas de mort/arrestation**

Dans `reviveCommon()` (~ligne 1405), dans le `if (missionRun.active) { ... }`, ajouter `storyMissionActive = false; killerChase = false;` :

```cpp
  if (missionRun.active) {                  // une mission en cours echoue
    missionRun.active = false; target.active = false;
    marcoWaiting = false; marcoAboard = false;
    mCarActive = false;
    storyMissionActive = false; killerChase = false;
  }
```

- [ ] **Step 4: Dessiner l'overlay et geler le monde**

Localiser, dans `loop()`, l'endroit après le rendu du monde et juste avant `gb.display` final / le dessin du HUD (chercher la fin du rendu de frame). Ajouter le dessin de l'overlay quand `missionFailedTimer > 0`. Code à insérer (dans la passe de rendu, après le blit du framebuffer monde, avant `gb.updateDisplay()` si applicable — l'exécutant repère le pattern de fin de frame existant) :

```cpp
  if (missionFailedTimer > 0) {
    // assombrir + texte centre
    for (int y = 24; y < 40; y++)
      for (int x = 0; x < SCREEN_W; x++)
        fb[y * SCREEN_W + x] = 0x0000;
    gb.display.setColor(RED);
    gb.display.setCursor(8, 28);
    gb.display.print("MISSION ECHOUEE");
  }
```

> Si le HUD texte du projet n'utilise pas `gb.display.print` mais une routine maison, suivre la routine de narration existante (`narrate`/affichage bandeau) pour centrer le texte. Vérifier visuellement.

- [ ] **Step 5: Décompter le timer et bloquer le gameplay pendant l'overlay**

Au tout début de la logique de mise à jour de `loop()` (avant la gestion des inputs/mouvement), ajouter :

```cpp
  if (missionFailedTimer > 0) {
    missionFailedTimer--;
    // on saute la mise a jour du monde pendant l'overlay (gel ~2 s) puis on rend.
    // -> placer ce garde de sorte a sauter input/mouvement mais conserver le rendu.
  }
```

L'exécutant adapte la structure de `loop()` du projet pour que, tant que `missionFailedTimer > 0`, l'input joueur et la physique soient sautés mais le rendu (dont l'overlay) ait lieu. Vérifier en jeu que l'overlay s'affiche ~2 s puis le contrôle revient.

- [ ] **Step 6: Compiler le device**

Run: `arduino-cli compile --fqbn gamebuino:samd:gamebuino_meta_native --build-property 'compiler.cpp.extra_flags=-DFOLDER_NAME="GTADEMAKE"' gta`
Expected: compilation réussie.

- [ ] **Step 7: Commit**

```bash
git add gta/gta.ino
git commit -m "feat(campagne): overlay MISSION ECHOUEE + relache de l'etat de trame"
```

### Task 8: Échec M4 — le tueur s'enfuit hors carte

**Files:**
- Modify: `gta/gta.ino` (`missionUpdate` ou `missionProgress` — détection cible hors limites)

- [ ] **Step 1: Détecter la fuite hors carte du tueur**

Dans `missionProgress()`, juste après le garde `if (!missionRun.active) return;` (~ligne 2308), ajouter :

```cpp
  // Echec de trame : un TUEUR (KILL en mode poursuite) qui sort des limites monde.
  if (target.active && killerChase && curObjs[missionRun.step].type == OBJ_KILL) {
    const int M = 8;  // marge px
    if (target.x < -M || target.y < -M ||
        target.x > CITY_W * TILE_W + M || target.y > CITY_H * TILE_H + M) {
      failMission("Le tueur s'est echappe.");
      return;
    }
  }
```

> Remarque : en pratique `missionChaseStep` ramène le tueur vers le joueur, donc l'échec ne se déclenche que si le joueur fuit longtemps. C'est l'effet voulu (filet de sécurité). Vérifier les constantes `CITY_W/CITY_H/TILE_W/TILE_H` existantes.

- [ ] **Step 2: Compiler le device**

Run: `arduino-cli compile --fqbn gamebuino:samd:gamebuino_meta_native --build-property 'compiler.cpp.extra_flags=-DFOLDER_NAME="GTADEMAKE"' gta`
Expected: compilation réussie.

- [ ] **Step 3: Commit**

```bash
git add gta/gta.ino
git commit -m "feat(campagne): echec M4 si le tueur s'echappe hors carte"
```

---

## Phase 5 — Contenu des missions (glue)

> Textes **courts** (flash). Chaque mission est une table `Objective[]` `const` + une entrée `MISSIONS[]` avec `isStory=true`. Champs `Objective` : `{ type, x, y, radius, requireCar, event, poi, text, doneText, count, limit }`. `x,y` = repli si POI absent (mis à 0 quand `poi` est résolu).

### Task 9: M1 — Premier jour

**Files:**
- Modify: `gta/gta.ino` (tables d'objectifs ~ligne 517 ; `MISSIONS[]` ~ligne 525 ; `STORY_SEQ`)

- [ ] **Step 1: Définir les objectifs de M1**

Après `OBJS_PIZZA` (~ligne 524), ajouter :

```cpp
// --- Trame Acte I ---
static const Objective OBJS_M1[] = {
  { OBJ_GOTO, 0, 0, 12, false, EV_NONE,       "Le Garage",
    "Marco t'attend au Garage. Vas-y a pied.", nullptr },
  { OBJ_TALK, 0, 0,  8, false, EV_MARCO_JOIN,  "Le Garage",
    "Parle a Marco.", "Marco : embarque, on a un colis a livrer." },
  { OBJ_GOTO, 0, 0, 16, true,  EV_NONE,        "Les Quais",
    "Conduis Marco aux Quais. Doucement.", nullptr },
  { OBJ_GOTO, 0, 0, 16, true,  EV_NONE,        "Les Quais",
    "Livre le colis.", "Colis livre. Marco : pas mal, petit." },
};
```

- [ ] **Step 2: Ajouter M1 à `MISSIONS[]` et brancher `STORY_SEQ[0]`**

Dans `MISSIONS[]`, après l'entrée pizza (~ligne 545), ajouter (l'index devient 16) :

```cpp
  { "Premier jour",     OBJS_M1, 4, 120, true },   // index 16 = M1 (trame)
```

Mettre à jour `STORY_SEQ` (Task 6 step 2) :

```cpp
static const uint8_t STORY_SEQ[ACT1_LAST] = { 16, MISSION_DEAL, MISSION_DEAL, MISSION_DEAL };
```

- [ ] **Step 3: Compiler + tester en jeu (boucle complète)**

Run: `arduino-cli compile --fqbn gamebuino:samd:gamebuino_meta_native --build-property 'compiler.cpp.extra_flags=-DFOLDER_NAME="GTADEMAKE"' gta`
Expected: compilation OK. **Vérification jouable** : au lancement, le téléphone rouge sonne près de la Planque ; décrocher démarre M1 ; aller au Garage, parler à Marco (il monte), conduire aux Quais, livrer ; prime +120 ; le rouge se réarme et resonne.

- [ ] **Step 4: Commit**

```bash
git add gta/gta.ino
git commit -m "feat(campagne): M1 Premier jour (GOTO/TALK/escorte)"
```

### Task 10: M2 — Les assurances (SUBDUE)

**Files:**
- Modify: `gta/gta.ino` (objectifs ; `MISSIONS[]` ; `STORY_SEQ[1]`)

- [ ] **Step 1: Définir les objectifs de M2**

> Simplification (cf. spec) : un seul POI `Commerces` ; le beat clé est le SUBDUE du commerçant. Promotion en 3 supérettes distinctes repoussée à M9 (Acte II).

Après `OBJS_M1`, ajouter :

```cpp
static const Objective OBJS_M2[] = {
  { OBJ_GOTO,   0, 0, 14, false, EV_NONE, "Commerces",
    "Tournee du racket. Suis Marco aux Commerces.", "Marco : regarde et apprends." },
  { OBJ_SUBDUE, 0, 0,  0, false, EV_NONE, "Commerces",
    "Le commercant refuse de payer. Corrige-le.", "Marco : voila comment on fait.",
    nullptr, 3, 0 },
};
```

- [ ] **Step 2: Ajouter M2 à `MISSIONS[]` et brancher `STORY_SEQ[1]`**

Après l'entrée M1 dans `MISSIONS[]` (index 17) :

```cpp
  { "Les assurances",   OBJS_M2, 2, 150, true },   // index 17 = M2 (trame)
```

`STORY_SEQ` :

```cpp
static const uint8_t STORY_SEQ[ACT1_LAST] = { 16, 17, MISSION_DEAL, MISSION_DEAL };
```

- [ ] **Step 3: Compiler + tester en jeu**

Run: `arduino-cli compile --fqbn gamebuino:samd:gamebuino_meta_native --build-property 'compiler.cpp.extra_flags=-DFOLDER_NAME="GTADEMAKE"' gta`
Expected: compilation OK. **Jouable** : M2 démarre après M1 ; aller aux Commerces ; un commerçant (cible) apparaît immobile ; le frapper 3× → il cède (ne meurt pas) → prime +150.

- [ ] **Step 4: Commit**

```bash
git add gta/gta.ino
git commit -m "feat(campagne): M2 Les assurances (SUBDUE commercant)"
```

### Task 11: M3 — Mauvaise dette (poursuite à pied)

**Files:**
- Modify: `gta/gta.ino` (objectifs ; `MISSIONS[]` ; `STORY_SEQ[2]`)

- [ ] **Step 1: Définir les objectifs de M3**

> Le débiteur réutilise la cible « qui erre/fuit » (comme Joe) via `OBJ_KILL` non-`killerChase` : on le coince à pied. (Pas de fleeing-SUBDUE à coder.)

Après `OBJS_M2`, ajouter :

```cpp
static const Objective OBJS_M3[] = {
  { OBJ_KILL, 0, 0, 0, false, EV_NONE, "Chinatown",
    "Un type nous doit de l'argent. Il traine a Chinatown.",
    "Dette reglee. Marco : il s'en souviendra." },
};
```

- [ ] **Step 2: Ajouter M3 à `MISSIONS[]` et brancher `STORY_SEQ[2]`**

Après l'entrée M2 dans `MISSIONS[]` (index 18) :

```cpp
  { "Mauvaise dette",   OBJS_M3, 1, 180, true },   // index 18 = M3 (trame)
```

`STORY_SEQ` :

```cpp
static const uint8_t STORY_SEQ[ACT1_LAST] = { 16, 17, 18, MISSION_DEAL };
```

- [ ] **Step 3: Compiler + tester en jeu**

Run: `arduino-cli compile --fqbn gamebuino:samd:gamebuino_meta_native --build-property 'compiler.cpp.extra_flags=-DFOLDER_NAME="GTADEMAKE"' gta`
Expected: compilation OK. **Jouable** : M3 démarre après M2 ; le débiteur erre/fuit à Chinatown ; le rattraper et le mettre KO → prime +180.

- [ ] **Step 4: Commit**

```bash
git add gta/gta.ino
git commit -m "feat(campagne): M3 Mauvaise dette (poursuite a pied)"
```

### Task 12: M4 — Le dernier trajet (réusinage d'OBJS_DEAL)

**Files:**
- Modify: `gta/gta.ino` (`OBJS_DEAL` ~ligne 427 ; titre `MISSIONS[1]` ~ligne 527 ; `STORY_SEQ[3]` ; `MISSIONS[1].isStory`)

- [ ] **Step 1: Réécrire `OBJS_DEAL` pour M4 (Garage → Chantier)**

Remplacer `OBJS_DEAL` (lignes ~427–436) par :

```cpp
// M4 (trame) : reutilise la mecanique Marco passager -> mort -> tueur qui fonce.
static const Objective OBJS_DEAL[] = {
  { OBJ_ENTER_CAR, 0, 0,  0, false, EV_NONE,       "Le Garage",
    "Marco : un dernier rendez-vous. Prends la caisse au Garage.", nullptr },
  { OBJ_GOTO,      0, 0, 14, true,  EV_MARCO_JOIN,  "Le Garage",
    "Passe prendre Marco.", "Marco : roule, au Chantier." },
  { OBJ_GOTO,      0, 0, 16, true,  EV_MARCO_DIE,   "Chantier",
    "Conduis Marco au Chantier.", "Un type surgit... Marco s'effondre !" },
  { OBJ_KILL,      0, 0,  0, false, EV_NONE,        "Chantier",
    "Tu peux pas laisser passer ca. Bute le tueur.", "Justice est faite. ...pour l'instant." },
};
```

- [ ] **Step 2: Renommer l'entrée `MISSIONS[1]` et la marquer `isStory`**

Remplacer la ligne `{ "Mauvaise affaire", OBJS_DEAL, 4, 500 },` (~ligne 527) par :

```cpp
  { "Le dernier trajet", OBJS_DEAL,    4, 250, true },   // index 1 = M4 (trame)
```

`STORY_SEQ` (final) :

```cpp
static const uint8_t STORY_SEQ[ACT1_LAST] = { 16, 17, 18, 1 };
```

- [ ] **Step 3: Compiler + tester en jeu (M4 + échec + clôture)**

Run: `arduino-cli compile --fqbn gamebuino:samd:gamebuino_meta_native --build-property 'compiler.cpp.extra_flags=-DFOLDER_NAME="GTADEMAKE"' gta`
Expected: compilation OK. **Jouable** : après M3, M4 démarre ; prendre la caisse au Garage, Marco monte, rouler au Chantier ; Marco se fait abattre, le tueur fonce ; le tuer → prime +250 + narration de clôture d'Acte I ; le téléphone rouge devient muet. Si on fuit le tueur hors carte → overlay « MISSION ECHOUEE », et revenir au rouge relance M4.

- [ ] **Step 4: Commit**

```bash
git add gta/gta.ino
git commit -m "feat(campagne): M4 Le dernier trajet + cloture Acte I"
```

### Task 13: Mesure flash + nettoyage

**Files:**
- Modify: `gta/gta.ino` (commentaires obsolètes : `#define MISSION_DEAL`, MARCO_BUSY si plus utilisé)

- [ ] **Step 1: Vérifier l'usage de `MARCO_BUSY_LINES`**

Le bloc « Cabine rouge » qui utilisait `MARCO_BUSY_LINES` a été remplacé (Task 6 step 5). Run:
`grep -n "MARCO_BUSY" gta/gta.ino`
S'il n'y a plus d'usage, supprimer `MARCO_BUSY_LINES[]` et `NUM_MARCO_BUSY` (~lignes 588–597) pour récupérer de la flash. Sinon, laisser.

- [ ] **Step 2: Compiler et noter la taille flash**

Run: `arduino-cli compile --fqbn gamebuino:samd:gamebuino_meta_native --build-property 'compiler.cpp.extra_flags=-DFOLDER_NAME="GTADEMAKE"' gta`
Expected: compilation OK ; noter le `%` flash (README indique ~47 % au départ). Si tension > ~90 %, raccourcir des `text`/`doneText`.

- [ ] **Step 3: Relancer les tests host (non-régression)**

Run: `g++ -std=c++11 -I gta tools/host_test/test_mission_host.cpp -o /tmp/mh && /tmp/mh`
Expected: `OK : ...`, code retour 0.

- [ ] **Step 4: Commit**

```bash
git add gta/gta.ino
git commit -m "chore(campagne): nettoyage post-Acte I + mesure flash"
```

---

## Notes de suivi (hors périmètre de ce plan)

- **EEPROM** : persister `campaignStep` via `gb.save`/`gb.load` (passe dédiée).
- **`EV_ALLY_*`** : renommer `EV_MARCO_*` quand l'Acte II/III introduit d'autres alliés.
- **3 Commerces distincts** : promouvoir M2 (et M9) à 3 repères séparés (offsets POI).
- **Actes II–IV** : `OBJ_ESCORT`/`OBJ_DEFEND`/`OBJ_PICKUP`/`OBJ_STEAL_CAR`, boss à PV élevés, POI déjà générés (Bar, Bureaux, Casino).
