/*
 * =====================================================================================
 *
 *       Filename: monsterbv.cpp
 *        Created: 03/19/2019 06:43:21
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
#include "coro.hpp"
#include "monster.hpp"
#include "monoserver.hpp"

bvnode_ptr Monster::BvNode_GetMasterUID(bvarg_ref nMasterUID)
{
    return bvtree::lambda_bool([this, nMasterUID]() mutable -> bool
    {
        nMasterUID.assign<uint64_t>(MasterUID());
        return MasterUID();
    });
}

bvnode_ptr Monster::BvNode_FollowMaster()
{
    return bvtree::if_branch
    (
        bvtree::lambda_stage([this](bvarg_ref nStage) mutable
        {
            FollowMaster([nStage]() mutable
            {
                nStage.assign<bvres_t>(BV_SUCCESS);
            },

            [nStage]() mutable
            {
                nStage.assign<bvres_t>(BV_FAILURE);
            });
        }),

        bvtree::op_delay(1000),
        bvtree::op_delay(200)
    );
}

bool Monster::CoroNode_FollowMaster()
{
    coro_variable<bool> done;
    FollowMaster([&done](){ done.assign(true); }, [&done](){ done.assign(false); });

    if(done.wait()){
        CoroNode_Wait(1200);
        return true;
    }

    CoroNode_Wait(200);
    return false;
}

bvnode_ptr Monster::BvNode_LocateUID(bvarg_ref nUID, bvarg_ref stLocation)
{
    return bvtree::lambda_stage([this, nUID, stLocation](bvarg_ref nStage) mutable
    {
        RetrieveLocation(nUID.as<uint64_t>(), [nStage, stLocation](const COLocation &stLoc) mutable
        {
            stLocation.assign<tuple<uint32_t, int, int>>(stLoc.MapID, stLoc.X, stLoc.Y);
            nStage.assign<bvres_t>(BV_SUCCESS);
        },

        [nStage]() mutable
        {
            nStage.assign<bvres_t>(BV_FAILURE);
        });
    });
}

void Monster::CoroNode_Wait(uint64_t ms)
{
    if(ms == 0){
        coro_yield();
        return;
    }

    hres_timer timer;
    while(timer.diff_msec() < ms){
        coro_yield();
    }
}

void Monster::CoroNode_RandomMove()
{
    if(std::rand() % 10 < 2){
        if(RandomTurn()){
            CoroNode_Wait(200);
        }
        else{
            CoroNode_Wait(1000);
        }
    }

    else{
        if(CoroNode_MoveForward()){
            CoroNode_Wait(1200);
        }
        else{
            CoroNode_Wait(200);
        }
    }
}

bvnode_ptr Monster::BvNode_RandomMove()
{
    return bvtree::random
    (
        BvNode_RandomTurn(),
        BvNode_MoveForward(),
        BvNode_MoveForward(),
        BvNode_MoveForward(),
        BvNode_MoveForward(),
        BvNode_MoveForward(),
        BvNode_MoveForward(),
        BvNode_MoveForward()
    );
}

bvnode_ptr Monster::BvNode_RandomTurn()
{
    auto fnUpdate = [this]() -> bvres_t
    {
        static const int nDirV[]
        {
            DIR_UP,
            DIR_UPRIGHT,
            DIR_RIGHT,
            DIR_DOWNRIGHT,
            DIR_DOWN,
            DIR_DOWNLEFT,
            DIR_LEFT,
            DIR_UPLEFT,
        };

        auto nDirCount = (int)(std::extent<decltype(nDirV)>::value);
        auto nDirStart = (int)(std::rand() % nDirCount);

        for(int nIndex = 0; nIndex < nDirCount; ++nIndex){
            auto nDirection = nDirV[(nDirStart + nIndex) % nDirCount];
            if(Direction() != nDirection){
                int nX = -1;
                int nY = -1;
                if(OneStepReach(nDirection, 1, &nX, &nY) == 1){
                    // current direction is possible for next move
                    // report the turn and do motion (by chance) in next update
                    m_Direction = nDirection;
                    DispatchAction(ActionStand(X(), Y(), Direction()));

                    // we won't do ReportStand() for monster
                    // monster's moving is only driven by server currently
                    return BV_SUCCESS;
                }
            }
        }
        return BV_FAILURE;
    };

    return bvtree::if_branch
    (
        bvtree::lambda(fnUpdate),
        bvtree::op_delay(200),
        bvtree::op_abort()
    );
}

bool Monster::CoroNode_MoveForward()
{
    int nextX = -1;
    int nextY = -1;

    if(OneStepReach(Direction(), 1, &nextX, &nextY) != 1){
        return false;
    }

    coro_variable<bool> done;
    RequestMove(nextX, nextY, MoveSpeed(), false, false, [&done](){ done.assign(true); }, [&done](){ done.assign(false); });
    return done.wait();
}

bvnode_ptr Monster::BvNode_MoveForward()
{
    bvarg_ref nStage;

    auto fnReset = [nStage]() mutable
    {
        nStage.assign_void();
    };

    auto fnUpdate = [this, nStage]() mutable -> bvres_t
    {
        if(!nStage.has_value()){
            int nX = -1;
            int nY = -1;

            if(OneStepReach(Direction(), 1, &nX, &nY) == 1){
                RequestMove(nX, nY, MoveSpeed(), false, false, [nStage]() mutable
                {
                    nStage.assign<bvres_t>(BV_SUCCESS);
                },

                [nStage]() mutable
                {
                    nStage.assign<bvres_t>(BV_FAILURE);
                });
                return BV_PENDING;
            }else{
                return BV_FAILURE;
            }
        }

        switch(auto nStatus = nStage.as<bvres_t>()){
            case BV_ABORT:
            case BV_FAILURE:
            case BV_PENDING:
            case BV_SUCCESS:
                {
                    return nStatus;
                }
            default:
                {
                    throw std::runtime_error(str_fflprintf(": Invalid node status: %d", nStatus));
                }
        }
    };

    return bvtree::if_branch
    (
        bvtree::lambda(fnReset, fnUpdate),
        bvtree::op_delay(1000),
        bvtree::op_delay( 200)
    );
}

bvnode_ptr Monster::BvNode_MoveOneStep(bvarg_ref stDstLocation)
{
    return bvtree::lambda_stage([this, stDstLocation](bvarg_ref nStage) mutable
    {
        auto [nMapID, nX, nY] = stDstLocation.as<tuple<uint32_t, int, int>>();
        if(!m_Map->In(nMapID, nX, nY)){
            nStage.assign<bvres_t>(BV_ABORT);
            return;
        }

        MoveOneStep(nX, nY, [nStage]() mutable
        {
            nStage.assign<bvres_t>(BV_SUCCESS);
        },

        [nStage]() mutable
        {
            nStage.assign<bvres_t>(BV_FAILURE);
        });
    });
}

bvnode_ptr Monster::BvNode_HasMaster()
{
    return bvtree::lambda_bool([this]() -> bool
    {
        return MasterUID();
    });
}

bvnode_ptr Monster::BvNode_GetProperTarget(bvarg_ref nTargetUID)
{
    return bvtree::lambda_stage([this, nTargetUID](bvarg_ref nStage) mutable
    {
        GetProperTarget([nTargetUID, nStage](uint64_t nUID) mutable
        {
            if(!nUID){
                nTargetUID.assign<uint64_t>(0);
                nStage.assign<bvres_t>(BV_FAILURE);
                return;
            }

            nTargetUID.assign<uint64_t>(nUID);
            nStage.assign<bvres_t>(BV_SUCCESS);
        });
    });
}

uint64_t Monster::CoroNode_GetProperTarget()
{
    coro_variable<uint64_t> targetUID;
    GetProperTarget([&targetUID](uint64_t uid){ targetUID.assign(uid); });
    return targetUID.wait();
}

bvnode_ptr Monster::BvNode_AttackUID(bvarg_ref nTargetUID, bvarg_ref nDCType)
{
    return bvtree::lambda_stage([this, nTargetUID, nDCType](bvarg_ref nStage)
    {
        AttackUID(nTargetUID.as<uint64_t>(), nDCType.as<int>(), [nStage]() mutable
        {
            nStage.assign<bvres_t>(BV_SUCCESS);
        },

        [nStage]() mutable
        {
            nStage.assign<bvres_t>(BV_FAILURE);
        });
    });
}

bool Monster::CoroNode_TrackAttackUID(uint64_t targetUID)
{
    coro_variable<bool> done;
    TrackAttackUID(targetUID, [&done]{ done.assign(true); }, [&done]{ done.assign(false); });

    if(done.wait()){
        CoroNode_Wait(1200);
        return true;
    }

    CoroNode_Wait(200);
    return false;
}

bvnode_ptr Monster::BvNode_TrackAttackUID(bvarg_ref nTargetUID)
{
    return bvtree::if_branch
    (
        bvtree::lambda_stage([this, nTargetUID](bvarg_ref nStage) mutable
        {
            TrackAttackUID(nTargetUID.as<uint64_t>(), [nStage]() mutable
            {
                nStage.assign<bvres_t>(BV_SUCCESS);
            },

            [nStage]() mutable
            {
                nStage.assign<bvres_t>(BV_FAILURE);
            });
        }),

        bvtree::op_delay(1000),
        bvtree::op_delay(200)
    );
}
