#include "ControlRW.hpp"

#include <charconv>
#include <chrono>
#include <climits>
#include <exception>
#include <iostream>
#include <string_view>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>

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

struct Registro {
    long version = 0;
    long doble_version = 0;
};

struct Diagnostico {
    std::atomic<long> lectoras_dentro{0};
    std::atomic<long> escritoras_dentro{0};
    std::atomic<long> max_lectoras{0};
    std::atomic<long> conflictos{0};
    std::atomic<long> lecturas_incoherentes{0};
};


//variables locales a cada hebra (thread_local hace que cada 
//hebra tenga su propia copia de la variable)

thread_local int id_hebra = -1;
//guarda el turno que recibe una hebra en entrar_escritura
//o entrar_lectura
thread_local long turno_hebra = -1;

// monitor principal
class MonitorRW : public ControlRW {
public:
    explicit MonitorRW(Politica p) : politica(p) {}

    void entrar_lectura() override {
        std::unique_lock<std::mutex> lock(m);
        
        //se inicializa el turno en -1 por si la politica no
        //es FIFO
        long mi_turno = -1;

        if (politica == Politica::Fifo) {
            //si la politica es fifo el turno de la hebra
            //cambia al turno al siguiente +1
            mi_turno = siguiente_turno++;
        }

        // se registra la solicitud en la traza antes de 
        //comenzar a esperar
        traza.push_back({
            id_hebra, Tipo::Lectora, Accion::Pide, mi_turno
        });

        ++lectoras_esperando;
        
        puede_entrar.wait(lock, [this, mi_turno] {

            //caso fifo
            if (politica == Politica::Fifo) {
                return mi_turno == turno_actual &&
                       !escritora_activa;
            }

            return !escritora_activa &&
                   (politica == Politica::Lectores ||
                    escritoras_esperando == 0);
        });

        --lectoras_esperando;
        ++lectoras_activas;

        //se registra la entrada en la traza
        traza.push_back({
            id_hebra, Tipo::Lectora, Accion::Entra, mi_turno
        });


        // avanzamos el turno cuando la lectora entra, en lugar
        // de esperar a que salga. Esto permite que otras
        // lectoras con turnos consecutivos tambien puedan entrar
        // mientras la primera sigue leyendo.

        if (politica == Politica::Fifo) {
            // se evalua la evaluacion de la siguiente
            //solicitud 
            ++turno_actual;
            puede_entrar.notify_all();
        }
    }

    void salir_lectura() override {
        std::lock_guard<std::mutex> lock(m);
        --lectoras_activas;
        if (lectoras_activas == 0) {
            puede_entrar.notify_all();
        }
    }

    void entrar_escritura() override {
        std::unique_lock<std::mutex> lock(m);
        ++escritoras_esperando;
        puede_entrar.wait(lock, [this] {
            return !escritora_activa && lectoras_activas == 0 &&
                   (politica == Politica::Escritores || lectoras_esperando == 0);
        });
        --escritoras_esperando;
        escritora_activa = true;
    }

    void salir_escritura() override {
        std::lock_guard<std::mutex> lock(m);
        escritora_activa = false;
        puede_entrar.notify_all();
    }

private:
    Politica politica;
    std::mutex m;
    std::condition_variable puede_entrar;
    long lectoras_activas = 0;
    long lectoras_esperando = 0;
    long escritoras_esperando = 0;
    bool escritora_activa = false;

    //traza de admision
    std::vector<AnotacionTraza> traza;

    //variables de control de FIFO
    long siguiente_turno = 0;
    long turno_actual = 0;
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

void simular_trabajo(long duracion_us) {
    if (duracion_us > 0) {
        std::this_thread::sleep_for(std::chrono::microseconds(duracion_us));
    }
}

void ejecutar_lectora(ControlRW& control, const Registro& registro,
                     Diagnostico& diagnostico, long k, long duracion_us) {
    for (long op = 0; op < k; ++op) {
        control.entrar_lectura();

        const long lectoras = diagnostico.lectoras_dentro.fetch_add(1) + 1;
        if (diagnostico.escritoras_dentro.load() != 0) {
            ++diagnostico.conflictos;
        }

        long maximo = diagnostico.max_lectoras.load();
        while (lectoras > maximo &&
               !diagnostico.max_lectoras.compare_exchange_weak(maximo, lectoras)) {}

        const long version = registro.version;
        const long doble_version = registro.doble_version;
        if (doble_version != 2 * version) {
            ++diagnostico.lecturas_incoherentes;
        }
        simular_trabajo(duracion_us);

        --diagnostico.lectoras_dentro;
        control.salir_lectura();
    }
}

void ejecutar_escritora(ControlRW& control, Registro& registro,
                       Diagnostico& diagnostico, long k, long duracion_us) {
    for (long op = 0; op < k; ++op) {
        control.entrar_escritura();

        if (diagnostico.escritoras_dentro.fetch_add(1) + 1 != 1 ||
            diagnostico.lectoras_dentro.load() != 0) {
            ++diagnostico.conflictos;
        }

        ++registro.version;
        registro.doble_version = 2 * registro.version;
        simular_trabajo(duracion_us);

        --diagnostico.escritoras_dentro;
        control.salir_escritura();
    }
}

int main(int argc, char* argv[]) {
    Configuracion c{};
    if (!leer_configuracion(argc, argv, c)) {
        mostrar_uso(argv[0]);
        return 1;
    }

    if (c.modo == Modo::Atomic) {
        std::cerr << "error: el modo atomic todavia no esta implementado.\n";
        return 1;
    }
    if (c.politica == Politica::Fifo) {
        std::cerr << "error: la politica fifo todavia no esta implementada.\n";
        return 1;
    }

    Registro registro;
    Diagnostico diagnostico;
    MonitorRW control(c.politica);
    std::vector<std::thread> hebras;

    try {
        hebras.reserve(c.L + c.E);
        for (long i = 0; i < c.L; ++i) {
            hebras.emplace_back([&] {
                ejecutar_lectora(control, registro, diagnostico, c.K, c.dur_lec_us);
            });
        }
        for (long i = 0; i < c.E; ++i) {
            hebras.emplace_back([&] {
                ejecutar_escritora(control, registro, diagnostico, c.K, c.dur_esc_us);
            });
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

    const long total_escrituras = c.E * c.K;
    if (registro.version != total_escrituras ||
        registro.doble_version != 2 * total_escrituras ||
        diagnostico.lecturas_incoherentes.load() != 0 ||
        diagnostico.conflictos.load() != 0 ||
        diagnostico.lectoras_dentro.load() != 0 ||
        diagnostico.escritoras_dentro.load() != 0) {
        std::cerr << "error: no se cumplieron las invariantes de R2.\n";
        return 1;
    }

    std::cout << "maximo de lectoras simultaneas: " << diagnostico.max_lectoras.load() << '\n';
    return 0;
}
