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
| Gain : missions | ⏳ À faire | Récompense en $ à la complétion d'une mission (champ `reward` sur `MissionDef`). |
| Gain : ventes (casse, etc.) | ⏳ À faire | cf. POI ci-dessous. |
| Dépense : achats/services | 🔜 En cours de spec | cf. POI interactifs. |

---

## POI existants (générés)

Ces POI sont déjà posés sur la carte et détectés (bandeau HUD + ancrage de
missions). Aucun n'est encore *interactif* au sens « j'achète / je vends ».

| POI | Type généré | Rôle actuel | Interactivité visée | État |
|---|---|---|---|---|
| **Les Quais** (port) | quartier thématique | repère + ancrage mission | (ambiance ; trafic bateau plus tard) | ⏳ |
| **Chinatown** | quartier thématique | repère + ancrage mission | (ambiance) | ⏳ |
| **Chantier** (construction) | quartier thématique | repère + ancrage mission | (ambiance) | ⏳ |
| **La Casse** (junkyard) | quartier thématique | repère + ancrage mission | **Grue → broyeur** : vendre/détruire sa caisse contre $ | ⏳ À faire |
| **Commissariat** (police) | stamp 3×3 | repère | (entrer = prison/relâche si arrêté) | ⏳ idée |
| **Hôpital** | stamp 3×3 | repère + ancrage mission | **Soin** : entrer → vie restaurée (payant) | ⏳ À faire |
| **Pompiers** | stamp 3×3 | repère + ancrage mission | (camion de pompiers / mission incendie) | ⏳ idée |

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
- **État** : ⏳ À faire (UI modale + stamp + tuiles).

### 2. Hôpital — soin 🏥 *(POI déjà présent)*
- **Service** : entrer (à pied) → la vie remonte (gratuit ou payant, p.ex. 50 $).
- **Implémentation** :
  - Prérequis : transformer la vie HUD (`HUD_HEARTS`, aujourd'hui constante) en
    **variable** `playerHearts` + une source de **dégâts** (balles ennemies,
    collisions) pour que le soin ait un sens.
  - Effet : `poiAtTile()` == Hôpital → `playerHearts = HUD_HEARTS_MAX` (débit $).
- **État** : ⏳ À faire (dépend du système de vie/dégâts).

### 3. Pay'n'Spray — peinture auto 🎨
- **Service** : entrer **en voiture** → la caisse change de couleur **et on perd
  ses étoiles** (recherche police remise à zéro).
- **Implémentation** :
  - Prérequis : transformer les étoiles HUD (`HUD_STARS`, constante) en variable
    `wantedLevel` + une source qui le fait monter (crimes).
  - Nouveau POI/stamp `paint` (garage). À l'entrée en voiture : `carColor` =
    nouvelle teinte, `wantedLevel = 0`, petit fondu/son. Débit $.
- **État** : ✅ Fait (V1 sans nouvelles tuiles). Plusieurs garages éparpillés
  (`SPRAYS[]` dans `gta.ino`), snappés sur la **route** la plus proche
  (`findRoadSpot`) donc accessibles en voiture ; dessin pixel-art procédural
  (`drawSprayShop`, auvent rayé jaune/bleu). **Auto** : rouler dessus (recherche
  active) → `repaintCar()` : nouvelle couleur, `wantedClear`, débit 50 $ (gratuit
  si fauché, pour ne pas bloquer à 5 étoiles). **Positions provisoires** (grille
  large) — à replacer aux bons spots quand le client les aura choisis.

### 4. La Casse — grue & broyeur ♻️ *(quartier déjà présent)*
- **Service** : amener sa caisse sur une zone marquée → une **grue** la saisit et
  l'envoie au **broyeur** ; gain en $ (revente d'épave) selon l'état du véhicule.
- **Implémentation** :
  - Zone de dépose dans la bbox « La Casse » (point-cible existant). En voiture,
    à l'arrêt sur la zone, presser action → anim grue (sprite simple qui descend,
    soulève, déplace), véhicule détruit, `playerMoney += prime`, joueur éjecté.
  - Anim : peu de frames, pixels — pas de gros assets.
- **État** : ⏳ À faire.

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

1. **Hôpital interactif** : entrer à pied → `playerHearts` restauré (payant). Le
   système de vie est désormais en place.
2. **AMU Nation** : tuiles + stamp + UI d'achat (le plus gros morceau « interface »).
3. **La Casse** : grue/broyeur (revente, selon `carHp`).
4. **Récompenses de mission** en $ (boucle économique complète).
5. **Pay'n'Spray** : remplacer les positions provisoires par les bons spots, et
   éventuellement passer à un vrai stamp/tuiles dédiés (cf. POI #3).
