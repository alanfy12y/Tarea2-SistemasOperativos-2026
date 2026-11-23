#include "verificador.h"

ResultadoVerificacion verificar_traza(const std::vector<AnotacionTraza>& traza,
                                      Politica politica) {
    ResultadoVerificacion res;
    long lec_activas = 0;
    long esc_activas = 0;
    long lec_pendientes = 0;
    long esc_pendientes = 0;
    long ingresos_fifo = 0;

    auto fallar = [&](std::size_t idx, const std::string& msg) {
        if (res.politica_ok) {
            res.politica_ok = false;
            res.indice_error = idx;
            res.mensaje_error = msg;
        }
    };

    for (std::size_t i = 0; i < traza.size(); ++i) {
        const auto& ev = traza[i];

        if (ev.accion == Accion::Pide) {
            if (ev.tipo == Tipo::Lectora) {
                // Evidencia Escritores: lectora pide mientras hay escritoras esperando
                if (politica == Politica::Escritores && esc_pendientes > 0) {
                    ++res.evidencia;
                }
                // Regla Lectores: si no hay escritora activa, la siguiente anotación debe ser su Entra
                if (politica == Politica::Lectores && esc_activas == 0) {
                    if (i + 1 >= traza.size() ||
                        traza[i + 1].hebra != ev.hebra ||
                        traza[i + 1].accion != Accion::Entra) {
                        fallar(i, "Lectores: lectora pidio sin escritora activa pero no entro de inmediato");
                    }
                }
                ++lec_pendientes;
            } else {
                ++esc_pendientes;
            }
        } else if (ev.accion == Accion::Entra) {
            if (ev.tipo == Tipo::Lectora) {
                --lec_pendientes;
                // Regla general: no puede entrar lectora con escritora activa
                if (esc_activas > 0) {
                    fallar(i, "General: lectora entro con escritora activa");
                }
                // Regla Escritores: no puede entrar lectora con escritoras pendientes
                if (politica == Politica::Escritores && esc_pendientes > 0) {
                    fallar(i, "Escritores: lectora entro con escritoras pendientes");
                }
                // Evidencia Lectores: lectora entra con escritoras pendientes
                if (politica == Politica::Lectores && esc_pendientes > 0) {
                    ++res.evidencia;
                }
                // Evidencia FIFO: lectora entra mientras otra lectora esta activa
                if (politica == Politica::Fifo && lec_activas > 0) {
                    ++res.evidencia;
                }
                ++lec_activas;
            } else {
                --esc_pendientes;
                // Regla general: acceso exclusivo de escritora
                if (lec_activas > 0 || esc_activas > 0) {
                    fallar(i, "General: escritora entro sin recurso libre");
                }
                // Regla Lectores: escritora no entra si hay lectoras pendientes
                if (politica == Politica::Lectores && lec_pendientes > 0) {
                    fallar(i, "Lectores: escritora entro con lectoras pendientes");
                }
                ++esc_activas;
            }

            // Regla FIFO: el k-esimo Entra debe corresponder al turno k
            if (politica == Politica::Fifo) {
                if (ev.turno != ingresos_fifo) {
                    fallar(i, "FIFO: orden de ingreso distinto al turno asignado");
                }
                ++ingresos_fifo;
            }
        } else if (ev.accion == Accion::Sale) {
            if (ev.tipo == Tipo::Lectora) {
                --lec_activas;
            } else {
                --esc_activas;
            }
        }
    }

    return res;
}