#pragma once
#include "tipos.h"
#include <string>
#include <vector>

struct ResultadoVerificacion {
    bool politica_ok = true;
    long evidencia = 0;
    std::size_t indice_error = 0;
    std::string mensaje_error;
};

ResultadoVerificacion verificar_traza(const std::vector<AnotacionTraza>& traza,
                                      Politica politica);