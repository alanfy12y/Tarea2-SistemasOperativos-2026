#pragma once
#include "tipos.h"
#include <vector>

class ControlRW {
public:
    virtual ~ControlRW() = default;
    virtual void entrar_lectura() = 0;
    virtual void salir_lectura() = 0;
    virtual void entrar_escritura() = 0;
    virtual void salir_escritura() = 0;
};

class MonitorConTraza : public ControlRW {
public:
    virtual std::vector<AnotacionTraza> obtener_traza() = 0;
};
