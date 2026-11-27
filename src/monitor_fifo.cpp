#include "monitor_fifo.h"

void MonitorFifoRW::anotar(Tipo tipo, Accion accion, long turno) {
    if (traza_habilitada_) traza.push_back({id_hebra, tipo, accion, turno});
}

void MonitorFifoRW::entrar_lectura() {
    std::unique_lock<std::mutex> lk(m_);
    long mi_turno = siguiente_turno_++;
    anotar(Tipo::Lectora, Accion::Pide, mi_turno);

    cv_.wait(lk, [this, mi_turno] {
        return mi_turno == turno_actual_ && !escritor_activo_;
    });

    ++lectores_activos_;
    ++turno_actual_;
    anotar(Tipo::Lectora, Accion::Entra, mi_turno);
    cv_.notify_all();
}

void MonitorFifoRW::salir_lectura() {
    std::lock_guard<std::mutex> lk(m_);
    --lectores_activos_;
    anotar(Tipo::Lectora, Accion::Sale, -1);
    if (lectores_activos_ == 0) cv_.notify_all();
}

void MonitorFifoRW::entrar_escritura() {
    std::unique_lock<std::mutex> lk(m_);
    long mi_turno = siguiente_turno_++;
    anotar(Tipo::Escritora, Accion::Pide, mi_turno);

    cv_.wait(lk, [this, mi_turno] {
        return mi_turno == turno_actual_ && !escritor_activo_ && lectores_activos_ == 0;
    });

    escritor_activo_ = true;
    ++turno_actual_;
    anotar(Tipo::Escritora, Accion::Entra, mi_turno);
}

void MonitorFifoRW::salir_escritura() {
    std::lock_guard<std::mutex> lk(m_);
    escritor_activo_ = false;
    anotar(Tipo::Escritora, Accion::Sale, -1);
    cv_.notify_all();
}

std::vector<AnotacionTraza> MonitorFifoRW::obtener_traza() {
    std::lock_guard<std::mutex> lk(m_);
    return traza;
}