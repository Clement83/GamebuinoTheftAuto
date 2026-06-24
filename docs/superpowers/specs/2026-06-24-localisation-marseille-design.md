# Localisation marseillaise de la trame (reskin texte)

Date : 2026-06-24 · Branche : `gta-marseille`

## Objectif & scope

Rendre **toute la trame plus marseillaise** : on réécrit **tous les textes et
toutes les répliques PNJ** (titres de mission, narration des objectifs, taunts
de boss, scènes d'embuscade/livraison, cutscenes de mort/départ, fail messages,
épilogue). Accent **caricatural à fond** mais lignes courtes (écran 80×64).

**On ne change PAS la campagne** : mêmes 18 missions, même ordre (`STORY_SEQ`),
mêmes mécaniques (PV de boss, nombre/type d'ennemis, objectifs, primes). Aucune
modif de `tools/ai.py`/`engine.py` (pas de données de trame là-bas → pas de
souci de parité).

**Deux seuls changements structurels, explicitement validés :**
1. Le turf du gang (ex-`Chinatown`, POI supprimé de la carte) est **repointé sur
   un quartier nord réel** (voir §3) — les objectifs concernés montent au nord.
2. La mission finale M18 est **repointée du POI `Le Casino` vers `Vieux-Port`**.

**Choix de fidélité assumés (on s'écarte volontairement du script fourni) :**
- Les dernières paroles de Marius (« César… c'est César… ») **ne sont PAS
  ajoutées** à la mort en M4. La campagne existante construit la vérité
  progressivement (le joueur est manipulé contre les Nordistes, puis le Sanglier
  l'envoie vers Costa). Nommer Costa dès M4 casserait ce mystère. La révélation
  reste celle du code actuel (M11 le Sanglier → Costa).
- La « glacière sur les plages » (script M12) et « la cabane aux Goudes »
  (M13/M14) sont rendues **par le texte uniquement** : les objectifs gardent
  leurs POI actuels (parking quartiers nord, Planque). On évoque Goudes/plage/
  cabane dans les répliques sans déplacer les objectifs.

## 1. Mapping des personnages (renommage global cohérent)

| Existant | Marseille | Notes |
|---|---|---|
| Marco | **Marius** (Rossi) | mentor, meurt en M4 |
| Victor | **Costa** (César Costa) | boss final, jamais vu avant la fin |
| Rico le Loup | **Jo le Sanglier** / « le Sanglier » | boss #1 |
| Bruno | **Paulo** | boss #2, bras droit |
| Nico | **Dédé** | indic du PMU |
| Sarah | **Sonia** | ex-comptable de Costa (plus « journaliste ») |
| Tony | **Jeannot** | le mécano, patron du garage |
| « les Loups » / « un Loup » | **les Nordistes / un Nordiste** | gang |
| « petit » / « gamin » | **minot** | le joueur (Alex « le Minot ») |
| Joe (mission secondaire) | **Riton** | évite la confusion avec Jo le Sanglier |
| « journaliste » | **comptable** | rôle de Sonia |

Lexique caricatural à saupoudrer (lisible) : *minot, collègue, vé, peuchère,
fada, dégun, mèfi, emboucaner, cacou, le cousin, pastaga, l'OM, brave, ça pique,
ça va le rendre chèvre*.

## 2. Parité des clés `strcmp` (mod_mission.h)

Les taunts/livraisons/fail sont keyés **par titre de mission** via `strcmp`. En
renommant les titres, **on met à jour les clés en miroir**, sinon le taunt
disparaît silencieusement. Table titres :

| Index | Ancien titre | Nouveau titre |
|---|---|---|
| 1  | Le dernier trajet | **Le dernier trajet** (inchangé) |
| 16 | Premier jour | **Le colis du port** |
| 17 | Les assurances | **Les protections** |
| 18 | Mauvaise dette | **Le cousin Remi** |
| 19 | Un nom | **Dede le PMU** |
| 20 | Message aux Loups | **Les Nordistes** |
| 21 | Voiture volee | **Le fourgon** |
| 22 | Represailles | **Le garage attaque** |
| 23 | Tournee de Marco | **La tournee de Marius** |
| 24 | L'entrepot | **L'entrepot** (inchangé) |
| 25 | Rico le Loup | **Le Sanglier** |
| 26 | La mallette | **La glaciere** |
| 27 | Le temoin | **La comptable** |
| 28 | Embuscade | **La cabane** |
| 29 | Les dossiers | **Les papiers** |
| 30 | Sabotage | **Le sabotage** |
| 31 | Bruno | **Paulo** |
| 32 | Le dernier appel | **Le Vieux-Port** |

Clés `strcmp` à mettre à jour : `bossTauntLines` (« Rico le Loup »→« Le
Sanglier », « Bruno »→« Paulo », « Le dernier appel »→« Le Vieux-Port ») ;
`ambushTauntLines` (« Les assurances »→« Les protections », « Mauvaise
dette »→« Le cousin Remi », « Message aux Loups »→« Les Nordistes »,
« L'entrepot » idem, « Embuscade »→« La cabane », « Les dossiers »→« Les
papiers ») ; `deliveryLines` (« Voiture volee »→« Le fourgon », « Les
assurances »→« Les protections ») ; fail M9 (« Tournee de Marco »→« La tournee
de Marius »).

## 3. Carte : turf du gang « Quartiers Nord »

- L'ancien `Chinatown` était au **centre** de carte (centre px ~812,436) → pas
  « au nord ». Le vrai nord = bande tile y 0–32.
- **Décision** : on rebaptise le slot POI **`La Rose`** (`citymap_data.cpp:173`,
  coords `126,0,198,32`, centre `1284,100` — une cité bien au nord) en
  **`Cite Nord`** (libellé court qui tient à l'écran ; la narration dit « les
  quartiers nord » en toutes lettres). Tous les objectifs ex-`Chinatown` pointent
  désormais sur `Cite Nord` → ils remontent au nord.
- `music_data.cpp:70` : zone `"Chinatown"` → `"Cite Nord"`.
- À vérifier en implémentation : `city/overlay.json` (libellé `La Rose` éventuel)
  et largeur du libellé sur la carte.

> NB : le POI `Le Casino` reste tel quel (utilisé par M16 et la mission pompier
> secondaire) ; seul M18 est repointé sur `Vieux-Port`.

## 4. Réécritures — `gta/game_state.h` (trame)

Format : `O# TYPE [POI]` → `text` / `doneText`.

### M4 — OBJS_DEAL « Le dernier trajet » (la mort de Marius)
- O1 GOTO Le Garage (join) → « Marius : un dernier rendez-vous, ce soir. Passe me prendre au garage du cousin. » / « Marius : direction le chantier de l'Estaque. »
- O2 GOTO Chantier (die) → « Emmene Marius au chantier. Il est nerveux ce soir. » / « Le chantier. Marius descend, mefiant... une silhouette l'attend dans l'ombre. »
- O3 KILL Chantier → « Le tueur fonce sur toi. Le laisse pas filer, vé ! » / « Justice est faite. ...pour l'instant. »

### M1 — OBJS_M1 « Le colis du port »
- O1 GOTO Le Garage → « Premier jour, minot. Marius, le bras droit du patron, t'attend au garage du cousin. Vas-y a pied. »
- O2 TALK Le Garage (join) → « Marius : deux secondes minot, j'arrive ! » / « Marius : la caisse est garee a cote. Monte, on a un colis a prendre aux quais. »
- O3 GOTO Les Quais (delivery) → « Direction les quais. Roule peinard, mefi aux flics. » / « Colis pris. Marius : nickel, minot. Maintenant ramene-moi. »
- O4 GOTO Le Garage (leave) → « Ramene Marius au garage du cousin. »

### M2 — OBJS_M2 « Les protections » (racket)
- O1 GOTO Le Garage → « Jour de tournee. Marius t'attend au garage. Vas-y a pied le chercher. »
- O2 TALK (join) → « Marius : deux secondes minot, j'arrive ! » / « Marius : la tournee des protections. Tu regardes et t'apprends. Suis-moi. »
- O3 GOTO Commerces (delivery) → « Premier commerce. Regarde Marius faire. » / « Marius : tu vois ? Facile. Lui au moins il comprend vite. »
- O4 GOTO Les Bureaux → « Le suivant fait le difficile, aux bureaux. Rejoins Marius. »
- O5 SUBDUE Les Bureaux → « Ce gerant refuse et te saute dessus. Mate-le, le tue pas. » / « Il crache les sous. Marius : voila comment on fait, minot. »
- O6 GOTO Le Bar (delivery) → « Encore un : le vieux du Bar paie toujours rubis sur l'ongle. » / « Le vieux paie et t'offre le pastaga. Ca requinque : pleine forme ! »
- O7 GOTO Chantier → « Dernier client, au chantier. Marius : celui-la... je le sens pas. Doucement. »
- O8 BEAT Chantier (ambush) → « Un type t'attend, plante entre deux gros bras. » / « Marius : quelqu'un nous a vendus. On reglera ca. »
- O9 GOTO Le Garage (leave) → « Tournee finie. Ramene Marius au garage. »

### M3 — OBJS_M3 « Le cousin Remi »
- O1 GOTO Le Garage → « Le cousin Remi doit du fric et fait le mort. Marius veut un exemple. Va le chercher au garage. »
- O2 TALK (join) → « Marius : deux secondes minot, j'arrive ! » / « Marius : ce Remi se fout de nous depuis trop longtemps. Suis-moi. »
- O3 GOTO Cite Nord → « Remi se planque aux quartiers nord, entoure de ses gros bras. Approche. »
- O4 BEAT Cite Nord → « Ecarte ses hommes de main. » / « La voie est libre. Reste Remi. »
- O5 KILL Cite Nord → « Il detale, le fada ! Rattrape-le. » / « Dette reglee. Marius : il s'en souviendra... si sa tete s'en souvient. »
- O6 GOTO Le Garage (leave) → « C'est fait. Ramene Marius au garage. »

### M5 — OBJS_M5 « Dede le PMU »
- O1 GOTO Le Bar → « Inconnu (au tel) : tu veux savoir pour Marius ? Trouve Dede. Il traine au PMU, devant le Bar. »
- O2 SUBDUE Le Bar → « Dede crane devant ses copains de comptoir et te bouscule. Secoue-le, sans le tuer. » / « Ses copains se debinent. Dede : les Nordistes cherchent un type depuis des semaines... c'est eux. »

### M6 — OBJS_M6 « Les Nordistes »
- O1 GOTO Cite Nord → « Inconnu : envoie-leur un message. Monte dans les quartiers nord. » / « Les voila, accoudes au mur. Ils t'ont calcule. »
- O2 BEAT Cite Nord → « Mets trois Nordistes au tapis. Qu'ils comprennent. » / « Ca devrait les remuer. Inconnu : bien joue, minot. »

### M7 — OBJS_M7 « Le fourgon »
- O1 GOTO Les Quais → « Jeannot - l'inconnu a un nom : un fourgon des Nordistes dort aux quais. » / « Deux gardes armes le surveillent. »
- O2 KILL Les Quais → « Occupe-toi des deux gardes. » / « La voie est libre. Le fourgon est a toi. »
- O3 ENTER_CAR Les Quais → « Embarque le fourgon des Nordistes. »
- O4 GOTO Commissariat (delivery) → « Planque-le derriere le commissariat. Roule peinard. » / « Jeannot : parfait. Tu montes en grade, minot. »

### M8 — OBJS_M8 « Le garage attaque »
- O1 GOTO Le Garage → « Jeannot : ils ont retrouve la planque ! Ramene-toi, et arme-toi en chemin. » / « Les Nordistes debarquent. Tiens bon. »
- O2 BEAT Le Garage → « Premiere vague : repousse-les ! » / « Ca se calme... oh non, ils reviennent ! »
- O3 BEAT Le Garage → « Deuxieme vague, et ils sont armes. Couvre-toi ! » / « Jeannot : ils deviennent nerveux. Merci, minot. »

### M9 — OBJS_M9 « La tournee de Marius »
- O1 GOTO Commerces (client) → « Jeannot : reprends la tournee de Marius aux commerces. C'est toi le patron, maintenant. » / « Un commercant : c'est toi qui passes ? ...Desole, pour Marius. C'etait un brave. »
- O2 BEAT Commerces → « Des Nordistes sont venus se servir. Renvoie-les chez eux. » / « Pour toi, Marius. »
- O3 GOTO Le Bar (client) → « Le vieux du Bar a vu quelque chose. Va l'ecouter. » / « Le vieux : un type chic donnait des ordres aux Nordistes... va savoir qui. »

### M10 — OBJS_M10 « L'entrepot »
- O1 GOTO Les Quais → « Jeannot : on a trouve le stock des Nordistes, aux entrepots des quais. » / « Des gardes verrouillent l'entree. »
- O2 KILL Les Quais → « Force le passage : deux gardes armes a la porte. » / « Entree degagee. »
- O3 KILL Les Quais → « Nettoie l'entrepot. Que personne ressorte. » / « Le patron des Nordistes va nous tuer pour ca... tant pis. »

### M11 — OBJS_M11 « Le Sanglier » (boss, PV=5)
- O1 GOTO Chantier → « Jeannot : Jo le Sanglier, le chef des Nordistes, se terre a la vieille usine. Finis-le. » / « Ses hommes te coupent la route. »
- O2 BEAT Chantier → « Ecarte sa garde rapprochee. » / « Reste le Sanglier. Il tombera pas en un coup. »
- O3 KILL Chantier → « Le Sanglier est coriace. Acharne-toi. » / « Le Sanglier, a terre : on a jamais touche Marius, minot. Tu bosses pour le vrai coupable... Costa. »

### M12 — OBJS_M12 « La glaciere »
- O1 GOTO Cite Nord → « Sonia (numero inconnu) : je peux prouver ce qu'a dit le Sanglier. Une glaciere, planquee aux quartiers nord. » / « Un homme mort, une glaciere pres du corps. Mais t'es pas seul... »
- O2 KILL Cite Nord → « Des nettoyeurs viennent pour la glaciere. Prends-les de vitesse. » / « Les nettoyeurs sont a terre. La glaciere est restee pres du corps. »
- O3 GOTO Cite Nord → « Marche jusqu'au corps et empoigne la glaciere. » / « Tu empoignes la glaciere. Maintenant file a la planque. »
- O4 GOTO Planque → « Rapporte la glaciere a la planque. » / « Sonia : c'est bien ce que je craignais. Faut qu'on se voie. »

### M13 — OBJS_M13 « La comptable » (escorte Sonia)
- O1 GOTO Le Bar → « Sonia, l'ancienne comptable de Costa, t'attend au Bar. Vas-y a pied. »
- O2 TALK (join Sonia, count=1) → « Sonia : Costa a peur, il efface les preuves. Sors-moi d'ici. » / « Sonia monte. Direction la cabane, et vite. »
- O3 GOTO Cite Nord → « Prends une caisse et file a la planque en passant par les quartiers nord. » / « Une berline pile en travers. Embuscade ! »
- O4 KILL Cite Nord → « Les hommes de Costa ont repere Sonia ! Ecarte-les. » / « La voie est libre. Sonia, tassee sur le siege : roule, roule ! »
- O5 GOTO Planque → « Reprends la route vers la cabane, vite ! » / « Sonia est a l'abri. Pour l'instant. »

### M14 — OBJS_M14 « La cabane » (défense Sonia)
- O1 GOTO Planque → « Sonia : ils nous ont trouves ! Reviens a la cabane, vite ! » / « Les hommes de Costa encerclent la cabane. »
- O2 BEAT Planque → « Defends Sonia : premiere vague ! » / « Ils refluent... non, d'autres arrivent ! »
- O3 BEAT Planque → « Des tireurs, cette fois. Tiens bon ! » / « Sonia : je sais ou il garde les dossiers. Ses bureaux. »

### M15 — OBJS_M15 « Les papiers »
- O1 GOTO Les Bureaux → « Les bureaux de Costa. Les preuves sont dedans. Entre. » / « Des gardes patrouillent le hall. »
- O2 KILL Les Bureaux → « Neutralise les gardes du hall. » / « Hall degage. Les dossiers sont dans le coffre. »
- O3 GOTO Les Bureaux → « Rafle les papiers de Costa. » / « Tu rafles les dossiers. Maintenant, sors. »
- O4 BEAT Les Bureaux → « Des renforts te coupent la sortie. Force le passage. » / « Sonia : 'Costa a commande le meurtre de Marius.' On le tient. »

### M16 — OBJS_M16 « Le sabotage »
- O1 ENTER_CAR Cite Nord → « Jeannot : on va lui faire mal au portefeuille. Vole sa caisse de luxe, aux quartiers nord. »
- O2 CRUSH La Casse → « Amene-la a la casse. Descends pres de la grue et fais-la BROYER. » / « Une de moins. Jeannot : ca pique, hein Costa ? »
- O3 ENTER_CAR Le Casino → « Encore une, garee devant le Casino. »
- O4 CRUSH La Casse → « Rebelote : au broyeur, et reste pres de la grue jusqu'au bout. » / « Jeannot : ca va le rendre chevre. »

### M17 — OBJS_M17 « Paulo » (boss, PV=5)
- O1 GOTO La Casse → « Jeannot : Paulo, le bras droit de Costa, surveille la casse. Descends-le. » / « Paulo et ses hommes t'attendent. »
- O2 KILL La Casse → « Ses gardes d'abord. » / « Reste Paulo. Il lachera pas facilement. »
- O3 KILL La Casse → « Paulo encaisse. Le lache pas. » / « Paulo, mourant : le patron... il t'attend au Vieux-Port. »

### M18 — OBJS_M18 « Le Vieux-Port » (boss final Costa, PV=7) — POI repointé
- O1 GOTO **Vieux-Port** → « Costa : je t'attends au Vieux-Port, minot. Viens donc. » / « Costa : t'es alle trop loin. Messieurs, occupez-vous de lui. »
- O2 KILL Vieux-Port → « Ses gardes du corps t'attaquent ! Ouvre-toi un chemin jusqu'a Costa. » / « La voie est libre. Reste Costa. »
- O3 KILL Vieux-Port → « Costa est coriace et bien protege. Acharne-toi. » / « Tout ca... pour un ami. C'est fini, Costa. »

## 5. Réécritures — missions secondaires (`game_state.h`)

Renommages POI `Chinatown`→`Cite Nord` + accent léger ; noms swappés si présents.
- **OBJS_JOE** « Joe »→« Riton » : « Riton se planque aux quais. Retrouve-le et fais-le taire. » / « Beau boulot. Riton parlera plus. »
- **OBJS_VENGEANCE** (POI→Cite Nord) : « Il se cache aux quartiers nord. Fais-lui la peau. » / « Vengeance accomplie. »
- **OBJS_RACE** O3 (POI→Cite Nord) : « Checkpoint 2 : quartiers nord. »
- **OBJS_DEBT** (POI→Cite Nord ×2) : « Un mauvais payeur traine aux quartiers nord. Va le voir. » / « Le voila. » ; « Fais-lui cracher la dette : tabasse-le. » / « Il paiera, crois-moi. »
- **OBJS_RACKET** O1 (POI→Cite Nord) : « Tournee de protection. Commerce 1 : quartiers nord. »
- **OBJS_PIZZA** O2 (POI→Cite Nord) : « Livre la pizza avant qu'elle refroidisse ! »
- Autres secondaires (Nettoyage, Temoin genant, Taxi, Cavale, Chauffard, Livreur perdu, Alerte incendie) : passage d'accent léger, aucun nom de perso de trame → simples retouches optionnelles.

## 6. Réécritures — `gta/mod_mission.h`

**bossTauntLines** (clés mises à jour) :
- `Le Sanglier` → « Jo le Sanglier : Jeannot t'envoie crever a ma place, minot ? » / « Le Sanglier : approche, que je t'apprenne le respect. »
- `Paulo` → « Paulo : t'es alle trop loin. Costa veut ta tete. » / « Paulo : et c'est moi qui vais la lui porter. »
- `Le Vieux-Port` → « Costa : t'as du cran de venir jusqu'ici, minot. » / « Costa : Marius allait me balancer. J'avais pas le choix. »

**ambushTauntLines** (clés mises à jour) :
- `Les protections` → « Le type : Marius t'envoie au charbon, hein minot ? Mauvaise pioche. » / « Marius : c'est un piege ! Sors les poings, vite ! »
- `Le cousin Remi` → « Marius : tu sais pourquoi on est la, Remi. Paie tes dettes. » / « Remi : ...emboucanez-le, les collegues ! »
- `Les Nordistes` → « Un Nordiste, cacou : t'es perdu, collegue ? C'est pas ton quartier. » / « Un autre : ve, regarde-le... il va comprendre. Chopez-le ! »
- `L'entrepot` → « Un garde : personne entre ici. Fais demi-tour, tant que tu peux. » / « L'autre arme son flingue. Trop tard pour causer, ve. »
- `La cabane` → « Un homme de Costa : la comptable est la ! On la veut vivante. » / « Sonia : ils sont partout ! Les laisse pas approcher ! »
- `Les papiers` → « Un garde : t'as rien a faire dans les bureaux de M. Costa. » / « Il degaine. Les autres rappliquent. »

**deliveryLines** (clés mises à jour) :
- `Le fourgon` → « Le receleur : le fourgon des Nordistes... beau bebe. Jeannot sera content. » / « Le receleur : file, minot. Je m'occupe de lui. »
- `Les protections` → « Marius : tu connais la chanson. La protection. » / « Le commercant : ...tiens. C'est tout ce que j'ai, peuchere. »
- défaut (M1) → « Marius : tiens, le paquet. C'est tout bon. » / « Le docker : t'es en retard, peuchere. Filez avant les flics. »

**Cutscene départ Marius** (l.636-638) :
- `Le colis du port` → « Marius : bon boulot pour un premier jour. Repose-toi, minot. »
- défaut → « Marius : bon boulot, minot. On remet ca bientot. »

**Cutscene mort Marius** (l.750/757/767/775) :
- « Marius : t'es l'acheteur ? Bon, montre le fric, on echange. »
- « L'acheteur degaine. Desole, Marius. Rien de personnel. »
- « Marius s'effondre. » *(pas de nom de Costa ici — cf. choix de fidélité)*
- « Le tueur file ! Rattrape-le, ve ! »

**Fail messages** (l.872/879) :
- défense alliée : « Sonia est morte ! Mission ratee. » / « Jeannot est mort ! Mission ratee. »
- client tué (`La tournee de Marius`) : « Jeannot : t'as tue notre client, fada ! Mission ratee. »
- autre fail tueur (l.833) : « Le tueur s'est echappe. »

## 7. Réécritures — `gta/gta.ino` épilogue (l.309-310, 314)

- « Le port, au petit matin. Tout le monde sait, pour Costa. Pour Marius. »
- « Le telephone sonne. Tu decroches. Un silence... puis : 'Allo minot ?' *clic* FIN. »
- muet → « Le telephone reste muet. »

## 8. Carte & musique

- `citymap_data.cpp:173` : `"La Rose"` → `"Cite Nord"`.
- `music_data.cpp:70` : `"Chinatown"` → `"Cite Nord"`.
- Vérifier `city/overlay.json` (libellé `La Rose`) en implémentation.

## 9. Checklist build/test

1. `grep` final : plus aucune occurrence de `Marco|Tony|Victor|Rico|Bruno|Nico|Sarah|Chinatown|Loup|les Loups` dans les **chaînes** (hors identifiants de code type `MARCO_COLOR`, conservés).
2. Vérifier chaque clé `strcmp` renommée ↔ titre `MISSIONS[]` (zéro taunt orphelin).
3. `host_test/` : doivent passer (aucune logique modifiée).
4. Recompiler le firmware ; surveiller la taille flash.
5. Régénérer `GTADEMAKE/GTADEMAKE.bin` (`FOLDER_NAME="GTADEMAKE"`) avant commit.

## Hors-scope

- Identifiants de code internes (`MARCO_COLOR`, `SARAH_COLOR`, `TONY_COLOR`,
  `MISSION_DEAL`, `EV_MARCO_*`) : non visibles à l'écran → conservés tels quels.
- Aucune nouvelle mécanique, aucun nouveau POI, aucun sprite.
