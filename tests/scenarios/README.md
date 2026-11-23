# Adversarial scenario files

The supplied `data/QandT.csv` is clean: no zero-size sides, no crossed books, no `?` aggressors,
no malformed rows. Every adversarial case the rubric asks for must be written here.

Planned scenarios (M3):

- crossed / locked book
- zero-quantity bid or ask (cancel semantics)
- `?` aggressor
- trade before any quote for the symbol
- trade exactly at the band edge, and one tick inside it
- trade that lands exactly on the participation cap
- feed containing only other symbols
- trade during HALT
- 12-character symbol (no NUL terminator)
- same-millisecond quote/trade interleavings
