/*
 * =====================================================================================
 *
 *       Filename: fflerror.hpp
 *        Created: 03/07/2020 16:58:13
 *    Description: 
 *
 *        Version: 1.0
 *       Revision: none
 *       Compiler: gcc
 *
 *         Author: ANHONG
 *          Email: anhonghe@gmail.com
 *   Organization: USTC
 *
 * =====================================================================================
 */

#pragma once
#include <stdexcept>
#include "strfunc.hpp"
#define fflerror(...) std::runtime_error(str_ffl() + ": " + str_printf(__VA_ARGS__))
