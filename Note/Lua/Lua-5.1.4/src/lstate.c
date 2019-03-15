/*
** $Id: lstate.c,v 2.36.1.2 2008/01/03 15:20:39 roberto Exp $
** Global State
** See Copyright Notice in lua.h
*/


#include <stddef.h>

#define lstate_c
#define LUA_CORE

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "llex.h"
#include "lmem.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"

//state尺寸
#define state_size(x)	(sizeof(x) + LUAI_EXTRASPACE)
//获取state的指针
#define fromstate(l)	(cast(lu_byte *, (l)) - LUAI_EXTRASPACE)
//指针转state
#define tostate(l)   (cast(lua_State *, cast(lu_byte *, l) + LUAI_EXTRASPACE))


/**
 * Main thread combines a thread state and the global state
 * 这里，主线程必须定义在结构的前面，否则关闭虚拟机的时候(如下代码)就无法正确的释放内存。
 */
typedef struct LG {
  lua_State l;
  global_State g;
} LG;
  

/**
 * 初始化堆栈
 */
static void stack_init (lua_State *L1, lua_State *L) {
  /* initialize CallInfo array */
  // 创建CallInfo数组
  L1->base_ci = luaM_newvector(L, BASIC_CI_SIZE, CallInfo);
  //设置当前初始栈
  L1->ci = L1->base_ci;
  //设置当前栈大小
  L1->size_ci = BASIC_CI_SIZE;
  //设置栈末尾
  L1->end_ci = L1->base_ci + L1->size_ci - 1;
  /* initialize stack array */
  // 创建TValue数组
  L1->stack = luaM_newvector(L, BASIC_STACK_SIZE + EXTRA_STACK, TValue);
  //设置栈大小
  L1->stacksize = BASIC_STACK_SIZE + EXTRA_STACK;
  L1->top = L1->stack;
  L1->stack_last = L1->stack+(L1->stacksize - EXTRA_STACK)-1;
  /* initialize first ci */
  // 没看懂这个下面几句什么意思
  L1->ci->func = L1->top;
  // 这里的作用是把当前top所在区域的值set成nil,然后top++
  // 在top++之前,从上一句代码可以看出,top指向的位置是L1->ci->func,也就是把L1->ci->func set为nil
  setnilvalue(L1->top++);  /* `function' entry for this `ci' */
  // 执行这句调用之后, base = top = stack + 1, 但是base是存放什么值的呢??
  L1->base = L1->ci->base = L1->top;
  // 这里的意思是,每个lua函数最开始预留LUA_MINSTACK个栈位置,不够的时候再增加,见luaD_checkstack函数
  L1->ci->top = L1->top + LUA_MINSTACK;
}

/**
 * 释放栈上的调用信息Callinfo以及值TValue
 */
static void freestack (lua_State *L, lua_State *L1) {
  luaM_freearray(L, L1->base_ci, L1->size_ci, CallInfo);
  luaM_freearray(L, L1->stack, L1->stacksize, TValue);
}


/*
** open parts that may cause memory-allocation errors
*/
static void f_luaopen (lua_State *L, void *ud) {
  global_State *g = G(L);
  UNUSED(ud);
  // 初始化堆栈
  stack_init(L, L);  /* init stack */
  // 初始化全局表
  sethvalue(L, gt(L), luaH_new(L, 0, 2));  /* table of globals */
  // 初始化寄存器
  sethvalue(L, registry(L), luaH_new(L, 0, 2));  /* registry */
  //初始化stringtable的大小
  luaS_resize(L, MINSTRTABSIZE);  /* initial size of string table */
  //初始化元方法名，保证不会被回收
  luaT_init(L);
  //初始化lua关键字，保证不会被回收
  luaX_init(L);
  // 初始化not enough memory这个字符串并且标记为不可回收（否则容量不足时无法申请此字符串将会出现无法查错的情况）
  luaS_fix(luaS_newliteral(L, MEMERRMSG));
  g->GCthreshold = 4*g->totalbytes;
}

/**
 * 初始化luastate中的变量，设置全局状态机
 */
static void preinit_state (lua_State *L, global_State *g) {
  G(L) = g;
  L->stack = NULL;
  L->stacksize = 0;
  L->errorJmp = NULL;
  L->hook = NULL;
  L->hookmask = 0;
  L->basehookcount = 0;
  L->allowhook = 1;
  resethookcount(L);
  L->openupval = NULL;
  L->size_ci = 0;
  L->nCcalls = L->baseCcalls = 0;
  L->status = 0;
  L->base_ci = L->ci = NULL;
  L->savedpc = NULL;
  L->errfunc = 0;
  //将全局表的类型设为空
  setnilvalue(gt(L));
}

/**
 * 关闭状态机
 */
static void close_state (lua_State *L) {
  global_State *g = G(L);
  //关闭该状态机的所有upvalue
  luaF_close(L, L->stack);  /* close all upvalues for this thread */
  //清理所有GC对象
  luaC_freeall(L);  /* collect all objects */
  lua_assert(g->rootgc == obj2gco(L));
  lua_assert(g->strt.nuse == 0);
  //释放字符串
  luaM_freearray(L, G(L)->strt.hash, G(L)->strt.size, TString *);
  //释放stringbuffer
  luaZ_freebuffer(L, &g->buff);
  //释放栈
  freestack(L, L);
  lua_assert(g->totalbytes == sizeof(LG));
  //释放状态机
  (*g->frealloc)(g->ud, fromstate(L), state_size(LG), 0);
}

/**
 * 创建新的状态机
 */
lua_State *luaE_newthread (lua_State *L) {
  lua_State *L1 = tostate(luaM_malloc(L, state_size(lua_State)));
  luaC_link(L, obj2gco(L1), LUA_TTHREAD);
  //初始化状态机内部变量
  preinit_state(L1, G(L));
  //初始化堆栈
  stack_init(L1, L);  /* init stack */
  //共享全局表
  setobj2n(L, gt(L1), gt(L));  /* share table of globals */
  //TODO:Hook相关
  L1->hookmask = L->hookmask;
  L1->basehookcount = L->basehookcount;
  L1->hook = L->hook;
  resethookcount(L1);
  lua_assert(iswhite(obj2gco(L1)));
  return L1;
}

/**
 * 释放状态机
 */
void luaE_freethread (lua_State *L, lua_State *L1) {
  luaF_close(L1, L1->stack);  /* close all upvalues for this thread */
  lua_assert(L1->openupval == NULL);
  luai_userstatefree(L1);
  freestack(L, L1);
  luaM_freemem(L, fromstate(L1), state_size(lua_State));
}

/**
 * 新建全局状态机
 */
LUA_API lua_State *lua_newstate (lua_Alloc f, void *ud) {
  int i;
  lua_State *L;
  global_State *g;
  //申请LuaState和GlobalState的内存
  void *l = (*f)(ud, NULL, 0, state_size(LG));
  if (l == NULL) return NULL;
  //转成luastate
  L = tostate(l);
  //地址可以转为LuaState和GlobalState，获取其中的GlobalState
  g = &((LG *)L)->g;
  L->next = NULL;
  L->tt = LUA_TTHREAD;
  g->currentwhite = bit2mask(WHITE0BIT, FIXEDBIT);
  L->marked = luaC_white(g);
  set2bits(L->marked, FIXEDBIT, SFIXEDBIT);
  //初始化luastate中的变量，设置全局状态机
  preinit_state(L, g);
  g->frealloc = f;
  g->ud = ud;
  g->mainthread = L;
  g->uvhead.u.l.prev = &g->uvhead;
  g->uvhead.u.l.next = &g->uvhead;
  g->GCthreshold = 0;  /* mark it as unfinished state */
  g->strt.size = 0;
  g->strt.nuse = 0;
  g->strt.hash = NULL;
  //注册表类型设置类空
  setnilvalue(registry(L));
  //初始化字符串缓冲buffer
  luaZ_initbuffer(L, &g->buff);
  g->panic = NULL;
  g->gcstate = GCSpause;
  g->rootgc = obj2gco(L);
  g->sweepstrgc = 0;
  g->sweepgc = &g->rootgc; 
  g->gray = NULL;
  g->grayagain = NULL;
  g->weak = NULL;
  g->tmudata = NULL;
  g->totalbytes = sizeof(LG);
  g->gcpause = LUAI_GCPAUSE;
  g->gcstepmul = LUAI_GCMUL;
  g->gcdept = 0;
  for (i=0; i<NUM_TAGS; i++) g->mt[i] = NULL;
  //保护模式调用f_luaopen
  if (luaD_rawrunprotected(L, f_luaopen, NULL) != 0) {
    /* memory allocation error: free partial state */
    //失败情况
    close_state(L);
    L = NULL;
  }
  else{
    //自定义函数
    luai_userstateopen(L);
  }
  return L;
}

//调用所有GC
static void callallgcTM (lua_State *L, void *ud) {
  UNUSED(ud);
  luaC_callGCTM(L);  /* call GC metamethods for all udata */
}

/**
 * 销毁指定 Lua 状态机中的所有对象 （如果有垃圾收集相关的元方法的话，会调用它们）， 并且释放状态机中使用的所有动态内存。 
 * 在一些平台上，你可以不必调用这个函数， 因为当宿主程序结束的时候，所有的资源就自然被释放掉了。 
 * 另一方面，长期运行的程序，比如一个后台程序或是一个网站服务器， 会创建出多个 Lua 状态机。那么就应该在不需要时赶紧关闭它们。
 */
LUA_API void lua_close (lua_State *L) {
  L = G(L)->mainthread;  /* only the main thread can be closed */
  lua_lock(L);
  // 清除栈上的所有上值
  luaF_close(L, L->stack);  /* close all upvalues for this thread */
  // TODO:GC相关，将有__gc的userdata和没有的userdata分离
  luaC_separateudata(L, 1);  /* separate udata that have GC metamethods */
  L->errfunc = 0;  /* no error function during GC metamethods */
  do {  /* repeat until no more errors */
    L->ci = L->base_ci;
    L->base = L->top = L->ci->base;
    L->nCcalls = L->baseCcalls = 0;
  } while (luaD_rawrunprotected(L, callallgcTM, NULL) != 0);
  //保证调用完之后不存在包含gc的对象
  lua_assert(G(L)->tmudata == NULL);
  //userstate关闭时候的回调
  luai_userstateclose(L);
  close_state(L);
}
