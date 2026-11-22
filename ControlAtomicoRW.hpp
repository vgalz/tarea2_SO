#ifndef CONTROL_ATOMICO_RW_HPP
#define CONTROL_ATOMICO_RW_HPP

#include "ControlRW.hpp"

#include <atomic>
#include <cstdint>

//TODO: darle implementacion a los metodos (R9)

class ControlAtomicoRW : public ControlRW {
public:
    void entrar_lectura() override;
    void salir_lectura() override;
    void entrar_escritura() override;
    void salir_escritura() override;

private:
    

};

#endif