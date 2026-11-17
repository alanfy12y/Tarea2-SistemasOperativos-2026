#include "monitor_escritores.h"

void MonitorEscritoresRW::anotar(Tipo tipo, Accion accion) {
    if (traza_habilitada_) traza.push_back({id_hebra, tipo, accion, -1});
}

void MonitorEscritoresRW::entrar_lectura() {
    std::unique_lock<std::mutex> lk(m_);
    anotar(Tipo::Lectora, Accion::Pide);
    cv_.wait(lk, [this] {
        return !escritor_activo_ && escritores_esperando_ == 0;
    });
    ++lectores_activos_;
    anotar(Tipo::Lectora, Accion::Entra);
}

void MonitorEscritoresRW::salir_lectura() {
    std::lock_guard<std::mutex> lk(m_);
    --lectores_activos_;
    anotar(Tipo::Lectora, Accion::Sale);
    if (lectores_activos_ == 0) cv_.notify_all();
}

void MonitorEscritoresRW::entrar_escritura() {
    std::unique_lock<std::mutex> lk(m_);
    anotar(Tipo::Escritora, Accion::Pide);
    ++escritores_esperando_;
    cv_.wait(lk, [this] {
        return !escritor_activo_ && lectores_activos_ == 0;
    });
    --escritores_esperando_;
    escritor_activo_ = true;
    anotar(Tipo::Escritora, Accion::Entra);
}

void MonitorEscritoresRW::salir_escritura() {
    std::lock_guard<std::mutex> lk(m_);
    escritor_activo_ = false;
    anotar(Tipo::Escritora, Accion::Sale);
    cv_.notify_all();
}

std::vector<AnotacionTraza> MonitorEscritoresRW::obtener_traza() {
    std::lock_guard<std::mutex> lk(m_);
    return traza;
}
