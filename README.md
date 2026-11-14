## Requisitos

Un compilador compatible con C++17 (por ejemplo `g++`) y soporte de `std::thread`.

## Compilación

```sh
make
```

## Uso

```sh
./rw <modo> <politica> <L> <E> <K> <dur_lec_us> <dur_esc_us> [notraza]
```

Ejemplos:

```sh
./rw cond lectores 3 2 5 1000 2000
./rw cond lectores 2 2 3 0 0 notraza
```