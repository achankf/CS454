#ifndef _debug_hpp_
#define _debug_hpp_

#include "postman.hpp"

struct Function;
struct Name;

std::string format_arg(int arg_type);
void debug_print_type(const Postman::Request &req);
void print_function(const Function &func);
std::string to_ipv4_string(int bin);
std::string to_format(const Name &name);

#endif
