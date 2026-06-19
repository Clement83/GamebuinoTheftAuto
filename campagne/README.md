# Campagne — scénario de la trame principale

Trame principale scénarisée du demake : **18 missions + épilogue**, pilotées par
un **téléphone rouge unique** à la Planque. Ce dossier documente la campagne
**telle qu'implémentée** : chaque fiche `m*.md` est un script fidèle au code
(`OBJS_M*` dans `gta/game_state.h`, glue et cinématiques dans `gta/mod_mission.h`,
décrochage dans `gta/gta.ino`).

Contexte moteur d'origine : [docs/superpowers/specs/2026-06-09-missions-modulaires-design.md](../docs/superpowers/specs/2026-06-09-missions-modulaires-design.md).

## Sommaire des missions

### ACTE I — Marco (M1–M4) · *tutoriel déguisé*
- [M1 — Premier jour](acte-1-marco/m1-premier-jour.md)
- [M2 — Les assurances](acte-1-marco/m2-les-assurances.md)
- [M3 — Mauvaise dette](acte-1-marco/m3-mauvaise-dette.md)
- [M4 — Le dernier trajet](acte-1-marco/m4-le-dernier-trajet.md)

### ACTE II — La vengeance manipulée (M5–M11) · *fausse piste, les Loups*
- [M5 — Un nom](acte-2-vengeance-manipulee/m5-un-nom.md)
- [M6 — Message aux Loups](acte-2-vengeance-manipulee/m6-message-aux-loups.md)
- [M7 — Voiture volée](acte-2-vengeance-manipulee/m7-voiture-volee.md)
- [M8 — Représailles](acte-2-vengeance-manipulee/m8-represailles.md)
- [M9 — La tournée de Marco](acte-2-vengeance-manipulee/m9-la-tournee-de-marco.md)
- [M10 — L'entrepôt](acte-2-vengeance-manipulee/m10-l-entrepot.md)
- [M11 — Rico le Loup](acte-2-vengeance-manipulee/m11-rico-le-loup.md)

### ACTE III — La vérité (M12–M15) · *Sarah, les preuves*
- [M12 — La mallette](acte-3-la-verite/m12-la-mallette.md)
- [M13 — Le témoin](acte-3-la-verite/m13-le-temoin.md)
- [M14 — Embuscade](acte-3-la-verite/m14-embuscade.md)
- [M15 — Les dossiers](acte-3-la-verite/m15-les-dossiers.md)

### ACTE IV — Victor (M16–M18 + épilogue)
- [M16 — Sabotage](acte-4-victor/m16-sabotage.md)
- [M17 — Bruno](acte-4-victor/m17-bruno.md)
- [M18 — Le dernier appel](acte-4-victor/m18-le-dernier-appel.md)
- [Épilogue](acte-4-victor/epilogue.md)

---

## 1. Boucle de la campagne

Deux familles de cabines coexistent :

| Cabine | Couleur | Rôle |
|---|---|---|
| `PHONES[]` (secondaires) | **bleu** `0x019F` | 15 missions **rejouables**, réparties sur la carte, indépendantes de la trame |
| Cabine de trame | **rouge** `0xC800` | **une seule**, ancrée à la **Planque**. Sonne au lancement, puis se réarme après chaque mission de trame |

`campaignStep` (sauvegardé, persiste à l'extinction) indexe `STORY_SEQ[]` :
l'ordre des missions de trame dans `MISSIONS[]`.

```
Lancement / retour à la Planque
   └─> la cabine ROUGE sonne (tant que campaignStep <= STORY_LEN)
        └─> décrocher (A à portée) -> startMission(STORY_SEQ[campaignStep])
             └─> objectifs…
                  ├─ Réussite -> prime $, bandeau MISSION ACCOMPLIE,
                  │              campaignStep++ ; la cabine re-sonne pour la suivante
                  └─ Échec   -> bandeau MISSION RATÉE ; campaignStep inchangé ;
                               revenir à la cabine relance la même mission
   └─> campaignStep == STORY_LEN -> un dernier décrochage = épilogue (FIN)
```

Pendant qu'une mission de trame est active, la cabine rouge ne propose que la
mission courante ; les cabines bleues ne se décrochent qu'**hors mission**.

---

## 2. Types d'objectifs du moteur (réels)

Une mission = un tableau d'`Objective` enchaînés (`mission.h`). Types existants :

| Type | Comportement |
|---|---|
| `OBJ_GOTO` | atteindre un point/POI dans un rayon. Option `requireCar` (« conduis jusqu'à »), option `limit` chrono (dépassé → échec) |
| `OBJ_ENTER_CAR` | monter dans la **caisse de mission** (orange), posée à l'activation ou au décrochage |
| `OBJ_KILL` | soit une **cible nommée** (fugitif `count=1` qui erre/fuit ; **boss** `count>1` à `targetHp` coups qui fonce), soit « tous les ennemis scénarisés à terre » (`enemyCount>0`, aucune cible nommée) |
| `OBJ_BEAT` | mettre KO `count` passants, **ou** abattre tous les ennemis scénarisés (`enemyCount>0`) |
| `OBJ_SUBDUE` | frapper une cible nommée `count` fois : elle **cède** sans mourir (elle se défend entre-temps) |
| `OBJ_TALK` | s'approcher (petit rayon, à pied) d'un PNJ nommé qui émerge d'un bâtiment |
| `OBJ_CRUSH` | amener la caisse de mission au broyeur de La Casse et la faire écraser |
| `OBJ_SURVIVE` | tenir `limit` frames *(non utilisé par la trame)* |

**Ennemis scénarisés** : `EK_THUG` (gros bras, fonce et cogne) ou `EK_GUNNER`
(tireur). Mode `SP_PRESENT` (visibles d'emblée) ou `SP_AMBUSH` (passifs jusqu'à
l'approche). Posés en anneau déterministe (`spawnEnemiesForObjective`), max
`MAX_ENEMIES == 4`. `MAX_OBJS == 8` objectifs par mission.

**Événements & cinématiques** (`EV_*` / `CUT_*`, joueur figé) :
- `EV_MARCO_JOIN` — un allié émerge puis monte passager. `count==1` → l'allié est
  **Sarah** (magenta) au lieu de Marco (cyan). Utilisé en **M1, M4, M13**.
- `EV_MARCO_DIE` — `CUT_MARCO_DEATH` : Marco descend, parle au tueur, se fait
  abattre ; le tueur prend la fuite (à rattraper). **M4 uniquement.**
- `EV_MARCO_LEAVE` — `CUT_MARCO_LEAVE` : Marco descend, remercie, rentre chez lui,
  clôt la mission. **M1 uniquement.**
- `CUT_TAUNT` — bref face-à-face de boss (deux répliques) avant la baston, pour
  **Rico (M11)**, **Bruno (M17)**, **Victor (M18)** (`bossTauntLines`).

**Échec** : seule **M4** peut échouer dans la trame (le tueur en fuite sort des
limites du monde). Les chronos `limit` existent pour les missions secondaires
(Taxi, Course…) mais aucune mission de trame n'en pose.

**Économie** : prime `reward` créditée à la complétion (`finishMission`), de
**120 $** (M1) à **800 $** (M18).

---

## 3. Lieux (POI) utilisés par la trame

Ancrés par nom de POI (`findPoi`), résolus en coords au lancement.

| POI | Rôle | Missions |
|---|---|---|
| **Planque** | base du joueur, **téléphone rouge** | toutes |
| **Le Garage** | Marco puis Tony | M1, M4, M8 |
| **Les Commerces** | tournée / racket | M2, M9 |
| **Le Bar** | Nico (M5), Sarah (M13), le vieux | M5, M13 |
| **Chinatown** | quartier des Loups, parking de la mallette | M3, M6, M12, M16 |
| **Les Quais** | docks, caisse des Loups, entrepôt | M7, M10 |
| **Le Chantier** | mort de Marco, repaire de Rico | M4, M11 |
| **Commissariat** | planque arrière | M7 |
| **Les Bureaux** | bureaux de Victor (dossiers) | M15 |
| **Le Casino** | repaire final de Victor | M16, M18 |
| **La Casse** | broyeur (sabotage), repaire de Bruno | M16, M17 |

---

## 4. Personnages

- **Le joueur** — sans nom, exécutant.
- **Marco** — petite frappe sympathique, mentor du joueur. Allié cyan
  (`MARCO_COLOR`). Tué en M4.
- **L'Inconnu / Tony** — voix au téléphone qui « venge » Marco (M5–M6), se révèle
  être **Tony**, patron du Garage, vrai manipulateur (M7–M11, M16–M17).
- **Les Loups** — gang rival, fausse piste. **Rico**, leur lieutenant (boss M11),
  révèle que le joueur sert le vrai coupable.
- **Sarah** — journaliste/témoin, détient les preuves contre Victor. Alliée
  magenta (`SARAH_COLOR`), escortée en M13.
- **Victor** — commanditaire de l'assassinat de Marco, boss final (M18).
- **Bruno** — homme de main de Victor, boss (M17).

---

## 5. Convention des fiches

Chaque fiche `m*.md` est un **script fidèle au code**, dans l'ordre :
**Entrée** (index `MISSIONS[]`/`STORY_SEQ[]`), **Déclencheur**, **Prime**,
**Échec**, **Setup au décrochage** si pertinent, puis pour **chaque objectif** :
type moteur · narration exacte (activation/atteint) · action joueur · script
déclenché · condition de complétion ; enfin les **cinématiques** et la
**clôture**. Pas de duplication entre fiches : le détail vit dans la fiche, ce
README ne donne que la vue d'ensemble.
