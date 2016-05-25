/*
 * =====================================================================================
 *
 *       Filename: memorypn.cpp
 *        Created: 05/24/2016 19:14:52
 *  Last Modified: 05/24/2016 19:17:12
 *
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

#include "memorypn.hpp"
#include "monoserver.hpp"

MemoryPN::MemoryPN()
    : MemoryBlockPN<64, 1024, 4>()
{
    extern MemoryPN *g_MemoryPN;
    if(g_MemoryPN){
        extern MonoServer *g_MonoServer;
        g_MonoServer->AddLog(LOGTYPE_WARNING, "one global memory pool instance please");
        g_MonoServer->Restart();
    }
}
