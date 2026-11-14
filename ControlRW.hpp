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

#endif // CONTROL_RW_HPP
