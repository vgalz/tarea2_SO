#ifndef CONTROL_RW_HPP
#define CONTROL_RW_HPP

// interfaz comun para monitor y controlador atomico
class ControlRW {
public:
    virtual ~ControlRW() = default;
    virtual void entrar_lectura() = 0;
    virtual void salir_lectura() = 0;
    virtual void entrar_escritura() = 0;
    virtual void salir_escritura() = 0;
};

enum class Politica { Lectores, Escritores, Fifo };

//permite identificar que tipo de hebra esta realizando
//operacion
enum class Tipo { Lectora, Escritora };
//identifica que tipo de accion esta haciendo la hebra, para
//contronar los turnos en funcion de ello
enum class Accion { Pide, Entra, Sale };

//cada registro de traza

struct AnotacionTraza {
    int hebra; // identificador lógico de la hebra
    Tipo tipo;
    Accion accion;
    long turno; // turno FIFO; -1 en las otras políticas
}; 

// Cada hebra tendrá su propio identificador lógico.
thread_local int id_hebra = -1;




#endif // CONTROL_RW_HPP
