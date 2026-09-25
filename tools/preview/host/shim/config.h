#pragma once
/* config.h in the MobiFlight core; the preview only needs the one predicate, which gates
   sending button events while the board is still starting up. */
inline bool getBoardReady() { return true; }
