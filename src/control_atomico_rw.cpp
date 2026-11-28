#include "control_atomico_rw.h"
#include <thread>

namespace {

    struct RegistroLocalHebra {
        ControlAtomicoRW* dueño = nullptr;
        EstadisticasAtomicas stats{};

        ~RegistroLocalHebra() {
            if (dueño != nullptr) {
                dueño->acumular_de_hebra(stats);
            }
        }
    };
    EstadisticasAtomicas& obtener_stats_hebra(ControlAtomicoRW* instancia) {
        //"thread_local" hace que cada hilo tenga su copia independiente en su memoria privada
        thread_local RegistroLocalHebra registro;

        /*una medida de seguridad por si en un mismo programa se crearan
         dos objetos ControlAtomicoRW distintos uno tras otro en el mismo hilo. 
        Si detecta que cambió el dueño, vacía los datos anteriores y reinicia los 
        contadores a cero (EstadisticasAtomicas{}) antes de enlazar el nuevo dueño.*/
        if (registro.dueño != instancia) {
            if (registro.dueño != nullptr) {
                registro.dueño->acumular_de_hebra(registro.stats);
                registro.stats = EstadisticasAtomicas{};
            }
            registro.dueño = instancia;
        }
        return registro.stats;
    }

}
void ControlAtomicoRW::entrar_lectura() {
    auto& stats = obtener_stats_hebra(this);
    std::uint64_t esperado = estado_.load(); // una "foto" de los 64 bits de estado_

    while (true) {
        bool esc_activo = (esperado & BIT_ESC_ACT) != 0;//si el bit 32 esta encendido, es decir, hay un escritor activo
        std::uint64_t esc_esperando = (esperado & MASK_ESC_ESP) >> SHIFT_ESC_ESP;

        // Si se cumple más de una causa, contar como recurso ocupado
        if (esc_activo) { 
            ++stats.lec_fallo_ocupado;
            std::this_thread::yield();
            esperado = estado_.load();
            continue;
        }
        if (esc_esperando > 0) {
            ++stats.lec_fallo_esc_esperando;
            std::this_thread::yield();
            esperado = estado_.load();
            continue;
        }

        // Punto de decisión atómico: admite lectora solo si sigue sin escritoras activas ni esperando
        std::uint64_t nuevo = esperado + UNO_LEC_ACT;
        if (estado_.compare_exchange_weak(esperado, nuevo)) {
            return;
        }

        // Fallo de CAS (otra hebra modificó la palabra o fallo spurious)
        ++stats.lec_fallo_cas;
    }
}

void ControlAtomicoRW::salir_lectura() {
    estado_.fetch_sub(UNO_LEC_ACT);
}

void ControlAtomicoRW::entrar_escritura() {
    auto& stats = obtener_stats_hebra(this);

    // Registrarse como esperando antes de reintentar (bloquea nuevas lectoras)
    std::uint64_t esperado = estado_.fetch_add(UNO_ESC_ESP) + UNO_ESC_ESP;

    // Esperar hasta que no haya lectoras activas ni escritora activa
    while (true) {
        std::uint64_t lec_activos = esperado & MASK_LEC_ACT;
        bool esc_activo = (esperado & BIT_ESC_ACT) != 0;

        if (lec_activos > 0 || esc_activo) {
            ++stats.esc_fallo_ocupado;
            std::this_thread::yield();
            esperado = estado_.load();
            continue;
        }

        // Punto de decisión atómico: descontar 1 de esperando y marcar activa encendiendo el bit 32 en el mismo CAS
        std::uint64_t nuevo = (esperado - UNO_ESC_ESP) | BIT_ESC_ACT;
        if (estado_.compare_exchange_weak(esperado, nuevo)) {
            return;
        }

        // Fallo de CAS
        ++stats.esc_fallo_cas;
    }
}

void ControlAtomicoRW::salir_escritura() {
    //el bit 32 ahora pasa a 0, indicando que ya no hay un escritor activo
    estado_.fetch_sub(BIT_ESC_ACT);
}

void ControlAtomicoRW::acumular_de_hebra(const EstadisticasAtomicas& local) {
    total_lec_ocupado_.fetch_add(local.lec_fallo_ocupado);
    total_lec_esc_esperando_.fetch_add(local.lec_fallo_esc_esperando);
    total_lec_cas_.fetch_add(local.lec_fallo_cas);
    total_esc_ocupado_.fetch_add(local.esc_fallo_ocupado);
    total_esc_cas_.fetch_add(local.esc_fallo_cas);
}
EstadisticasAtomicas ControlAtomicoRW::obtener_estadisticas() const {
    EstadisticasAtomicas res;
    res.lec_fallo_ocupado = total_lec_ocupado_.load();
    res.lec_fallo_esc_esperando = total_lec_esc_esperando_.load();
    res.lec_fallo_cas = total_lec_cas_.load();
    res.esc_fallo_ocupado = total_esc_ocupado_.load();
    res.esc_fallo_cas = total_esc_cas_.load();
    return res;
}