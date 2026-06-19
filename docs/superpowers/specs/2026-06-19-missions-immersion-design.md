# Design — Immersion des missions (passe d'enrichissement)

> Objectif : étoffer les missions de la trame avec **plus de script et de
> logique « réaliste »**, pour que jouer une mission ne soit plus « aller à un
> point A et revenir », mais vivre des **scènes**. Document de design (aucun
> code à ce stade) ; on traite les missions **une par une, dans l'ordre**.
>
> Référence de l'état actuel (fidèle au code) : `campagne/` et ses fiches
> `m*.md`. Ce document décrit les **améliorations** à apporter par-dessus.

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

## M3 → M18 + épilogue — *à dérouler*

Sections ajoutées au fur et à mesure, mission par mission, dans l'ordre.
