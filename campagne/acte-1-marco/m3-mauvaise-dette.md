# M3 — Mauvaise dette

*ACTE I — Marco (M1–M4) · tutoriel déguisé*

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
