#include "monitor_lectores.h"

void MonitorLectoresRW::anotar(Tipo tipo, Accion accion) {
    if (traza_habilitada_) traza.push_back({id_hebra, tipo, accion, -1});
}

void MonitorLectoresRW::entrar_lectura() {
    std::unique_lock<std::mutex> lk(m_);
    anotar(Tipo::Lectora, Accion::Pide);
    if (escritor_activo_) {
        ++lectores_esperando_;
        cv_.wait(lk, [this] { return !escritor_activo_; });
        --lectores_esperando_;
    }
    ++lectores_activos_;
    anotar(Tipo::Lectora, Accion::Entra);
}

void MonitorLectoresRW::salir_lectura() {
    std::lock_guard<std::mutex> lk(m_);
    --lectores_activos_;
    anotar(Tipo::Lectora, Accion::Sale);
    if (lectores_activos_ == 0) cv_.notify_all();
}

void MonitorLectoresRW::entrar_escritura() {
    std::unique_lock<std::mutex> lk(m_);
    anotar(Tipo::Escritora, Accion::Pide);
    cv_.wait(lk, [this] {
        return !escritor_activo_ && lectores_activos_ == 0 && lectores_esperando_ == 0;
    });
    escritor_activo_ = true;
    anotar(Tipo::Escritora, Accion::Entra);
}

void MonitorLectoresRW::salir_escritura() {
    std::lock_guard<std::mutex> lk(m_);
    escritor_activo_ = false;
    anotar(Tipo::Escritora, Accion::Sale);
    cv_.notify_all();
}

std::vector<AnotacionTraza> MonitorLectoresRW::obtener_traza() {
    std::lock_guard<std::mutex> lk(m_);
    return traza;
}
