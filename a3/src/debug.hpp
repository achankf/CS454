#ifndef _debug_hpp_
#define _debug_hpp_

#include "postman.hpp"

class Function;

std::string format_arg(int arg_type);
void debug_print_type(const Postman::Request &req);
void print_function(Function &func);
std::string to_ipv4_string(int bin);

#endif
