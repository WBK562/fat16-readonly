# fat16-readonly
Prosty kod symulujący działanie FAT16 na określonym pliku.
Projekt implementuje parser systemu plików FAT16 w trybie tylko do odczytu. Obsługiwane są funkcje inspirowane standardem POSIX, takie jak `file_open`, `file_read`, `file_seek`, `file_close`, `dir_open`, `dir_read` i `dir_close`.

## Możliwości

Kod pozwala na:
1. Otwieranie, czytanie i zamykanie urządzenia blokowego (w formie pliku).
2. Otwieranie i zamykanie woluminu FAT16.
3. Otwieranie, przeszukiwanie, czytanie oraz zamykanie plików w systemie FAT.
4. Otwieranie, czytanie i zamykanie katalogów.

## Uruchomienie

Kod można uruchomić w dowolnym IDE wspierającym język C. Potrzebny jest określony plik, który w swojej zawartości symuluje strukture FAT (jeden jest już dołączony wystarczy pobrać repozytorium).
W funkcji main() zawarty jest już przykładowy kod, który testuje wszystkie funkcje.
