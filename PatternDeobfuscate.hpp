#pragma once
#include <hexrays.hpp>

struct ObfCompilerOptimizer : public optinsn_t
{
#if IDA_SDK_VERSION >= 750    
	int func(mblock_t *blk, minsn_t *ins, int optflags);
#else
	int func(mblock_t *blk, minsn_t *ins);
#endif
};
