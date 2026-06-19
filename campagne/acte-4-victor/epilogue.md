# Épilogue

*ACTE IV — Victor (M16–M18 + épilogue)*

> Script fidèle à l'implémentation (glue téléphone rouge, `gta.ino` ;
> `drawPhones` pour la sonnerie). Pas de mission : une simple séquence de
> décrochage terminal.

- **État** — après M18, `campaignStep == 18` (`== STORY_LEN`).
- **Sonnerie** — `drawPhones` : la cabine rouge sonne tant que
  `campaignStep <= STORY_LEN` **et** qu'aucune mission n'est active. Elle sonne
  donc **une dernière fois** pour l'épilogue.

## Déroulé (décrochage de la cabine rouge)

1. Joueur à portée (`PHONE_REACH`) de la cabine rouge → presser **A**.
2. `campaignStep == STORY_LEN` → branche épilogue (pas de `startMission`) :
   - **Narration** — la **voix de Sarah**, la journaliste sauvée :
     *« Sarah : c'est sorti ce matin. Tout le monde sait, pour Victor. Pour
     Marco. »* puis *« Sarah : t'aurais pu finir comme lui. Prends soin de toi,
     petit. *clic* FIN. »*
   - Son : `playCancel`.
   - `campaignStep` passe à **19** (`> STORY_LEN`).
3. Désormais `campaignStep > STORY_LEN` : la cabine rouge **ne sonne plus** et,
   si on la redécroche, *« Le téléphone reste muet. »*

## Après la fin
- Le monde reste **ouvert** : les **15 cabines bleues** (missions secondaires)
  continuent de sonner et restent rejouables.
- Tous les services (Hôpital, AMU Nation, Pay'n'Spray, La Casse, Casino, Bar,
  Commerces) restent actifs.
