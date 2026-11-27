#pragma once
#include "control_rw.h"
#include <condition_variable>
#include <mutex>
#include <vector>

class MonitorFifoRW final : public MonitorConTraza {
public:
    explicit MonitorFifoRW(bool traza_habilitada = true)
        : traza_habilitada_(traza_habilitada) {}

    void entrar_lectura() override;
    void salir_lectura() override;
    void entrar_escritura() override;
    void salir_escritura() override;
    std::vector<AnotacionTraza> obtener_traza() override;

private:
    std::mutex m_;
    std::condition_variable cv_;
    long siguiente_turno_ = 0;
    long turno_actual_ = 0;
    int lectores_activos_ = 0;
    bool escritor_activo_ = false;
    bool traza_habilitada_;
    std::vector<AnotacionTraza> traza;

    void anotar(Tipo tipo, Accion accion, long turno);
};