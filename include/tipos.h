#pragma once

enum class Tipo { Lectora, Escritora };
enum class Accion { Pide, Entra, Sale };
enum class Politica { Lectores, Escritores, Fifo };

struct AnotacionTraza {
    int hebra;
    Tipo tipo;
    Accion accion;
    long turno;
};

extern thread_local int id_hebra;
