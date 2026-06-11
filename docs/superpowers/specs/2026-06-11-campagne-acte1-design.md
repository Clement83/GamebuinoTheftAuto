# Campagne — Acte I (M1–M4) : design d'implémentation

Date : 2026-06-11
Cible : GTA demake Gamebuino META (sketch `gta/`, écran 80×64, ~25 fps).

Ce document est la **couche « comment on construit »** par-dessus le scénario
[campagne/](../../../campagne/README.md). Il couvre le **socle campagne** et les
**4 missions de l'Acte I** (Marco), choisis comme premier jalon jouable.

## Décisions de cadrage (brainstorming 2026-06-11)

- **Périmètre** : tout l'Acte I (M1→M4) + socle campagne, d'un seul jalon.
- **Persistance** : `campaignStep` **en RAM** pour ce jalon. L'EEPROM
  (`gb.save`/`gb.load`) est repoussé à une passe dédiée ultérieure.
- **`OBJ_TALK`** : version simple = `OBJ_GOTO` à petit rayon sur un PNJ nommé
  immobile + message auto (pas d'appui touche dédié).
- **Une seule mission active à la fois** (ajustement vs campagne/README.md §1) :
  pendant **n'importe quelle** mission (cabine bleue OU rouge), **toutes** les
  autres cabines restent **visibles mais muettes et non-décrochables**. On ne
  peut donc jamais lancer deux missions en parallèle → une seule flèche
  d'objectif et un seul `missionRun` actif à gérer.
- **Hors périmètre pour ce jalon** : `OBJ_ESCORT`/`OBJ_DEFEND`/`OBJ_PICKUP`/
  `OBJ_STEAL_CAR` (Actes II+), EEPROM, renommage `EV_MARCO_*`→`EV_ALLY_*`.

## État de départ (déjà en place)

- Moteur modulaire `mission.h` : `OBJ_GOTO`/`ENTER_CAR`/`KILL`/`BEAT`/`SURVIVE`,
  IA fuite (`missionFleeStep`) et poursuite (`missionChaseStep`), ligne de vue,
  flèche HUD (`missionArrowVec`).
- 15 missions secondaires (cabines bleues, `PHONES[]`/`MISSIONS[]`). Le swap §9
  est **déjà fait** : « Livraison de pizza » remplace l'ancienne cabine Marco ;
  `OBJS_DEAL`/`MISSION_DEAL` (mécanique Marco passager → mort → tueur) restent
  définis et seront **réutilisés pour M4**.
- Un téléphone rouge `STORY_PHONES[0]` ancré près du POI **Planque**, qui **ne
  sonne pas encore**.
- Narration : file FIFO de 4 messages (`narrate()`), bandeau défilant.
- `failMission()` existe mais ne fait qu'un message (pas d'overlay).

## 1. Socle campagne

### Données

- **`campaignStep`** (uint8, RAM) : index de la prochaine mission de trame.
  `0..3` = M1..M4 à jouer ; `4` = Acte I terminé (le rouge se tait).
- **`MissionDef.isStory`** : flag ajouté **en fin de struct** (init agrégat →
  rétro-compatible, comme `reward`). Vrai pour les missions de trame.
- **`STORY_MISSIONS[]`** : 4 `MissionDef` (M1–M4), table distincte de
  `MISSIONS[]`. `startStoryMission(step)` démarre `STORY_MISSIONS[step]`.

### Téléphone rouge séquentiel

- Le rouge **sonne** ssi `campaignStep < 4` ET aucune mission active.
- Décrocher (touche A à portée, comme les bleus) → `startStoryMission(campaignStep)`.
- À la complétion d'une mission de trame : prime créditée (déjà géré) **+**
  `campaignStep++`. Le rouge se réarme et resonne pour la suivante au retour
  près de la Planque.
- À `campaignStep == 4` : narration de clôture d'Acte I ; le rouge se tait.

### Verrou « une seule mission »

- Une seule variable `missionRun.active` (existante) gouverne tout.
- Tant que `missionRun.active`, **aucune** cabine (bleue ou rouge) ne sonne ni
  ne se décroche ; elles restent dessinées, muettes. (Le code de sonnerie/
  décrochage teste déjà `missionRun.active` côté bleues ; on étend la même
  condition au rouge et on désactive le décrochage des bleues pendant une
  mission de trame.)

## 2. Écran d'échec

- Nouvel état `missionFailedTimer` (uint16, frames). `failMission(msg)` le met à
  ~50 frames (~2 s).
- Tant que `> 0` : overlay plein écran **« MISSION ECHOUEE »** (texte centré,
  fond assombri), gameplay gelé, puis retour au repos.
- L'échec **n'incrémente pas** `campaignStep` → revenir au rouge relance la même
  mission de trame.
- Seul **M4** arme un échec (le tueur s'enfuit hors carte : nouvelle condition
  d'échec `failOnTargetEscaped`, testée quand la cible-poursuite sort des
  limites monde). Les missions secondaires gardent leur échec chrono actuel.

## 3. Nouveaux types d'objectifs (purs, `mission.h` + tests host)

Ajoutés **en fin** de l'enum `ObjType` (compat init agrégat) :

- **`OBJ_TALK`** : identique à `OBJ_GOTO` petit rayon (à pied), valide au
  contact, joue `text`/`doneText`. La glue spawne le PNJ nommé immobile à `x,y`.
  Dans le moteur pur : même test que `OBJ_GOTO` (rayon, sans `requireCar`).
- **`OBJ_SUBDUE`** : frapper une **cible nommée** `count` fois ; elle ne meurt
  pas, elle cède. Réutilise un compteur de coups (comme `OBJ_BEAT`) mais ciblé
  sur l'entité de mission spawnée. Champ réutilisé : `count`. Nouveau champ
  d'état `MissionState.subdueCount`. Validé quand `subdueCount >= count`.

### Escorte (pas de nouveau type en Acte I)

Marco est **invulnérable** en Acte I (sa seule mort est **scriptée** en M4 via
`EV_MARCO_DIE`). « Escorter » se code donc avec l'existant : `OBJ_GOTO`
`requireCar` + Marco passager (`marcoAboard`, `EV_MARCO_JOIN`). On conserve
`EV_MARCO_*` ; le renommage `EV_ALLY_*` est repoussé aux actes avec alliés
multiples.

## 4. Contenu des missions (Acte I)

Conforme à campagne/ §6. Textes **courts** (contrainte flash).

- **M1 — Premier jour** (prime 120) : `OBJ_GOTO` Le Garage (à pied) →
  `OBJ_TALK` Marco (`EV_MARCO_JOIN`) → `OBJ_GOTO requireCar` Les Quais →
  `OBJ_GOTO requireCar` point de livraison. Aucun échec.
- **M2 — Les assurances** (prime 150) : `OBJ_GOTO` Commerce 1 → `OBJ_GOTO`
  Commerce 2 puis `OBJ_SUBDUE` le commerçant (3 coups) → `OBJ_GOTO` Commerce 3.
  Aucun échec. Les 3 repères Commerces seront réutilisés en M9 (Acte II).
- **M3 — Mauvaise dette** (prime 180) : `OBJ_GOTO` parking (le débiteur est là,
  il s'enfuit — IA de fuite existante) → `OBJ_SUBDUE`/`OBJ_KILL` après
  poursuite à pied. Aucun échec.
- **M4 — Le dernier trajet** (prime 250) : reprend la mécanique `OBJS_DEAL` :
  `OBJ_ENTER_CAR` au Garage → `OBJ_GOTO requireCar` prendre Marco
  (`EV_MARCO_JOIN`) → `OBJ_GOTO requireCar` **Chantier** (`EV_MARCO_DIE` :
  tueur apparaît, Marco meurt) → `OBJ_KILL` le tueur (poursuite). **Échec** si
  le tueur s'enfuit hors carte. Fin → narration de clôture d'Acte I.

## 5. POI / repères

- **Le Garage** : nouveau POI (procédural, sans nouvelle tuile, façon
  Pay'n'Spray) ou repère fixe sur tuile valide. Utilisé par M1 et M4.
- **3 Commerces** : repères fixes sur tuiles valides (trottoir/route), réutilisés
  en M9. Documenter les coords dans `gta.ino`.
- **Chantier**, **Les Quais**, **Planque** : POI existants, réutilisés via
  `findPoi`.

## 6. Tests host (parité obligatoire)

Étendre `tools/host_test/test_mission_host.cpp` :

- `missionObjectiveDone` pour `OBJ_TALK` (rayon, à pied) et `OBJ_SUBDUE`
  (seuil `subdueCount >= count`).
- Enchaînement `missionAdvance` sur une séquence type M1 (4 objectifs) et M4
  (événements `EV_MARCO_JOIN`/`EV_MARCO_DIE` aux bonnes transitions).
- Condition d'échec « cible hors carte » (logique pure si extraite ; sinon
  testée au niveau de la fonction de bornage).
- Conservation des tests existants (vue, fuite, poursuite, flèche HUD).
- **Parité `mission.h` ↔ `tools/*.py`** maintenue si la logique pure est
  dupliquée côté Python.

## 7. Vigilance flash / RAM

- Tables `STORY_MISSIONS[]`/`Objective[]` en flash `const` : coût RAM nul.
- RAM ajoutée marginale : `campaignStep` (1 o), `missionFailedTimer`,
  `subdueCount`. Textes courts, formulations réutilisées.
- **Mesurer la taille flash à la compilation** après l'Acte I ; si tension,
  raccourcir les dialogues.

## 8. Ordre d'implémentation

1. Socle pur : `isStory`, `OBJ_TALK`, `OBJ_SUBDUE`, `subdueCount` dans
   `mission.h` + **tests host d'abord** (TDD).
2. Glue socle : `campaignStep`, `STORY_MISSIONS[]`, rouge séquentiel, verrou
   mono-mission, overlay d'échec.
3. Contenu M1, validé en jeu (boucle sonnerie → mission → re-sonnerie).
4. M2, M3, M4 ; échec M4 ; clôture d'Acte I.
5. POI/repères Garage + Commerces ; mesure flash.
