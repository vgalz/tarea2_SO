#include "ControlRW.hpp"

#include <charconv>
#include <chrono>
#include <climits>
#include <exception>
#include <iostream>
#include <string_view>
#include <thread>
#include <vector>

enum class Modo { Cond, Atomic };

struct Configuracion {
    Modo modo;
    Politica politica;
    long L;
    long E;
    long K;
    long dur_lec_us;
    long dur_esc_us;
    bool traza_habilitada;
};

void mostrar_uso(const char* programa) {
    std::cerr << "Uso: " << programa
              << " <modo> <politica> <L> <E> <K>"
              << " <dur_lec_us> <dur_esc_us> [notraza]\n";
}

// conversion estricta que no admite texto adicional, decimales ni desbordamiento.
bool leer_long(std::string_view texto, long& resultado) {
    const auto conversion = std::from_chars(texto.data(), texto.data() + texto.size(), resultado);
    return conversion.ec == std::errc{} && conversion.ptr == texto.data() + texto.size();
}

bool leer_configuracion(int argc, char* argv[], Configuracion& c) {
    if (argc != 8 && argc != 9) {
        std::cerr << "Error: cantidad de argumentos incorrecta.\n";
        return false;
    }

    const std::string_view modo = argv[1];
    if (modo == "cond") {
        c.modo = Modo::Cond;
    } else if (modo == "atomic") {
        c.modo = Modo::Atomic;
    } else {
        std::cerr << "Error: modo invalido.\n";
        return false;
    }

    const std::string_view politica = argv[2];
    if (politica == "lectores") {
        c.politica = Politica::Lectores;
    } else if (politica == "escritores") {
        c.politica = Politica::Escritores;
    } else if (politica == "fifo") {
        c.politica = Politica::Fifo;
    } else {
        std::cerr << "Error: politica invalida.\n";
        return false;
    }

    if (c.modo == Modo::Atomic && c.politica != Politica::Escritores) {
        std::cerr << "Error: modo atomic solo permite politica escritores.\n";
        return false;
    }

    if (!leer_long(argv[3], c.L) || !leer_long(argv[4], c.E) ||
        !leer_long(argv[5], c.K) || !leer_long(argv[6], c.dur_lec_us) ||
        !leer_long(argv[7], c.dur_esc_us)) {
        std::cerr << "Error: parametros numericos invalidos.\n";
        return false;
    }

    if (c.L < 1 || c.E < 1 || c.L > 1000 - c.E) {
        std::cerr << "Error: se requiere L >= 1, E >= 1 y L + E <= 1000.\n";
        return false;
    }

    if (c.K < 1 || c.dur_lec_us < 0 || c.dur_esc_us < 0) {
        std::cerr << "Error: K >= 1 y ambas duraciones >= 0.\n";
        return false;
    }

    // se comprueban los productos sin multiplicar para evitar desbordamientos.
    if (c.K > LONG_MAX / (2 * c.E) || c.K > LONG_MAX / (c.L + c.E)) {
        std::cerr << "Error: 2*E*K o (L+E)*K no cabe en long.\n";
        return false;
    }

    if (argc == 9 && std::string_view(argv[8]) != "notraza") {
        std::cerr << "Error: argumento opcional invalido.\n";
        return false;
    }
    c.traza_habilitada = (argc == 8);
    return true;
}

// R1: cada hebra simula K operaciones sin acceder a un registro compartido.
void ejecutar_hebra(long k, long duracion_us) {
    for (long op = 0; op < k; ++op) {
        if (duracion_us > 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(duracion_us));
        }
    }
}

int main(int argc, char* argv[]) {
    Configuracion c{};
    if (!leer_configuracion(argc, argv, c)) {
        mostrar_uso(argv[0]);
        return 1;
    }

    std::vector<std::thread> hebras;
    try {
        hebras.reserve(c.L + c.E);
        for (long i = 0; i < c.L + c.E; ++i) {
            const long duracion = (i < c.L) ? c.dur_lec_us : c.dur_esc_us;
            hebras.emplace_back(ejecutar_hebra, c.K, duracion);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error al crear hebras: " << e.what() << '\n';
        for (auto& hebra : hebras) {
            hebra.join();
        }
        return 1;
    }

    for (auto& hebra : hebras) {
        hebra.join();
    }
    return 0;
}
