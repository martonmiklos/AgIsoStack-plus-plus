#pragma once

#include "can_lib_badge.hpp"
#include "can_NAME.hpp"

namespace isobus
{

class ControlFunction
{
public:
    enum class Type
    {
        Internal,
        External,
        Partnered
    };

    ControlFunction(NAME NAMEValue, std::uint8_t addressValue, std::uint8_t CANPort);
    virtual ~ControlFunction();

    std::uint8_t get_address() const;

    bool get_address_valid() const;

    std::uint8_t get_can_port() const;

    NAME get_NAME() const;

    Type get_type() const;
protected:
    NAME controlFunctionNAME;
    const Type controlFunctionType;
    std::uint8_t address;
    std::uint8_t canPortIndex;
};

} // namespace isobus
