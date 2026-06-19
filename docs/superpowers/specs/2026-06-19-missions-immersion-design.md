# Design — Immersion des missions (passe d'enrichissement)

> Objectif : étoffer les missions de la trame avec **plus de script et de
> logique « réaliste »**, pour que jouer une mission ne soit plus « aller à un
> point A et revenir », mais vivre des **scènes**. Document de design (aucun
> code à ce stade) ; on traite les missions **une par une, dans l'ordre**.
>
> Référence de l'état actuel (fidèle au code) : `campagne/` et ses fiches
> `m*.md`. Ce document décrit les **améliorations** à apporter par-dessus.

## État d'implémentation (live)

Livraison incrémentale, chaque pas vérifié (tests host + build firmware + `.bin`).

- ✅ **Compagnon physique — descend/monte avec toi** (`gta.ino` sortie MENU). Base
  des missions à allié (M1/M4/M13).
- ✅ **Échec sélectif — véhicule de mission détruit** (`failOnCarLoss` +
  `missionCarLossFail`, testé host ; câblé `missionProgress`). Actif sur **M7**.
- ✅ **Échec sélectif — PNJ requis tué** (`failOnAllyDeath` + `missionAllyDeathFail`,
  testé host). Allié défendu stationnaire (réutilise l'entité allié) ; les ennemis
  les plus proches de lui le ciblent, le joueur s'interpose ; sa mort = échec.
  Actif sur **M8 (Tony, vert)** et **M14 (Sarah, magenta)**. *(M13 escorte : l'allié
  est aboard en voiture, donc non blessable tant qu'il ne descend pas — couvert par
  l'ambush en route, à venir avec les scènes scriptées.)*
- ⏳ **Règle racket — tuer un client coopératif = échec** (M2, M9).
- 🔨 **Scènes scriptées à destination** — système posé : `EV_DELIVERY` +
  entité contact (`sceneNpc*`) + cinématique `CUT_DELIVERY` (le compagnon descend,
  marche vers le contact, dialogue, remonte) + échec si on écrase le contact.
  **Livré sur M1 (docks)**. Réutilisable pour M7 (receleur), M2/M9 (clients), M4
  (deal subverti). Reste à dérouler sur ces missions.
- ⏳ **Death-beats de boss** (Rico/Bruno/Victor), subversion M4, M16 (caisses de
  luxe marquées + nuance broyage), épilogue habité.

## Boîte à outils transverse

Mécaniques réutilisées dans (presque) toutes les missions. Elles définissent le
vocabulaire commun ; chaque fiche y pioche.

1. **Compagnon physique persistant** — un PNJ allié (Marco, Sarah…) est une vraie
   entité dans le monde : il te suit à pied, **monte quand tu montes** en
   voiture, et **descend quand tu descends**. Il existe tant que la mission le
   porte (fini le PNJ « narratif » qui disparaît une fois « parlé »).
2. **Structure aller-chercher → emmener → redéposer** — une mission « avec PNJ »
   commence par **aller le chercher** (chez lui, au Garage, au Bar…) et finit par
   **le redéposer** à un lieu qui lui est propre. Pas de PNJ qui se téléporte.
3. **Échec sur mort d'un élément requis — sélectif** — si un **PNJ requis** meurt
   ou si un **véhicule spécifique** de mission est détruit, → **MISSION RATÉE**.
   Appliqué **seulement quand le scénario le justifie** (pas systématiquement).
4. **Véhicule spécifique identifié** — quand une mission impose une caisse précise
   (caisse de Marco, voiture de luxe de Victor…), elle est marquée/protégée : la
   prendre, la garder intacte, la livrer.
5. **Scène scriptée à destination** — à un point clé, un PNJ attend ; le
   compagnon (ou le joueur) joue un **beat autonome** (s'approche, dialogue,
   échange/remise, repart) pendant que le joueur regarde, avant que la mission
   reprenne. Réutilise la structure des cinématiques existantes (`CUT_MARCO_*`),
   mais **non létale**. C'est l'outil clé de l'immersion.

### Règles dérivées (transverses)

- **Règle racket** — sur toute mission de racket/recouvrement, **tuer un client
  coopératif** (celui qui paie) = **MISSION RATÉE**, avec le mentor qui peste.
  Apprend la retenue. La cible récalcitrante reste en `SUBDUE` (ne meurt pas).
- **Règle compagnon** — toute mission avec compagnon applique l'outil 2
  (aller le chercher / le redéposer).
- **Marco invulnérable jusqu'à M4** — Marco ne peut mourir qu'au pivot tragique
  (M4). Avant, les ennemis ne ciblent que le joueur.

---

## M1 — Premier jour ✅ *(verrouillée)*

État actuel : `GOTO Garage (à pied)` → `TALK Marco` (il suit à pied) →
`GOTO Quais (en voiture)` *« colis livré »* → `GOTO Garage (en voiture)` →
cinématique d'adieu. Prime 120 $, aucun échec. M1 incarne déjà le motif
aller-chercher → emmener → redéposer ; on l'enrichit côté script et physique.

**Améliorations retenues :**

- **Compagnon physique (outil 1)** — dès qu'on a parlé à Marco : s'il descend de
  la caisse, **Marco descend aussi** et attend à côté ; il **remonte** quand on
  remonte. Si on s'éloigne trop à pied, il râle (*« Oh petit, on n'a pas toute la
  nuit »*) — **sans échec** (intro).
- **Scène scriptée aux Quais (outil 5)** — un **contact** attend sur les docks.
  La caisse s'arrête, **Marco descend seul**, marche jusqu'au contact, **court
  échange** (2 répliques : remise du colis, remerciement/mise en garde), puis
  **revient et remonte**. La main est rendue ensuite.
- **Échec sélectif (outil 3)** — si on **écrase/tue le contact des docks** →
  **MISSION RATÉE**, Marco peste (*« Mais t'es malade ?! C'était notre client,
  abruti ! »*).
- **Fin logique sur la caisse (outil 4)** — au retour au Garage, Marco descend,
  te **laisse les clés** de la caisse (*« garde-la, petit »*) et rentre chez lui
  → fin, prime 120 $. Justifie que le joueur démarre la campagne avec une voiture.

---

## M2 — Les assurances ✅ *(verrouillée)*

État actuel (tout à pied) : `GOTO Commerces` → `SUBDUE commerçant` (3 coups) →
`GOTO Chinatown` → `GOTO Le Bar` → `GOTO Chantier` → `BEAT embuscade` (3 gros
bras). Prime 150 $. Marco aujourd'hui **narratif seulement**.

**Améliorations retenues :**

- **Marco compagnon physique (outils 1 + 2)** — on **va chercher Marco** au début
  (au Garage), il **t'accompagne à pied** sur toute la tournée (lui le collecteur,
  toi le muscle — *« regarde et apprends »*), et on **le redépose** à la fin.
- **Chaque arrêt = scène scriptée (outil 5)** :
  - **Commerce coopératif** — Marco s'approche, le commerçant paie sans broncher,
    Marco empoche (*« Tu vois ? Facile. »*).
  - **Commerçant récalcitrant (`SUBDUE`)** — il refuse, **bouscule Marco**, Marco
    recule (*« Vas-y petit, montre-lui »*) → on le mate → il crache l'argent.
  - **Le Bar** — le vieux paie rubis sur l'ongle et **offre un verre** (petit
    soin / flavor à la sortie).
  - **Le Chantier** — Marco se fige, méfiant (*« …je le sens pas »*) →
    l'embuscade jaillit.
- **Échec racket (règle dérivée)** — **tuer un commerçant coopératif** → MISSION
  RATÉE, Marco explose (*« T'es taré ?! On rackette, on massacre pas ! »*). Le
  récalcitrant est en `SUBDUE` (ne meurt pas).
- **Marco invulnérable** dans l'embuscade finale (ne meurt qu'en M4) : les gros
  bras ne ciblent que le joueur.

---

## M3 — Mauvaise dette ✅ *(verrouillée)*

État actuel : `GOTO Chinatown` → `BEAT 2 gros bras` *(SP_PRESENT)* →
`KILL le payeur` *(count 1, il fuit)*. Prime 180 $. Marco narratif.

**Améliorations retenues :**

- **Marco compagnon (règle compagnon)** — aller le chercher / le redéposer. Il
  reste **en retrait** (le bras, c'est le joueur) mais commente.
- **Scène de confrontation à Chinatown (outil 5)** — le payeur est là, **entouré
  de ses gros bras**. Marco s'avance (*« Tu sais pourquoi on est là. Paie. »*),
  le payeur crâne et **fait signe à ses hommes** (*« Bossez, les gars ! »*) → la
  bagarre démarre.
- **Finition brutale (`KILL` conservé)** — ses hommes au sol, le payeur
  **détale** ; on le rattrape. Beat de supplication possible (*« Pitié ! Je
  paie ! »*) **juste avant la mise à mort** — Marco voulait « un exemple ». Pas de
  fric, juste un cadavre. **Aucun échec** sur cette mission.

---

## M4 — Le dernier trajet ✅ *(verrouillée)*

État actuel : `ENTER_CAR Garage` → `GOTO Garage` (Marco monte) → `GOTO Chantier`
(Marco passager) → **cinématique de mort** → `KILL le tueur` (fuite, fail s'il
s'échappe). Prime 250 $. Le pivot tragique — déjà la mission la plus scriptée.

**Améliorations retenues :**

- **Ambiance « dernier trajet » (outil 5, dialogue)** — Marco passager est
  **nerveux** : répliques égrenées sur le trajet (*« Après ce soir, je me tire de
  cette ville, petit. »* / *« T'es un bon gars. Fais pas mes conneries. »* /
  *« …t'as vu cette caisse derrière nous ? Non ? Laisse tomber. »*). Monte le
  malaise.
- **Subvertir la scène de M1 (le coup de génie, outil 5)** — au Chantier, ça
  ressemble à **un deal normal** : un « acheteur » attend, exactement comme le
  contact des docks en M1. Marco **descend gérer l'échange** — le même beat
  rassurant — sauf que l'acheteur le **trahit et l'abat**. Le rituel de M1
  retourné contre le joueur : c'est ce qui donne le punch.
- **La traque durcie** — le tueur prend la fuite **à pied**, atteint une caisse
  et **file en voiture** : il faut le **rattraper en caisse**. **Fail** s'il sort
  de la map (durcissement du fail existant). Beat final quand on le coince
  (*« L'autre : c'était juste un contrat, mec ! »* → exécution).
- **Mort de Marco scriptée** (l'histoire, pas un fail). Marco cesse d'être
  invulnérable **ici seulement**.

---

# ACTE II — La vengeance manipulée (M5–M11)

> Note de ton : **le joueur est désormais SEUL**, sans compagnon, guidé par la
> seule voix au téléphone (« l'Inconnu » / Tony). La solitude est un parti pris :
> l'outil « compagnon physique » ne s'applique donc quasiment pas à l'Acte II.

## M5 — Un nom ✅ *(verrouillée)*

État actuel : `GOTO Bar` → `SUBDUE Nico` (3 coups). Prime 200 $.

**Améliorations retenues :**

- **Scène au Bar (outil 5)** — Nico traîne **avec deux copains de comptoir**. Tu
  approches, tu demandes pour Marco ; Nico fait le malin, te **bouscule** (*« Nico :
  jamais entendu parler. Dégage. »*). Ses copains, lâches, **se débinent** quand
  ça chauffe (flavor, pas de combat).
- **`SUBDUE` (pas de mort)** — tu le secoues, il parle (la voie est non létale par
  construction : impossible de le tuer → pas besoin de fail).
- **Confirmation au téléphone** — après l'aveu de Nico, l'Inconnu **rappelle**
  pour orienter vers les Loups (pont vers M6).

## M6 — Message aux Loups ✅ *(verrouillée)*

État actuel : `GOTO Chinatown` → `BEAT 3 Loups` (SP_PRESENT). Prime 250 $.

**Améliorations retenues :**

- **Scène de provocation (outil 5)** — les 3 Loups **glandent et fanfaronnent** ;
  l'un s'avance, crâneur (*« T'es perdu, l'ami ? »*) → la baston part.
- **Conséquence systémique (réalisme)** — tabasser 3 types en pleine rue **fait
  monter les étoiles** ; un beat de témoin qui appelle les flics, puis narration
  qui **oriente vers Pay'n'Spray / la fuite**. Apprend la boucle recherche →
  repeinture (première vraie chauffe du joueur).
- **Aucun échec.**

## M7 — Voiture volée ✅ *(verrouillée)*

État actuel : `GOTO Quais` → `KILL 2 gardes armés` → `ENTER_CAR` (caisse des
Loups) → `GOTO Commissariat` (planque). Prime 350 $. Premier appel signé **Tony**.

**Améliorations retenues :**

- **Intro Tony (outil 5, téléphone)** — au lancement, l'Inconnu **se nomme** :
  *« Tony. Tu peux m'appeler Tony, maintenant. »*
- **Scène des gardes (outil 5)** — aux Quais, 2 **gardes armés** veillent la
  caisse en discutant ; ils t'aggro dès l'approche.
- **Véhicule marqué + échec sélectif (outils 4 + 3)** — la **caisse des Loups**
  est l'objet de la mission : si elle est **détruite** (explosée / trop amochée)
  avant la livraison → **MISSION RATÉE** (*« Tony : t'as niqué la bagnole ?! Elle
  valait une fortune ! »*).
- **Scène de remise (outil 5)** — derrière le Commissariat, un **receleur**
  attend ; tu lui livres la caisse, il acquiesce et te paie (*« Tony : tu montes
  en grade, petit. »*).

## M8 — Représailles ✅ *(verrouillée)*

État actuel : `GOTO Garage` → `BEAT 3 gros bras` → `BEAT 2 tireurs` (ambush).
Prime 350 $. Défense du Garage, aujourd'hui sans entité ni échec.

**Améliorations retenues :**

- **Tony physiquement présent + défendu (outils 3 + 5)** — Tony est **à la porte
  du Garage** (*« Tony : ils arrivent ! Couvre l'entrée ! »*). Il est un PNJ
  **requis** : **s'il meurt → MISSION RATÉE** (justifié : Tony doit survivre
  jusqu'à l'Acte IV). Les assaillants peuvent le **prendre pour cible** → tu dois
  t'interposer.
- **Nudge AMU Nation** — narration d'intro qui pousse à **s'armer en chemin**
  (*« arme-toi »*).
- **Beats de vagues (outil 5)** — entre les deux vagues, accalmie trompeuse puis
  les tireurs surgissent (*« Ça se calme... non, ils reviennent ! »*).

## M9 — La tournée de Marco ✅ *(verrouillée)*

État actuel : `GOTO Commerces` → `BEAT 3 Loups` → `GOTO Bar`. Prime 300 $. Racket
callback, après la mort de Marco.

**Améliorations retenues :**

- **Tonalité « seul » (callback émotionnel)** — tu refais **seul** la tournée que
  tu faisais avec Marco. Au Commerce, le commerçant te **reconnaît** (*« C'est toi
  qui passes, maintenant ? ...Désolé, pour Marco. »*) — beat scripté mélancolique.
- **Règle racket (échec sélectif)** — **tuer un commerçant coopératif → MISSION
  RATÉE** (cf. règle dérivée).
- **Scène des Loups** — des Loups sont venus **se servir sur ton territoire** ;
  tu les renvoies (*« Pour toi, Marco. »*).
- **Scène d'info au Bar (outil 5)** — le vieux te confie avoir vu **« un type chic
  qui donnait des ordres aux Loups »** → première graine de Victor.

## M10 — L'entrepôt ✅ *(verrouillée)*

État actuel : `GOTO Quais` → `KILL 2 tireurs` (porte) → `KILL 3 gros bras`
(ambush, intérieur). Prime 400 $.

**Améliorations retenues :**

- **Assaut en deux temps (outil 5)** — **forcer la porte** (2 tireurs qui la
  verrouillent), beat *« Entrée dégagée »*, puis l'**embuscade jaillit** à
  l'intérieur (3 gros bras).
- **Le Loup qui s'échappe (pont vers M11)** — en fin de nettoyage, un **Loup
  survivant détale prévenir Rico** (narration / brève fuite) — installe le boss
  suivant (*« Le patron des Loups va nous tuer pour ça... »*).
- **Aucun échec** (pas d'élément requis).

## M11 — Rico le Loup ✅ *(verrouillée)*

État actuel : `GOTO Chantier` → `BEAT 2 tireurs` → `KILL Rico` (boss HP 5,
face-à-face). Prime 450 $. **Pivot narratif.**

**Améliorations retenues :**

- **Face-à-face renforcé (outil 5)** — le taunt existant reste ; Rico attend,
  entouré de ses derniers hommes.
- **Death-beat du retournement (le cœur de la mission)** — Rico à terre **ricane**
  avant de mourir : *« Tu crois qu'on a tué Marco ? Ouvre les yeux... tu bosses
  pour le vrai coupable. »* Beat scripté appuyé (joueur figé, gros plan
  narratif).
- **Appel évasif de l'Inconnu (pont vers Acte III)** — juste après, **le téléphone
  sonne** et l'Inconnu, **gêné, élude et raccroche sec** → installe le soupçon
  envers Tony.
- **Aucun échec** (boss non fuyard).

---

# ACTE III — La vérité (M12–M15)

## M12 — La mallette ✅ *(verrouillée)*

État actuel : `GOTO Chinatown` → `KILL 2 nettoyeurs` (ambush) → `GOTO Planque`.
Prime 350 $. Sarah (nouveau numéro) entre en scène.

**Améliorations retenues :**

- **Scène de découverte (outil 5)** — au parking de Chinatown : **un homme mort,
  une mallette près du corps**. Beat d'arrivée (caméra sur la scène, *« Tu n'es
  pas seul... »*).
- **Ramassage scripté de la mallette** — tu marches jusqu'à elle, **beat de
  ramassage** (*« Tu empoignes la mallette. »*) avant la fuite — la rend
  tangible sans système d'inventaire.
- **Nettoyeurs en embuscade** — 2 tireurs surgissent pour la mallette.
- **Remise à Sarah (outil 5)** — à la Planque, **Sarah te rejoint** (voix +
  silhouette), prend la mallette (*« C'est bien ce que je craignais. Il faut
  qu'on se voie. »*) → pont vers M13.

## M13 — Le témoin ✅ *(verrouillée)*

État actuel : `GOTO Bar` → `TALK Sarah` (alliée magenta) → `GOTO Planque`
(escorte). Prime 400 $. **Le compagnon revient (Sarah).**

**Améliorations retenues :**

- **Compagnon physique Sarah (outils 1 + 2)** — on **va chercher Sarah** au Bar,
  elle te suit, **monte/descend avec toi**, on la **dépose à la Planque**.
- **Échec sélectif (outil 3)** — Sarah est le témoin clé : **si Sarah meurt →
  MISSION RATÉE** (justifié).
- **Scène au Bar (outil 5)** — Sarah est planquée, nerveuse, **scrute les
  alentours** (*« On me suit. Vite, sors-moi d'ici. »*).
- **Embuscade en route (tension, outil 5)** — à mi-trajet, **les hommes de Victor
  l'ont repérée** : 1–2 tireurs surgissent ; tu les écartes **en protégeant
  Sarah** (qui se planque). Donne tout son sens au fail-sur-mort.
- **Arrivée à la Planque** — beat de mise à l'abri (*« Sarah est à l'abri. Pour
  l'instant. »*).

## M14 — Embuscade ✅ *(verrouillée)*

État actuel : `GOTO Planque` → `BEAT 3 gros bras` → `BEAT 2 tireurs` (ambush).
Prime 450 $. Défense, aujourd'hui sans entité ni échec.

**Améliorations retenues :**

- **Sarah présente + défendue (outils 3 + 5)** — Sarah est **barricadée à la
  Planque** (*« Ils sont partout ! »*) ; PNJ **requis** : **si Sarah meurt →
  MISSION RATÉE**. Les assaillants peuvent la viser → tu t'interposes.
- **Deux vagues avec beats (outil 5)** — gros bras puis tireurs en embuscade ;
  accalmie trompeuse entre les deux.
- **Reveal de fin** — *« Sarah : je sais où il garde les dossiers. Ses Bureaux. »*

## M15 — Les dossiers ✅ *(verrouillée)*

État actuel : `GOTO Bureaux` → `KILL 2 tireurs` (hall) → `BEAT 3 gros bras`
(ambush sortie). Prime 500 $.

**Améliorations retenues :**

- **Infiltration du hall (outil 5)** — entrer aux Bureaux (l'interaction libre
  `bureauVisit` est désactivée pendant la mission) ; des gardes **patrouillent**.
- **Ramassage scripté des dossiers** — beat de prise (*« Tu rafles les
  dossiers. »*), comme la mallette de M12.
- **Sortie coupée (ambush)** — des renforts te **bloquent la sortie** ; force le
  passage.
- **Reveal majeur (death/clue-beat)** — Sarah lit : *« Victor a ordonné
  l'assassinat de Marco. On le tient. »* — la preuve qui arme l'Acte IV.
- **Aucun échec** (pas de mécanique de perte des dossiers).

---

# ACTE IV — Victor (M16–M18 + épilogue)

## M16 — Sabotage ✅ *(verrouillée)*

État actuel : `ENTER_CAR Chinatown` → `CRUSH La Casse` → `ENTER_CAR Casino` →
`CRUSH La Casse`. Prime 550 $. Réutilise le broyeur.

**Améliorations retenues :**

- **Voitures de luxe marquées (outil 4)** — chacune est un **véhicule spécifique**
  (alarme / gardée). La voler **attire les hommes de Victor** (chauffe : ils te
  prennent en chasse).
- **Échec sélectif (outils 3 + 4)** — si la voiture de luxe est **détruite avant
  le broyeur** → **MISSION RATÉE** (*« Tony : fallait la broyer, pas la cramer ! »*).
  Justifié : il faut la livrer entière à la grue.
- **Scène au broyeur (outil 5)** — tu entres dans La Casse, **descends près de la
  grue**, et le **broyage scripté** écrase l'épave pendant que Tony jubile
  (*« Une de moins. Ça pique, hein Victor ? »*). Répété pour la 2e voiture.

## M17 — Bruno ✅ *(verrouillée)*

État actuel : `GOTO Casse` → `KILL 2 gardes` → `KILL Bruno` (boss HP 5,
face-à-face). Prime 600 $.

**Améliorations retenues :**

- **Présence de Bruno (outil 5)** — Bruno **règne sur La Casse**, te toise et te
  provoque (taunt existant : *« Victor veut ta tête, c'est moi qui vais la lui
  porter. »*). Ses gardes d'abord, puis lui.
- **Death-beat (pont vers le final)** — Bruno mourant lâche : *« Le vieux
  casino... Victor t'y attend. »* — désigne le lieu du boss final.
- **Aucun échec** (boss non fuyard).

## M18 — Le dernier appel ✅ *(verrouillée)*

État actuel : `GOTO Casino` → `KILL 4 gardes du corps` → `KILL Victor` (boss
HP 7, face-à-face). Prime 800 $. **Boss final.**

**Améliorations retenues :**

- **Remontée du Casino (outil 5)** — tu **traverses la salle** vers Victor, qui
  **monologue** calmement (*« Tu as du cran de venir jusqu'ici. »*) avant de
  lâcher ses gardes du corps (le pic d'opposition : 4 tireurs).
- **Death-beat final** — Victor, à terre, **avoue** : *« Marco allait parler. Je
  ne pouvais pas le laisser faire. Tout le reste... ce n'était que toi. »* Puis
  ta réplique de clôture (*« Tout ça... pour un ami. C'est fini, Victor. »*).
- **Aucun échec.**

## Épilogue ✅ *(verrouillé)*

État actuel : la cabine rouge sonne une dernière fois → décrocher → *« Silence...
puis la ligne coupe. FIN. »* puis muette.

**Améliorations retenues :**

- **Dernier appel habité (outil 5)** — on décroche : ce n'est pas le silence, mais
  **la voix de Sarah** qui remercie (*« C'est sorti ce matin. Tout le monde sait,
  pour Victor. Pour Marco. ...Prends soin de toi. »*) → puis la ligne coupe,
  **FIN**.
- **Ville ouverte en aftermath** — la cabine rouge se tait ; les **15 cabines
  bleues** continuent de sonner (missions secondaires rejouables), tous les
  services restent actifs. Le joueur garde la ville.

