# POI & économie — spec & avancement

Ce fichier liste les **points d'intérêt (POI)** du demake et leur degré
d'interactivité, plus le **système d'argent** qui les alimente. Il sert de
feuille de route : ce qui existe, ce qui est en cours, ce qui reste à faire.

Rappel technique : les POI sont **générés** (`tools/pois.py` → `gta/citymap_data.cpp`,
table `cityPois[]`). Un POI = une *bbox* en tuiles + un point-cible. Le jeu sait
déjà :

- détecter dans quel POI se trouve le joueur (`poiAtTile`, bandeau HUD) ;
- résoudre un POI par son nom pour les missions (`findPoi`).

Les POI interactifs se branchent donc sur cette détection : quand le joueur (à
pied ou en voiture selon le service) entre dans la bbox et presse l'action, on
déclenche l'effet.

---

## Système d'argent (base de l'économie)

| Élément | État | Détail |
|---|---|---|
| Variable `playerMoney` (int32) | ✅ Fait | Solde du joueur, init 0. |
| HUD billet + montant | ✅ Fait | `"$<montant>"` en vert sous les cœurs (`drawTopHud`). |
| Gain : pietons abattus | ✅ Fait | À la mort d'un piéton (poing/arme/écrasement), 40 % de chance de lâcher un billet (5–50 $) et 12 % une arme de poing (pistolet + munitions). Butin au sol ramassé à pied (`loots[]`, `dropLoot`, `tryPickupLoot`). |
| Gain : missions | ✅ Fait | Récompense en $ à la complétion (champ `reward` sur `MissionDef`, crédité dans `missionProgress` ; narration « Mission ! +$nnn » + cha-ching). Primes 120–500 $ selon la mission. |
| Gain : ventes (casse, etc.) | ✅ Fait | Broyeur de La Casse (prime selon `carHp`). |
| Gain/dépense : jeu (casino) | ✅ Fait | Machine à sous du Casino (mise / gains, retour ~0,90). |
| Gain : braquage (Commerces) | ✅ Fait | +100 $ si réussite (proba selon l'arme), sinon -1 PV. |
| Dépense : achats/services | ✅ Fait | Hôpital (soin 40 $), AMU Nation (armes), Pay'n'Spray (peinture), Le Bar (verre 10 $). |
| Pénalité : arrestation/mort | ✅ Fait | Perte de la moitié du fric (commissariat / hôpital). |

---

## POI existants (générés)

Ces POI sont déjà posés sur la carte et détectés (bandeau HUD + ancrage de
missions). Plusieurs sont désormais **interactifs** (✅) ; les autres restent
des repères/ancres de mission sans action propre.

| POI | Type généré | Rôle actuel | Interactivité visée | État |
|---|---|---|---|---|
| **Les Quais** (port) | quartier thématique | repère + ancrage mission | (ambiance ; trafic bateau plus tard) | ⏳ |
| **Chinatown** | quartier thématique | repère + ancrage mission | (ambiance) | ⏳ |
| **Chantier** (construction) | quartier thématique | repère + ancrage mission | (ambiance) | ⏳ |
| **La Casse** (junkyard) | quartier thématique | repère + ancrage mission | **Grue → broyeur** : vendre/détruire sa caisse contre $ | ✅ Fait |
| **Le Casino** | stamp 3×3 | repère + ancrage M18 | **Machine à sous** : miser, faire tourner les rouleaux | ✅ Fait |
| **Commissariat** (police) | stamp 3×3 | repère + ancrage M7 | **Arrestation** : entrer avec ≥1 étoile → busted | ✅ Fait |
| **Hôpital** | stamp 3×3 | repère + ancrage mission | **Soin** : entrer → vie restaurée (payant) | ✅ Fait |
| **Pompiers** | stamp 3×3 | repère + ancrage mission | (camion de pompiers / mission incendie) | ⏳ idée |
| **Le Garage** | stamp 3×3 | ancrage M1/M7/M8 | (réparer/repeindre sa caisse) | ⏳ idée |
| **Le Bar** | stamp 3×3 | ancrage M5 | **Tournée du poivrot** : -10 $, réplique au hasard | ✅ Fait |
| **Les Bureaux** | stamp 3×3 | ancrage M15 | — | ⏳ |
| **Les Commerces** | stamp 3×3 | ancrage racket M2/M9 | **Braquage** : réussite selon l'arme (+100 $ / -1 PV) | ✅ Fait |
| **Planque** | stamp 3×3 | base joueur + tél rouge trame | **Dormir** : écran noir → vie pleine (3 ❤). Tél rouge à venir | ✅ dodo / ⏳ trame |

**Découpage complet en quartiers** : ✅ Fait. En plus des 4 quartiers
thématiques, **chaque district** reçoit désormais un nom (style GTA :
*Portland, St. Mark's, Vice Point, Ocean Beach…*) via
`pois.assign_district_names` → un POI « quartier » par district dans
`cityPois[]`. On est donc **toujours dans un quartier nommé** (bandeau HUD).
Aucune modif du code jeu : `poiAtTile` choisit la plus petite bbox englobante,
donc les stamps 3×3 (Hôpital…) gardent la priorité sur leur case. Les noms sont
dessinés sur l'aperçu PNG (`render_png`).

---

## POI interactifs demandés

### 1. AMU Nation — armurerie 🔫
- **Service** : ouvre un **magasin d'armes**. Interface de sélection : on choisit
  une arme, on l'achète ; si on la possède déjà, l'achat recrédite des munitions.
- **Implémentation** :
  - Nouveau **stamp** `ammunation` dans `tools/pois.py` (`STAMP_DEFS` +
    `STAMP_NAMES["ammunation"] = "AMU Nation"`) → nécessite 3 tuiles
    (`amu_facade`, `amu_sign`, `amu_door`) dans le tileset (`assets/tileset.csv`
    + PNG sources, puis `build_assets.py`).
  - Côté jeu : état `SHOP_OPEN`, menu modal (liste `WEAPONS[]`, prix, munitions,
    solde). A/B pour naviguer, A acheter, MENU fermer. Geler le monde pendant.
  - Table de prix : `WEAPON_PRICE[WEAPON_COUNT]` + `AMMO_PRICE[WEAPON_COUNT]`.
- **État** : ✅ Fait (V1 procédurale, sans nouvelles tuiles, comme Pay'n'Spray).
  **Positions générées** par `tools/pois.py` (`place_services`) : 8 armureries en
  **bord de route, côté sans trottoir**, disséminées sur la carte, déterministes,
  exportées dans `citymap.h` (`cityAmmus[]`) et lues telles quelles par `gta.ino`
  (plus de placement aléatoire ni de snap runtime) ; devanture
  pixel-art (`drawAmmuShop`, auvent rouge + enseigne jaune). S'approcher et
  presser **A** ouvre l'**UI modale** (monde gelé) : liste des armes (icône 6×6 +
  nom + prix), HAUT/BAS choisit, **A** achète, **B/MENU** ferme. 1er achat = prix
  d'arme + 1 lot de munitions, ensuite = rechargement (`WEAPON_PRICE[]` /
  `AMMO_PRICE[]`). Son « cha-ching » à l'achat, son grave si trop fauché.

### 2. Hôpital — soin 🏥 *(POI déjà présent)*
- **Service** : entrer (à pied) → la vie remonte (gratuit ou payant, p.ex. 50 $).
- **Implémentation** :
  - Prérequis : transformer la vie HUD (`HUD_HEARTS`, aujourd'hui constante) en
    **variable** `playerHearts` + une source de **dégâts** (balles ennemies,
    collisions) pour que le soin ait un sens.
  - Effet : `poiAtTile()` == Hôpital → `playerHearts = HUD_HEARTS_MAX` (débit $).
- **État** : ✅ Fait. Entrer à pied dans la bbox « Hôpital » en étant blessé →
  soin complet contre 40 $ (gratuit si fauché), via la machine à états
  (`SEQ_HEAL` : croix verte pulsante + jingle ascendant do-mi-sol). Une fois par
  entrée ; narration « Pas assez ($40) » si solde insuffisant.

### 3. Pay'n'Spray — peinture auto 🎨
- **Service** : entrer **en voiture** → la caisse change de couleur **et on perd
  ses étoiles** (recherche police remise à zéro).
- **Implémentation** :
  - Prérequis : transformer les étoiles HUD (`HUD_STARS`, constante) en variable
    `wantedLevel` + une source qui le fait monter (crimes).
  - Nouveau POI/stamp `paint` (garage). À l'entrée en voiture : `carColor` =
    nouvelle teinte, `wantedLevel = 0`, petit fondu/son. Débit $.
- **État** : ✅ Fait (V1 sans nouvelles tuiles). **Positions générées** par
  `tools/pois.py` (`place_services`) : 6 garages en **bord de route, côté sans
  trottoir**, bien dispersés, déterministes, exportés dans `citymap.h`
  (`citySprays[]`) et lus tels quels par `gta.ino` (plus de placement aléatoire
  ni de snap runtime). Dessin pixel-art procédural (`drawSprayShop`, auvent rayé
  jaune/bleu). **Auto** : rouler dessus (recherche active) → `repaintCar()` :
  nouvelle couleur, `wantedClear`, débit 50 $ (gratuit si fauché, pour ne pas
  bloquer à 5 étoiles).

### 4. La Casse — grue & broyeur ♻️ *(quartier déjà présent)*
- **Service** : amener sa caisse sur une zone marquée → une **grue** la saisit et
  l'envoie au **broyeur** ; gain en $ (revente d'épave) selon l'état du véhicule.
- **Implémentation** :
  - Zone de dépose dans la bbox « La Casse » (point-cible existant). En voiture,
    à l'arrêt sur la zone, presser action → anim grue (sprite simple qui descend,
    soulève, déplace), véhicule détruit, `playerMoney += prime`, joueur éjecté.
  - Anim : peu de frames, pixels — pas de gros assets.
- **État** : ✅ Fait. Zone de dépose marquée (carré rayé jaune/noir,
  `drawCasseZone`) sur le point-cible du POi « La Casse ». Y amener sa caisse
  (hors voiture de mission) déclenche `SEQ_CRUSH` : pince qui descend (vrombissement
  descendant) → caisse écrasée en bloc qui s'aplatit + éclats (grincement) →
  prime `+$nnn` qui monte (cha-ching), joueur éjecté à pied. Prime = `40 + carHp*4`
  (≈40–160 $). La voiture broyée disparaît (`carGone`) : plus dessinée ni
  re-montable tant qu'on n'en vole pas une autre.

### 5. Le Casino — machine à sous 🎰 *(POI déjà présent)*
- **Service** : entrer à pied sur une tuile du POI « Le Casino » (là où le
  bandeau s'affiche) et presser **A** → ouvre une **machine à sous** modale
  (monde gelé, comme le magasin AMU). **Haut/Bas** règle la mise (1–50 $,
  plafonnée par le solde), **B** = mise max, **A** = lancer, **MENU** = sortir
  (comme pour sortir d'une voiture).
- **Implémentation** :
  - Logique de tirage/gains **pure** dans `gta/slot.h` (`slotEvaluate`,
    `slotNext` xorshift), couverte par `tools/host_test/test_slot_host.cpp`.
  - 5 symboles, rouleaux animés s'arrêtant un par un ; rendu procédural
    (`drawSlotReel`, pas de nouvelles tuiles). Entrée via
    `poiAtTile == findPoi("Le Casino")`.
  - Table de gains (×mise, total recrédité) : 🍒×3 = ×4, 🍋×3 = ×5, 🔔×3 = ×10,
    BAR×3 = ×20, 7×3 = ×50, deux cerises = ×2. Retour moyen ~0,90 (avantage
    maison ~10 % → pas de farm d'argent infini).
- **État** : ✅ Fait. (Le POI reste aussi l'ancre du boss final M18 de la
  campagne — les deux usages coexistent.)

### 6. La Planque — dormir 😴 *(POI déjà présent)*
- **Service** : à pied sur la bbox « Planque », presser **A** → **dodo**. Écran
  qui devient noir (petit « Zzz »), puis réveil **devant la porte** avec la vie
  pleine (**3 cœurs rouges**, gilet retiré). Gratuit.
- **Implémentation** : nouvelle séquence `SEQ_SLEEP` (réutilise la phase
  `PH_FADE` : écran noir N frames → `playerHearts = PLAYER_HEARTS_MAX` +
  `respawnAtPoi("Planque")`). N'efface ni les étoiles ni la mission en cours
  (≠ mort/arrestation). *(La Planque hébergera aussi le téléphone rouge de la
  trame, à venir.)*
- **État** : ✅ Fait (dodo). ⏳ Téléphone rouge (campagne).

### 7. Le Bar — la tournée du poivrot 🍺 *(POI déjà présent)*
- **Service** : à pied sur la bbox « Le Bar », **A** → **-10 $** et une
  **réplique au hasard** d'un vieux poivrot (10 lignes, ton GTA rigolard :
  Pay'n'Spray & poulets, La Casse, AMU Nation, casino…). Trop fauché (< 10 $) :
  pas de verre.
- **Implémentation** : `barDrink()` — `addMoney(-10)`, `narrate(BAR_LINES[rng%10])`.
- **État** : ✅ Fait.

### 8. Les Commerces — braquage 🔫 *(POI déjà présent)*
- **Service** : à pied sur la bbox « Commerces », **A** → tentative de braquage.
  **Réussite → +100 $** ; **échec → -1 PV** (le commerçant riposte). La proba
  dépend de l'**arme tenue** :

  | Arme sélectionnée | Réussite |
  |---|---|
  | Aucune (poing) | 10 % |
  | Pistolet | 30 % |
  | Pompe / PM (SMG) | 80 % |
  | Grenade / Bazooka | 100 % |

- **Implémentation** : `robStore()` — proba selon `curWeapon`, `aiRngNext % 100`,
  `addMoney(100)` ou `hurtPlayer(1, false)`.
- **État** : ✅ Fait.

### 9. Commissariat — arrestation 🚓 *(POI déjà présent)*
- **Service** : entrer (à pied **ou** en voiture) dans la bbox « Commissariat »
  avec **au moins une étoile** → **arrestation directe** (même cinématique
  `SEQ_BUSTED` que dans la rue : bandeau « ARRETE », écran noir, perte de la
  moitié du fric, étoiles remises à zéro, réapparition au commissariat).
- **Implémentation** : détection d'entrée (`poiAtTile`, flag `commInside` pour
  ne déclencher qu'une fois) + `wanted.level >= 1` → `bustedPlayer()`.
- **État** : ✅ Fait.

> **Bonus casino** : tenter de lancer la machine à sous **sans assez d'argent**
> → le **videur** te sort de force : tu te retrouves **allongé devant** (relevage
> auto), **-1 PV**, et une réplique au hasard (`casinoBouncerThrowOut`).

---

## POI supplémentaires pertinents (proposés)

Idées dans l'esprit « acheter un service/bien » ou « vendre pour gagner $ ».

| POI | Catégorie | Idée |
|---|---|---|
| **Marché / 24-7 (épicerie)** | acheter | Snacks → regagne un peu de vie ; petites munitions. |
| **Concession auto** | acheter | Acheter un véhicule qui *spawn* à côté (caisse perso persistante). |
| **Atelier / tuning** | acheter | Améliorer la voiture (vitesse, blindage) contre $. |
| **Banque** | vendre/voler | Coffre = gros gain mais monte les étoiles (mini-braquage). |
| **Bijouterie / dépôt-vente** | vendre | Revendre des objets ramassés. |
| **Bar / boîte** | service | Récupérer des contacts de mission ; mini-pari. |
| **Station-service** | service | Faire le plein (si jauge carburant un jour) ; explosif si on tire dessus. |
| **Planque / safehouse** | service | Sauvegarder, changer de tenue, garer la caisse. |
| **Taxi / dépôt** | gagner | Lancer une série de courses (déjà esquissé par `OBJS_TAXI`). |
| **Port — import/export** | gagner | Livrer des véhicules volés ciblés contre prime (liste de demandes). |

---

## POI de la trame principale (campagne) 🆕

Nouveaux POI **proposés** pour la trame principale scénarisée (18 missions +
épilogue) décrite dans [campagne/](campagne/README.md). À générer via `tools/pois.py`
(comme les quartiers thématiques / services : dessin procédural possible, pas
forcément de nouvelles tuiles). Bandeau HUD + ancrage de missions par nom
(`findPoi`), comme les POI existants.

| Nouveau POI | Rôle | Missions | État |
|---|---|---|---|
| **Planque** | Base du joueur. Ancre le **téléphone rouge unique** de la trame (sonne au lancement, se réarme après chaque mission, point de reprise après un échec). | toutes | ⏳ À faire |
| **Le Garage** | Garage de Marco puis QG de Tony. | M1, M7, M8 | ⏳ À faire |
| **Le Bar** | Nico traîne devant. | M5 | ⏳ À faire |
| **Les Bureaux** | Bureaux de Victor (dossiers à dérober). | M15 | ⏳ À faire |
| **Le Casino** | Repaire final de Victor. POI déjà posé + **machine à sous** interactive (cf. POI #5). | M18 | ✅ POI posé ; M18 ⏳ |
| **Les Commerces** | Rangée de 3 supérettes (repères fixes) pour le racket et son callback. | M2, M9 | ⏳ À faire |

POI **existants réutilisés** par la campagne (aucun nouveau travail) : **Les
Quais** (docks), **La Casse** (broyeur de sabotage en M16), **Chantier** (mort
de Marco M4 ; ancienne usine de Rico M11), **Commissariat** (planque M7),
**Hôpital** (soin entre missions). Le « quartier des Loups » réutilise un
**district nommé** existant + repères fixes.

> **Lien téléphones bleus** : l'ancienne cabine bleue « Mauvaise affaire »
> (Marco) est **retirée** des missions secondaires — Marco devient le pivot de
> la trame principale. Elle est remplacée par une **« Livraison de pizza »**
> (mission jetable, chrono, 0 nouvelle mécanique). Voir `campagne/README.md` §1 et §9.

---

## Travail déjà livré dans ce lot

- ✅ `playerMoney` + affichage HUD (billet vert sous les cœurs).
- ✅ Butin des piétons : argent et/ou pistolet, ramassage au sol.
- ✅ Tir des armes à feu = **petits pixels** qui filent (gerbe pour la pompe),
  fini le gros trait blanc (`fireBullets` / `drawBullets`).
- ✅ **Vie variable** (`playerHearts`, 3 cœurs) : balle de flic = -1 cœur (invuln.
  brève), explosion de voiture = mort. HUD branché sur la variable.
- ✅ **Recherche police variable** (`wanted`, machine d'état pure `wanted.h` +
  test host) : 3 meurtres rapprochés (<20 s) = 1 étoile ; une étoile retombe
  après 60 s sans crime ; montée jusqu'à 5 (gelée à 5).
- ✅ **Police** : sous recherche, des piétons spawn en flics (bleus) qui foncent
  sur le joueur. Contact à pied = **arrestation** (commissariat, perte d'argent) ;
  à ≥2 étoiles, ils **tirent** (perte de cœur).
- ✅ **PV des PNJ** : 3 coups de poing ou 1 balle d'arme à feu pour tomber.
- ✅ **PV des voitures** (~30) : tirs et accidents les usent ; fumée légère puis
  dense, puis explosion. Dedans = mort → **hôpital** (« MORT », perte d'argent).

## Prochaines étapes recommandées (ordre)

1. ~~**Hôpital interactif**~~ ✅ Fait.
2. ~~**AMU Nation**~~ ✅ Fait (V1 procédurale + UI d'achat modale).
3. ~~**La Casse**~~ ✅ Fait (grue/broyeur, prime selon `carHp`).
4. ~~**Récompenses de mission**~~ ✅ Fait (primes en $, boucle économique complète).
5. ~~**Pay'n'Spray** : remplacer les positions provisoires par les bons spots~~
   ✅ Fait (positions générées en bord de route, côté sans trottoir). Reste
   éventuellement : passer à un vrai stamp/tuiles dédiés (cf. POI #3).
6. ~~**Le Casino** : machine à sous~~ ✅ Fait (mini-jeu modal, `slot.h` + host test).
7. ~~**Planque** (dodo), **Le Bar** (poivrot), **Les Commerces** (braquage),
   **Commissariat** (arrestation)~~ ✅ Fait.
8. **Prochains POI interactifs** au choix : Le Garage (réparer/peindre la
   caisse), Pompiers (mission incendie), Les Bureaux. Même patterns que ci-dessus.
