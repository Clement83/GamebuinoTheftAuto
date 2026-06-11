# Campagne — scénario de la trame principale

Ce dossier décrit la **trame principale scénarisée** du demake (18 missions +
épilogue), pilotée par un **téléphone rouge unique** à la planque du joueur. Il
sert de feuille de route : ce qui se branche sur le moteur existant, et les
**nouvelles mécaniques à implémenter** (clairement balisées `🆕 À IMPLÉMENTER`).

Lis d'abord, pour le contexte :
- [docs/superpowers/specs/2026-06-09-missions-modulaires-design.md](../docs/superpowers/specs/2026-06-09-missions-modulaires-design.md)
  — moteur de missions modulaire (types d'objectifs, événements, narration).
- [POI.md](../POI.md) — points d'intérêt, économie, services interactifs.

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

## 1. Téléphones : ce qui change

Deux familles de cabines coexistent déjà dans `gta.ino` :

| Cabine | Couleur | Rôle | Changement demandé |
|---|---|---|---|
| `PHONES[]` (missions secondaires) | **bleu** `0x019F` | 15 missions **rejouables**, réparties sur la carte, indépendantes | **Inchangé**, sauf : on **supprime « Mauvaise affaire » (Marco, slot 1)** et on la **remplace par une livraison de pizza** (cf. §9). Marco quitte les missions secondaires : il devient le pivot de la trame principale. |
| `STORY_PHONES[]` (trame) | **rouge** `0xC800` | 4 cabines d'ancrage qui **ne sonnent pas** encore | **Refonte** : on n'en garde **qu'une seule**, ancrée à un **nouveau POI « Planque »** (la base du joueur). Elle **sonne au lancement** (Mission 1), puis **se réarme après chaque mission** pour appeler la suivante. Les 3 autres anchors rouges sont retirés. |

### Boucle de la campagne

```
Lancement du jeu
   └─> Planque : le téléphone rouge SONNE
        └─> on décroche -> dialogue -> Mission N démarre
             └─> objectifs… (réussite | échec)
                  ├─ Réussite -> prime $, narration de clôture,
                  │              campaignStep++ ; retour à la Planque,
                  │              le tél rouge re-sonne pour Mission N+1
                  └─ Échec   -> overlay « MISSION ÉCHOUÉE » plein écran ;
                               campaignStep inchangé ; revenir au tél de la
                               Planque relance la même mission
```

Pendant qu'une mission de trame est active, les cabines **bleues** restent
décrochables comme aujourd'hui (le joueur peut faire des missions secondaires
entre deux missions d'histoire), mais le **téléphone rouge ne propose que la
mission de trame courante**.

---

## 2. Modèle moteur

### 2.1 Ce qui existe déjà (réutilisé tel quel)

- **Types d'objectifs** : `OBJ_GOTO` (atteindre un point/POI, option `requireCar`
  + `limit` chrono), `OBJ_ENTER_CAR` (être au volant de la caisse de mission),
  `OBJ_KILL` (cible morte ; peut être un **tueur qui fonce** via
  `missionChaseStep`), `OBJ_BEAT` (mettre KO `count` PNJ), `OBJ_SURVIVE` (tenir
  `limit` frames).
- **Événements** : `EV_MARCO_JOIN` (un PNJ monte passager), `EV_MARCO_DIE` (le
  passager meurt, un tueur apparaît).
- **Ancrage POI** : un objectif référence un POI par nom (`findPoi`), résolu au
  lancement ; `x,y` sert de repli.
- **IA** : flânerie + fuite en ligne de vue, poursuite (`missionChase*`).
- **Économie** : prime `reward` créditée à la complétion ; HUD $, butin.
- **Services** : Hôpital (soin), AMU Nation (armes), Pay'n'Spray (effacer les
  étoiles), La Casse (broyeur → $). La campagne s'appuie dessus (se soigner,
  s'armer, blanchir entre deux missions).
- **Recherche police** (`wanted.h`) : les crimes montent les étoiles.

### 2.2 Nouvelles mécaniques `🆕 À IMPLÉMENTER`

Réparties par ordre de priorité (un sous-ensemble suffit pour l'Acte 1) :

| Mécanique | Description | Utilisée par |
|---|---|---|
| **Progression persistante** | `campaignStep` (uint8) = index de mission de trame atteint, sauvegardé (`gb.save`/EEPROM) pour survivre à l'extinction. `MissionDef` reçoit un flag `isStory`. | toute la campagne |
| **État d'échec + overlay** | Si une condition d'échec est remplie (allié mort, chrono écoulé, objet perdu), gros texte **« MISSION ÉCHOUÉE »** centré ~2 s, mission désarmée ; on relance en revenant au tél rouge. Champ `failLimit`/`failOn` sur l'objectif. | escortes, chronos, défenses |
| **`OBJ_TALK`** | S'approcher d'un PNJ nommé (immobile) et presser **A** → dialogue, objectif validé. (Version simple : `OBJ_GOTO` à petit rayon + auto-message.) | M1, M4, M11, M12, M13, M18 |
| **`OBJ_SUBDUE`** | Frapper une **cible nommée** jusqu'à `count` coups : elle ne **meurt pas**, elle **cède** (dialogue), objectif validé. Variante non-létale d'`OBJ_BEAT` ciblée. | M2 (commerçant), M3 (débiteur), M5 (Nico) |
| **`OBJ_PICKUP`** | Ramasser un **objet scénarisé** au sol (mallette, dossiers) — loot marqué, ramassé à pied. | M12, M15 |
| **`OBJ_ESCORT`** | Un **allié** monte (passager) ou suit ; l'amener à destination. L'allié a des PV ; **s'il meurt → échec**. Généralise `EV_MARCO_JOIN`. | M1, M4, M13 |
| **`OBJ_DEFEND`** | Protéger un **allié/lieu** : éliminer toutes les vagues d'ennemis. Si l'allié tombe → échec. | M8, M14 |
| **`OBJ_STEAL_CAR`** | Voler un **véhicule précis** (marqué, gardé) puis le ramener — `OBJ_ENTER_CAR` ciblé sur une caisse spécifique + `OBJ_GOTO requireCar`. | M7, M16 |

Généralisation d'événements (renommage propre, rétro-compatible) :
`EV_ALLY_JOIN`/`EV_ALLY_LEAVE`/`EV_ALLY_DIE` (ex-`EV_MARCO_*`), plus
`EV_AMBUSH` (spawn de voitures/ennemis hostiles), `EV_REVEAL` (apparition d'un
PNJ scénarisé : tueur, boss), `EV_CALL` (fin de mission → le tél rouge re-sonne,
ou « appel d'un inconnu »).

> **Note YAGNI** : on n'implémente pas tout d'un coup. L'Acte 1 (M1–M4) ne
> demande que `OBJ_TALK`, `OBJ_SUBDUE`, `OBJ_ESCORT` + progression + échec. Le
> reste arrive par actes (cf. §7).

---

## 3. Cartographie des lieux

### POI existants réutilisés

| Lieu du scénario | POI existant |
|---|---|
| Les docks | **Les Quais** |
| La casse / broyeur | **La Casse** |
| Le chantier (mort de Marco) | **Chantier** |
| Planque derrière le Commissariat (M7) | **Commissariat** (abords) |
| Se soigner / mourir | **Hôpital** |
| Quartier des Loups | un **district nommé** existant (ex. *St. Mark's*) + repères fixes |

### Nouveaux POI proposés `🆕`

Générés par `tools/pois.py` (comme les quartiers thématiques), bandeau HUD,
ancrage de missions. Pas forcément de nouvelles tuiles (dessin procédural
possible, comme Pay'n'Spray/AMU) :

| Nouveau POI | Rôle | Missions |
|---|---|---|
| **Planque** | base du joueur, **téléphone rouge** de trame | toutes |
| **Le Garage** | garage de Marco puis de Tony (QG criminel) | M1, M7, M8 |
| **Le Bar** | Nico traîne devant | M5 |
| **Les Bureaux** | bureaux de Victor (dossiers) | M15 |
| **Le Casino** | repaire final de Victor | M18 |
| **Les Commerces** | rangée de 3 supérettes (repères fixes) pour le racket | M2, M9 |

L'« ancienne usine » de Rico (M11) réutilise le **Chantier** ou un repère fixe
dans un district industriel — pas de nouveau POI dédié nécessaire.

---

## 4. Personnages

- **Le joueur** — sans nom, exécutant.
- **Marco** — petite frappe sympathique, mentor du joueur (Actes 1). Tué en M4.
  Couleur cyan déjà définie (`MARCO_COLOR 0x07FF`).
- **L'Inconnu** — voix au téléphone qui « venge » Marco et téléguide le joueur
  (M5–M11). Se révèle être **Tony**.
- **Tony** — vrai commanditaire, patron du Garage. Manipule le joueur contre
  les Loups (M7–M11, M16–M17).
- **Les Loups** — gang rival, fausse piste. Rico en est le lieutenant.
- **Rico** — Loup, révèle que le joueur sert le vrai coupable (M11).
- **Sarah** — journaliste/témoin, détient les preuves contre Victor (M12–M15).
- **Victor** — antagoniste final, commanditaire de l'assassinat de Marco (M18).
- **Bruno** — homme de main de Victor (M17).

---

## 5. Conventions des fiches de mission

Chaque mission précise : **Appel** (dialogue tél), **Lieu(x)**, **Objectifs**
(séquence de types moteur), **Événements**, **Échec**, **Prime**, et
**Améliorations** (ajouts/précisions par rapport à ton brouillon).

Primes indicatives : croissantes le long de l'arc (120 → 800 $), réutilisant le
barème des missions secondaires.

---

## 7. Phasage d'implémentation recommandé

Chaque phase est jouable de bout en bout et testable en host.

1. **Socle campagne** — POI **Planque** + tél rouge unique séquentiel,
   `campaignStep` persistant, **état d'échec + overlay**, `EV_CALL`
   (re-sonnerie). Porter l'Acte I sur ce socle.
2. **Acte I (M1–M4)** — `OBJ_TALK`, `OBJ_SUBDUE`, `OBJ_ESCORT` + nouveaux POI
   Garage et repères Commerces. Recycle la mécanique Marco existante en M4.
3. **Acte II (M5–M11)** — `OBJ_STEAL_CAR`, `OBJ_DEFEND`, boss (PV élevés),
   POI Bar. Branche le racket-callback (M9) sur les repères Commerces.
4. **Acte III (M12–M15)** — `OBJ_PICKUP`, `EV_AMBUSH`, POI Bureaux.
5. **Acte IV (M16–M18 + épilogue)** — réutilise le broyeur de La Casse (M16),
   boss final, POI Casino, séquence de fin.

> Pour itérer vite : implémenter d'abord **Phase 1 + M1** seules, valider la
> boucle (sonnerie → mission → réussite/échec → re-sonnerie + persistance),
> puis dérouler les actes.

---

## 8. Vigilance flash / RAM

La flash est à **~47 %** (README). 18 missions + épilogue, c'est surtout des
**chaînes de narration** (en flash). Mesures :

- Garder les textes **courts** (le bandeau défile déjà). Réutiliser des
  formulations communes.
- Les `Objective`/`MissionDef` de trame sont des tableaux **`const` en flash**
  (comme les missions secondaires actuelles), coût RAM négligeable.
- **RAM ajoutée** marginale : `campaignStep` (1 o, sauvegardé), flags d'allié,
  PV d'allié/boss, état d'échec.
- **Surveiller la taille à chaque acte** ; si la flash sature, fusionner des
  beats ou raccourcir les dialogues. Le phasage permet de s'arrêter à un acte
  propre.
- **Parité .h/.py & host tests** : toute logique pure ajoutée
  (`OBJ_SUBDUE`, échec, `campaignStep`, IA d'allié) doit être **testée en host**
  et restée en parité `mission.h` ↔ `ai.h`/`tools/*.py`.

---

## 9. Remplacement côté téléphones bleus : « Livraison de pizza »

On retire `MISSION_DEAL` (Marco) de `PHONES[]` et on branche à sa place une
mission secondaire **rejouable** légère, sans lien avec la trame :

- **Titre** — « Livraison de pizza ».
- **Objectifs** (types existants uniquement, **0 nouveau code**) :
  1. `OBJ_ENTER_CAR` → scooter/caisse de livraison (ex. à La Casse).
     *« Pizza Express : prends la caisse. »*
  2. `OBJ_GOTO requireCar` `limit≈1100` → client (ex. **Chinatown**).
     *« Livre la pizza avant qu'elle refroidisse ! »* — *« Pizza livrée,
     pourboire empoché ! »*
- **Échec/chrono** — réutilise le `limit` chrono déjà géré (comme Taxi/Course).
- **Prime** — ~200 $.
- **Note** — très proche de `OBJS_DELIVERY`/`OBJS_TAXI` existants : c'est
  volontaire (mission secondaire jetable, pas un morceau d'histoire).

---

*Document de conception. L'implémentation suivra via un plan dédié
(writing-plans), acte par acte, après validation de ce scénario.*
