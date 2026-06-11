# M1 — Premier jour

*ACTE I — Marco (M1–M4) · tutoriel déguisé*

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
