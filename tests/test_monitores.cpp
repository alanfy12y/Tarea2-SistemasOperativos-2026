#include "monitor_lectores.h"
#include "monitor_escritores.h"
#include "monitor_fifo.h"
#include "verificador.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

struct Registro {
    long version = 0;
    long doble_version = 0;
};

template <class Monitor>
bool probar(const char *nombre, Politica politica) {
    constexpr int L = 4, E = 2, K = 30;
    Monitor monitor;
    Registro reg;
    std::atomic<int> lectoras{0}, escritoras{0};
    std::atomic<int> violaciones{0}, inconsistencias{0};
    std::atomic<int> max_lectoras{0};
    std::vector<std::thread> hebras;

    for (int i = 0; i < L; ++i) {
        hebras.emplace_back([&, i] {
            id_hebra = i;
            for (int k = 0; k < K; ++k) {
                monitor.entrar_lectura();
                int n = lectoras.fetch_add(1) + 1;
                int anterior = max_lectoras.load();
                while (n > anterior &&
                       !max_lectoras.compare_exchange_weak(anterior, n)) {}
                if (escritoras.load() != 0) ++violaciones;
                long v = reg.version;
                long d = reg.doble_version;
                if (d != 2 * v) ++inconsistencias;
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                lectoras.fetch_sub(1);
                monitor.salir_lectura();
            }
        });
    }
    for (int i = 0; i < E; ++i) {
        hebras.emplace_back([&, i] {
            id_hebra = L + i;
            for (int k = 0; k < K; ++k) {
                monitor.entrar_escritura();
                if (escritoras.fetch_add(1) != 0) ++violaciones;
                if (lectoras.load() != 0) ++violaciones;
                ++reg.version;
                reg.doble_version = 2 * reg.version;
                std::this_thread::sleep_for(std::chrono::microseconds(150));
                escritoras.fetch_sub(1);
                monitor.salir_escritura();
            }
        });
    }
    for (auto &h : hebras) h.join();

    auto traza = monitor.obtener_traza();
    auto res = verificar_traza(traza, politica);

    if (!res.politica_ok) {
        std::cerr << "[" << nombre << "] Error en anotacion "
                  << res.indice_error << ": " << res.mensaje_error << '\n';
    }

    bool bien = reg.version == E * K && reg.doble_version == 2 * E * K &&
                inconsistencias == 0 && violaciones == 0 &&
                traza.size() == static_cast<std::size_t>(3 * (L + E) * K) &&
                res.politica_ok;

    std::cout << nombre << ": " << (bien ? "OK" : "ERROR")
              << " | version=" << reg.version
              << " | doble=" << reg.doble_version
              << " | inconsistencias=" << inconsistencias
              << " | violaciones=" << violaciones
              << " | max_lectoras=" << max_lectoras
              << " | traza=" << traza.size()
              << " | politica_ok=" << (res.politica_ok ? "SI" : "NO")
              << " | evidencia=" << res.evidencia << '\n';
    return bien;
}

int main() {
    bool a = probar<MonitorLectoresRW>("prioridad lectores", Politica::Lectores);
    bool b = probar<MonitorEscritoresRW>("prioridad escritores", Politica::Escritores);
    bool c = probar<MonitorFifoRW>("politica fifo", Politica::Fifo);
    return (a && b && c) ? 0 : 1;
}
