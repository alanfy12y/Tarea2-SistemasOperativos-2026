#include <atomic>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <vector>
#include <time.h>

#include "tipos.h"
#include "control_rw.h"


// CONTENIDOS: R1 y R2
// Uso: ./rw <cond|atomic> <lectores|escritores|fifo> L E K dur_lec_us dur_esc_us [notraza]
// Para integrar con otras partes:
//	1. Revisar que los nombres de los headres de los monitores coincidan con los de include.
//	2. ajustar el nombre de la clase y el constructor de cada monitor. (Funcion crear_control())
//	3. final del main(): imprimir politica_ok y el indicador de evidencia según la firma real de verificar_traza en include/verificador.h

// Para la compilacion debe complicar: src/rw.cpp junto con src/tipos.cpp, los .cpp de los monitores, verificador, version atomica, con -Iinclude
// id_hebra se ddefine en tipos.cpp

// Hay una prueba independiente que compila con -DUSAR_FALSO que deja pasar una hebra a la vez, sin depender de monitores. Solo sirve para probar R1 y R2, quitarlo despues.
// Hay que repetir la prueba con ThreadSanitizer usando los monitores reales.

struct Registro {
  long version=0;
  long doble_version=0;
};

static Registro reg;
static std::atomic<int> lectoras_dentro{0};
static std::atomic<int> escritoras_dentro{0};
static std::atomic<int> max_lectoras{0};
static std::atomic<int> violaciones{0};
static std::atomic<int> inconsistencias{0};

//Parte R1 (Leer y validar argumentos)
struct Params {
    std::string modo, pol;
    long L, E, K, dur_lec, dur_esc;
    bool traza = true;
};

static bool leer_long(const char* s, long& out) {
    char* fin;
    errno = 0;
    long v = std::strtol(s, &fin, 10);
    if (errno != 0 || *s == '\0' || *fin != '\0') return false;
    out = v;
    return true;
}

static bool leer_args(int argc, char** argv, Params& p) {
    if (argc != 8 && argc != 9) {
        std::cerr << "Uso: " << argv[0] << " <cond|atomic> <lectores|escritores|fifo> L E K dur_lec_us dur_esc_us [notraza]\n";
        return false;
    }

    p.modo = argv[1];
    p.pol = argv[2];
    if (p.modo != "cond" && p.modo != "atomic") {
        std::cerr << "Error: modo invalido\n"; return false;
    }

    if (p.pol != "lectores" && p.pol != "escritores" && p.pol != "fifo") {
        std::cerr << "Error: politica invalida\n"; return false;
    }

    if (p.modo == "atomic" && p.pol != "escritores") {
        std::cerr << "Error: el modo atomic solo implementa la politica escritores\n";
        return false;
    }

    if (!leer_long(argv[3], p.L) || !leer_long(argv[4], p.E) || !leer_long(argv[5], p.K) ||
        !leer_long(argv[6], p.dur_lec) || !leer_long(argv[7], p.dur_esc)) {
        std::cerr << "Error: L, E, K y las duraciones deben ser enteros\n"; return false;
    }

    if (p.L < 1 || p.E < 1 || p.L + p.E > 1000 || p.K < 1 || p.dur_lec < 0 || p.dur_esc < 0) {
        std::cerr << "Error: valores fuera de rango (L>=1, E>=1, L+E<=1000, K>=1, duraciones>=0)\n";
        return false;
    }

    //(2*E*K) y (L+E)*K, deben caber en un long (se comprueba con divisiones)
    if (p.E > (LONG_MAX / 2) / p.K || (p.L + p.E) > LONG_MAX / p.K) {
        std::cerr << "Error: K demasiado grande (desborda un long)\n"; return false;
    }

    if (argc == 9) {
        if (std::string(argv[8]) != "notraza") {
            std::cerr << "Error: el ultimo parametro solo puede ser 'notraza'\n"; return false;
        }
        p.traza = false;
    }
    return true;
}

//Parte R2 (Hebras lectora y escritora)
static void pausa(long us) {
    if (us > 0) std::this_thread::sleep_for(std::chrono::microseconds(us));
}

static void lectora(ControlRW& c, int id, long K, long dur) {
    id_hebra = id;
    for (long i = 0; i < K; i++) {
        c.entrar_lectura();
        int n = lectoras_dentro.fetch_add(1) + 1;
        if (escritoras_dentro.load() != 0) violaciones++;
        int m = max_lectoras.load();
        while (n > m && !max_lectoras.compare_exchange_weak(m, n)) {}

        pausa(dur);
        long v = reg.version;
        long d = reg.doble_version;
        if (d != 2 * v) inconsistencias++;

        lectoras_dentro.fetch_sub(1);
        c.salir_lectura();
    }
}

static void escritora(ControlRW& c, int id, long K, long dur) {
    id_hebra = id;
    for (long i = 0; i < K; i++) {
        c.entrar_escritura();
        int w = escritoras_dentro.fetch_add(1) + 1;
        if (w != 1 || lectoras_dentro.load() != 0) violaciones++;

        reg.version++;
        pausa(dur);
        reg.doble_version = 2 * reg.version;

        escritoras_dentro.fetch_sub(1);
        c.salir_escritura();
    }
}

#ifdef USAR_FALSO
//Control de prueba:deja pasar de a una hebra. no respeta politicas.
class ControlFalso : public ControlRW {
    std::mutex m;
public:
    void entrar_lectura() override { m.lock(); }
    void salir_lectura() override { m.unlock(); }
    void entrar_escritura() override { m.lock(); }
    void salir_escritura() override { m.unlock(); }
};
#endif

static bool crear_control(const Params& p, std::unique_ptr<ControlRW>& ctl,
                          MonitorConTraza*& mon) {
#ifdef USAR_FALSO
    (void)p; (void)mon;
    ctl.reset(new ControlFalso());
    return true;
#else
    if (p.modo == "cond") {
        MonitorConTraza* m = nullptr;
        //ADAPTAR: nombres de clase y constructor (aquí se asume un bool "traza activa")
        if (p.pol == "lectores")        m = new MonitorLectores(p.traza);
        else if (p.pol == "escritores") m = new MonitorEscritores(p.traza);
        else {
#ifdef HAY_FIFO
            m = new MonitorFifo(p.traza);
#else
            std::cerr << "Error: la politica fifo aun no esta disponible\n";
            return false;
#endif
        }
        ctl.reset(m);
        mon = m;
        return true;
    }
#ifdef HAY_ATOMICO
    ctl.reset(new ControlAtomicoRW());   //ADAPTAR: si el constructor es distinto
    return true;
#else
    std::cerr << "Error: el modo atomic aun no esta disponible\n";
    return false;
#endif
#endif
}

//Parte R1 (main con hebras, tiempos y resumen)
static double cpu_segundos() {
    timespec ts;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

int main(int argc, char** argv) {
    Params p;
    if (!leer_args(argc, argv, p)) return 1;

    std::unique_ptr<ControlRW> ctl;
    MonitorConTraza* mon = nullptr;
    if (!crear_control(p, ctl, mon)) return 1;

    auto t0 = std::chrono::steady_clock::now();
    double c0 = cpu_segundos();

    std::vector<std::thread> hebras;
    bool error = false;
    try {
        for (long i = 0; i < p.L; i++)
            hebras.emplace_back(lectora, std::ref(*ctl), (int)i, p.K, p.dur_lec);
        for (long j = 0; j < p.E; j++)
            hebras.emplace_back(escritora, std::ref(*ctl), (int)(p.L + j), p.K, p.dur_esc);
    } catch (const std::system_error& e) {
        std::cerr << "Error al crear hebras: " << e.what() << "\n";
        error = true;
    }
    for (auto& h : hebras) h.join();

    double cpu = cpu_segundos() - c0;
    double pared = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    if (error) return 1;

    long esperado = p.E * p.K;
    bool ok = reg.version == esperado && reg.doble_version == 2 * esperado &&
              inconsistencias.load() == 0 && violaciones.load() == 0;

    std::cout << "modo=" << p.modo << " politica=" << p.pol << "\n"
              << "L=" << p.L << " E=" << p.E << " K=" << p.K << "\n"
              << "operaciones_lectura=" << p.L * p.K << " operaciones_escritura=" << p.E * p.K << "\n"
              << "version_final=" << reg.version << " (esperado " << esperado << ")\n"
              << "doble_version_final=" << reg.doble_version << " (esperado " << 2 * esperado << ")\n"
              << "inconsistencias=" << inconsistencias.load() << "\n"
              << "violaciones_exclusion=" << violaciones.load() << "\n"
              << "max_lectoras_simultaneas=" << max_lectoras.load() << "\n"
              << "tiempo_pared_s=" << pared << " tiempo_cpu_s=" << cpu << "\n"
              << "correcto=" << (ok ? "SI" : "NO") << "\n";

    if (mon && p.traza) {
#ifdef HAY_VERIFICADOR
        std::vector<AnotacionTraza> traza = mon->obtener_traza();
        //ADAPTAR: ver en include/verificador.h la firma real de verificar_traza
        //y el modo de obtener politica_ok y el indicador de evidencia, e imprimirlos:
        //std::cout << "politica_ok=" << ... << " evidencia=" << ... << "\n";
        (void)traza;
#else
        std::cout << "politica_ok=NO_DISPONIBLE (verificador no integrado)\n";
#endif
    }
    return ok ? 0 : 1;
}
