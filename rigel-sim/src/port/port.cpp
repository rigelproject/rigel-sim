#include "port/port.h"
#include <string>
#include <sstream>
#include <iomanip>

//FIXME Use a function naming convention, not a class naming convention
std::string PortName(std::string parent, int id, std::string suffix, int index) {

    std::stringstream port_name;
    port_name << parent << "[" << std::setw(4) << id << "]." << suffix;
    if (index >= 0) {
      port_name << "[" << std::setw(4) << index << "]";
    }
    return port_name.str();

}

