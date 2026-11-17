#pragma once
#include "control_rw.h"
#include <condition_variable>
#include <mutex>
#include <vector>

class MonitorEscritoresRW final : public MonitorConTraza {
public:
    explicit MonitorEscritoresRW(bool traza_habilitada = true)
        : traza_habilitada_(traza_habilitada) {}

    void entrar_lectura() override;
    void salir_lectura() override;
    void entrar_escritura() override;
    void salir_escritura() override;
    std::vector<AnotacionTraza> obtener_traza() override;

private:
    std::mutex m_;
    std::condition_variable cv_;
    int lectores_activos_ = 0;
    int escritores_esperando_ = 0;
    bool escritor_activo_ = false;
    bool traza_habilitada_;
    std::vector<AnotacionTraza> traza;

    void anotar(Tipo tipo, Accion accion);
};
