# Campagne — scénario de la trame principale

Ce fichier décrit la **trame principale scénarisée** du demake (18 missions +
épilogue), pilotée par un **téléphone rouge unique** à la planque du joueur. Il
sert de feuille de route : ce qui se branche sur le moteur existant, et les
**nouvelles mécaniques à implémenter** (clairement balisées `🆕 À IMPLÉMENTER`).

Lis d'abord, pour le contexte :
- [docs/superpowers/specs/2026-06-09-missions-modulaires-design.md](docs/superpowers/specs/2026-06-09-missions-modulaires-design.md)
  — moteur de missions modulaire (types d'objectifs, événements, narration).
- [POI.md](POI.md) — points d'intérêt, économie, services interactifs.

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

## 6. Les missions

### ACTE I — Marco (M1–M4) · *tutoriel déguisé*

#### M1 — Premier jour
- **Appel** — *Marco : « Salut. On m'a dit que tu cherchais du boulot. Viens au
  parking derrière le Garage. »*
- **Lieux** — Le Garage (parking), Les Quais (docks).
- **Objectifs**
  1. `OBJ_GOTO` → Le Garage (à pied). *« Va voir Marco au Garage. »*
  2. `OBJ_TALK` → Marco. *« Parle à Marco. »* → `EV_ALLY_JOIN` (Marco monte).
  3. `OBJ_ESCORT` `requireCar` → **Les Quais**. *« Conduis Marco aux docks. »*
     → narration de conduite (cf. amélioration).
  4. `OBJ_GOTO` `requireCar` → point de livraison aux Quais. Un PNJ attend.
     *« Le colis est livré. »*
- **Événements** — `EV_ALLY_JOIN` à l'étape 2.
- **Échec** — aucun (mission d'intro ; Marco ne meurt pas ici).
- **Prime** — 120 $.
- **Améliorations**
  - *« Ralentis, c'est pas une course »* : si la vitesse dépasse un seuil ou en
    cas de collision, Marco râle (narration), **sans échec**. Tutoriel doux de
    la conduite.
  - On fusionne les deux arrêts de ton brouillon (Garage Sud puis docks) en
    **un seul trajet** Garage → Quais pour rester lisible sur 80×64.

#### M2 — Les assurances *(le racket, version pédagogique)*
- **Appel** — *Marco : « Aujourd'hui tu vas voir comment on gagne vraiment de
  l'argent. »*
- **Lieux** — Les Commerces (3 supérettes, repères fixes).
- **Objectifs**
  1. `OBJ_GOTO` → Commerce 1. Marco encaisse seul. *« Tu vois ? Facile. »*
  2. `OBJ_GOTO` → Commerce 2. *« Hé ! Viens là ! »* → `OBJ_SUBDUE` le
     commerçant (3 coups) : *« OK ! OK ! »* Marco récupère l'argent.
  3. `OBJ_GOTO` → Commerce 3. Paiement normal. *« Tournée bouclée. »*
- **Échec** — aucun.
- **Prime** — 150 $.
- **Améliorations** — réutilise les **3 repères Commerces** qui reviennent en
  **M9** (callback émotionnel après la mort de Marco).

#### M3 — Mauvaise dette
- **Appel** — *Marco : « Un idiot nous doit de l'argent. »*
- **Lieux** — un parking (repère fixe près de Chinatown).
- **Objectifs**
  1. `OBJ_GOTO` → le parking. Le débiteur est là. *« On vient chercher notre
     dû. »* → *« Pas aujourd'hui ! »* → `EV_REVEAL` : il **s'enfuit**.
  2. `OBJ_KILL`/`OBJ_SUBDUE` → poursuivre (IA de **fuite**) puis le tabasser.
     *« Tu cours moins vite, maintenant ? »*
- **Échec** — aucun (s'il s'échappe trop loin, simple re-flânerie, pas de fail).
- **Prime** — 180 $.
- **Améliorations** — réutilise directement l'IA de fuite existante (celle de
  Joe). Introduit la **poursuite à pied** avant la poursuite en voiture de M4.

#### M4 — Le dernier trajet *(pivot tragique)*
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

---

### ACTE II — La vengeance manipulée (M5–M11) · *fausse piste, les Loups*

#### M5 — Un nom
- **Appel** — *Inconnu : « Trouve Nico. »*
- **Lieux** — Le Bar `🆕`.
- **Objectifs** — `OBJ_GOTO` → le Bar ; `OBJ_SUBDUE` Nico : *« Les Loups
  cherchent quelqu'un depuis des semaines… c'est sûrement eux. »*
- **Échec** — aucun. **Prime** — 200 $.

#### M6 — Message aux Loups
- **Lieux** — quartier des Loups (district nommé).
- **Objectifs** — `OBJ_GOTO` → le quartier ; `OBJ_KILL`×3 ou `OBJ_BEAT count=3`
  (les trois membres). `EV_CALL` final : *« Ça devrait attirer leur
  attention. »*
- **Échec** — aucun. **Prime** — 250 $. Monte fortement les **étoiles** (3
  meurtres rapprochés) → le joueur découvre Pay'n'Spray / la fuite.

#### M7 — Voiture volée
- **Appel** — *Tony : « J'ai besoin d'une caisse appartenant aux Loups. »*
  (premier dialogue signé Tony, l'« Inconnu » prend un nom.)
- **Lieux** — parking des Loups (repère) → planque derrière le **Commissariat**.
- **Objectifs** — `OBJ_BEAT count=2` (gardes) ; `OBJ_STEAL_CAR` (la caisse
  marquée) ; `OBJ_GOTO requireCar` → planque. *« Tony : parfait. »*
- **Échec** — si la caisse volée est détruite : **échec**. **Prime** — 350 $.

#### M8 — Représailles
- **Appel** — *Tony : « Ils ont retrouvé mon Garage ! »*
- **Lieux** — Le Garage.
- **Objectifs** — `OBJ_GOTO` → Le Garage ; `OBJ_DEFEND` Tony (éliminer 3
  assaillants ; **Tony ne doit pas tomber**). *« Tony : ils deviennent
  nerveux. »*
- **Échec** — si Tony meurt : **échec**. **Prime** — 350 $.
- **Améliorations** — première mission **de défense** : conseiller au joueur de
  passer par **AMU Nation** avant (gear up).

#### M9 — La tournée de Marco *(callback émotionnel)*
- **Lieux** — Les **3 Commerces** de M2.
- **Objectifs**
  1. `OBJ_GOTO` Commerce 1 — *« C'est toi maintenant ? »*
  2. `OBJ_GOTO` Commerce 2 — *« Désolé pour Marco. »*
  3. `OBJ_GOTO` Commerce 3 — des **Loups** sont là → `OBJ_KILL`/`OBJ_BEAT`.
- **Échec** — aucun. **Prime** — 300 $. Petit **hommage à Marco** en narration.

#### M10 — L'entrepôt
- **Appel** — *Inconnu/Tony : « On a trouvé leur stock. »*
- **Lieux** — un entrepôt (repère fixe, district industriel / **Les Quais**).
- **Objectifs** — `OBJ_BEAT` (gardes de la porte) ; `OBJ_GOTO` (entrer) ;
  `OBJ_KILL`/`OBJ_BEAT count=N` (tous les ennemis). *« Le patron va nous
  tuer ! »*
- **Échec** — aucun. **Prime** — 400 $.

#### M11 — Rico le Loup *(retournement)*
- **Lieux** — ancienne usine (**Chantier** ou repère industriel).
- **Objectifs** — `OBJ_GOTO` → l'usine ; `OBJ_KILL` **Rico** (boss : plus de
  PV, peut tirer). Après défaite, `OBJ_TALK`/dialogue : *« Tu crois vraiment
  qu'on a tué Marco ? Tu bosses pour le vrai coupable. »* → `EV_CALL` :
  l'inconnu **raccroche aussitôt**.
- **Échec** — aucun. **Prime** — 450 $.
- **Améliorations** — **boss fight** : introduit l'idée d'ennemis à PV élevés
  (réutilise les PV PNJ existants, multipliés). Pivot narratif de la campagne.

---

### ACTE III — La vérité (M12–M15) · *Sarah, les preuves*

#### M12 — La mallette
- **Appel** — *(nouveau numéro) Sarah : « Je peux prouver ce qu'il dit. »*
- **Lieux** — un parking (repère). Un homme mort au sol, une **mallette** à
  côté.
- **Objectifs** — `OBJ_GOTO` → le parking ; `OBJ_PICKUP` la mallette ;
  `OBJ_GOTO` la rapporter (à Sarah / Planque).
- **Échec** — aucun. **Prime** — 350 $.

#### M13 — Le témoin
- **Lieux** — point de rendez-vous → planque de Sarah.
- **Objectifs** — `OBJ_TALK` Sarah (*« Victor a peur… il efface les
  preuves. »*) → `EV_ALLY_JOIN` ; `OBJ_ESCORT requireCar` → la planque.
  En route : `EV_AMBUSH` (2 voitures ennemies) ; `OBJ_GOTO requireCar`
  (continuer jusqu'à la planque malgré l'embuscade).
- **Échec** — si Sarah meurt : **échec**. **Prime** — 400 $.

#### M14 — Embuscade
- **Appel** — *Sarah : « Ils nous ont trouvés. »*
- **Lieux** — la planque de Sarah.
- **Objectifs** — `OBJ_GOTO` → la planque ; `OBJ_DEFEND` (éliminer tous les
  attaquants, Sarah survit). *« Je sais où sont les dossiers. »*
- **Échec** — si Sarah meurt : **échec**. **Prime** — 450 $.

#### M15 — Les dossiers
- **Lieux** — **Les Bureaux** `🆕` de Victor.
- **Objectifs** — `OBJ_GOTO` (entrer) ; `OBJ_PICKUP` les documents ;
  `OBJ_GOTO` (sortir) → à la sortie `EV_AMBUSH`/`OBJ_BEAT count=3` (gardes).
  Sarah lit : *« Victor a ordonné l'assassinat. »*
- **Échec** — aucun (le combat de sortie est obligatoire mais sans fail).
- **Prime** — 500 $.

---

### ACTE IV — Victor (M16–M18 + épilogue)

#### M16 — Sabotage *(intègre le broyeur de La Casse)*
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

#### M17 — Bruno
- **Lieux** — casse automobile (**La Casse**) — Bruno surveille.
- **Objectifs** — `OBJ_GOTO` → La Casse ; `OBJ_KILL` **Bruno** (boss). Après
  défaite : *« Le vieux casino… »* → `EV_CALL` coupé.
- **Échec** — aucun. **Prime** — 600 $.

#### M18 — Le dernier appel *(boss final)*
- **Appel** — *Victor : « Je suis au casino. »*
- **Lieux** — **Le Casino** `🆕`.
- **Objectifs** — `OBJ_GOTO` → Casino ; `OBJ_BEAT count=N` (gardes) ;
  `OBJ_GOTO` (atteindre Victor) ; dialogue (*« Marco allait me dénoncer. Je ne
  pouvais pas le laisser parler. »*) ; `OBJ_KILL` **Victor** (boss final, le
  plus coriace). Après : *« Tout ça pour un ami ? »*
- **Échec** — aucun. **Prime** — 800 $.

#### Épilogue
- À la Planque, le **téléphone rouge sonne une dernière fois**. On décroche.
  **Silence.** Puis la ligne coupe. **FIN.**
- Mécanique : `campaignStep` atteint le terminal → le tél rouge déclenche une
  **séquence de fin** (pas d'objectif), puis se tait définitivement (ou
  propose un mode « ville ouverte » : seules les cabines bleues sonnent encore).

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
