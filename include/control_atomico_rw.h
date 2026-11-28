#pragma once
#include "control_rw.h"
#include <atomic>
#include <cstdint>

struct EstadisticasAtomicas{
    std::uint64_t lec_fallo_ocupado = 0;    //Escritora activa
    std::uint64_t lec_fallo_esc_esperando = 0; //Escritora esperando, evita que entren mas lectores dandole prioridad a la escritora
    std::uint64_t lec_fallo_cas = 0;    // Fallo de compare_exchange (contención o espurio)

    std::uint64_t esc_fallo_ocupado = 0;     // Escritora activa o lectoras activas
    std::uint64_t esc_fallo_cas = 0;     // Fallo de compare_exchange (contención o espurio)
};

class ControlAtomicoRW : public ControlRW{
    public: 
    static_assert(std::atomic<uint64_t>::is_always_lock_free,
    "std::atomic<std::uint64_t> debe ser lock-free"); //Esto fue pedido en R9

    ControlAtomicoRW() = default;
    ~ControlAtomicoRW() override = default;

    void entrar_lectura() override;
    void salir_lectura() override;
    void entrar_escritura() override;
    void salir_escritura() override;

    // Llamar en main() después de hacer join() a todas las hebras
    EstadisticasAtomicas obtener_estadisticas() const;

    // Método interno invocado automáticamente al destruir el estado thread_local de cada hebra
    void acumular_de_hebra(const EstadisticasAtomicas& local);

    private:
    // Formato de la palabra de 64 bits:
    // Bits [0..15]  : lectores_activos     (16 bits, máx 65.535 >= 1000)
    // Bits [16..31] : escritores_esperando (16 bits, máx 65.535 >= 1000)
    // Bit  [32]     : escritor_activo      (1 bit)
    // Bits [33..63] : sin uso (0)
    static constexpr std::uint64_t MASK_LEC_ACT  = 0xFFFFULL; //máscara de 16 bits encendidos en la parte derecha
    static constexpr std::uint64_t SHIFT_ESC_ESP = 16; //Indica cuántas posiciones hacia la izquierda empieza la sección de escritoras esperando
    static constexpr std::uint64_t MASK_ESC_ESP  = (0xFFFFULL << SHIFT_ESC_ESP); //mueve la mascara anterior a la seccion del contador de escritoras
    static constexpr std::uint64_t BIT_ESC_ACT   = (1ULL << 32); //Pone un 1 en la posicion 32 del numero binario, actua como una bandera booleana para indicar si hay un escritor activo

    static constexpr std::uint64_t UNO_LEC_ACT   = 1ULL; // representa el valor 1 en el campo de lectoras activas (sumar UNO_LEC_ACT equivale a lectores_activos + 1)
    static constexpr std::uint64_t UNO_ESC_ESP   = (1ULL << SHIFT_ESC_ESP); // lo mismo pero para el sector de escritoras si se suma a estado_

    /// @brief Es una única variable compartida entre todos los hilos, inicializada en 0 (0 lectoras activas, 0 escritoras esperando, 0 escritoras activas). Toda decisión de entrada o salida se hace leyendo o modificando esta única caja mediante operaciones atómicas y CAS.
    std::atomic<std::uint64_t> estado_{0}; //

    // Totales globales consolidados tras el término de cada hebra
    // cada uno para contar los respectivos fallos
    std::atomic<std::uint64_t> total_lec_ocupado_{0};
    std::atomic<std::uint64_t> total_lec_esc_esperando_{0};
    std::atomic<std::uint64_t> total_lec_cas_{0};
    std::atomic<std::uint64_t> total_esc_ocupado_{0};
    std::atomic<std::uint64_t> total_esc_cas_{0};
};