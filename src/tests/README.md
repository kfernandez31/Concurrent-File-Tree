# Testy do zadania 2 (System plików)

Użycie:

Najpierw sklonować repo w folderze z kodem źródłowym,
potem w pliku `CMakeLists.txt` należy zastąpić `add_executable(main main.c)` linijką `include("${CMAKE_CURRENT_SOURCE_DIR}/testy-zad2/CMakeExtension.txt")` (oczywiście przed wysłaniem rozwiązania na moodla trzeba cofnąć tę zmianę).

Po tym wystarczy skompilować (tak jak na moodlu jest opisane) i odpalić: `./main`.

### Dodawanie swoich testów

Patrz [tutaj](https://gitlab.com/mimuw-ipp-2021/testy-duze-zadanie-3)

Dodatkowo:
- fork ma być publiczny (inaczej nie mogę sklonować repo i zobaczyć czy mi testy działają),
- na początku pliku .c ma być komentarz wyjaśniający test.
