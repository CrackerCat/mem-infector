#ifndef INFECTOR_H_
#define INFECTOR_H_

#include <vector>
#include <iostream>
#include <functional>
#include <map>
#include <elf.h>

#define MAX_ARG_NUM 7

class TargetOpt;
struct xed_decoded_inst_s;
typedef struct xed_decoded_inst_s xed_decoded_inst_t;

class Infector final
{
public:
    using SymTab = std::map<std::string, long>;
    using SymTabs = std::map<std::string, SymTab>;

    /**
     * @brief 构造函数，创建对象
     * 
     * @param pid   需要感染的目标进程
     * @param libcSoname   目标进程使用的C库名称
     */
    Infector(int pid, const std::string &libcSoname);
    ~Infector();

    /**
     * @brief 链接目标进程并且将其阻塞
     * @return true 成功
     * @return false 失败
     */
    bool attachTarget();

    /**
     * @brief 断开与目标进程的链接，使目标进程重新进入运行状态
     * @return true 成功
     * @return false 失败
     */
    bool detachTarget();

    /**
     * @brief 在目标进程内部调用其函数
     * 
     * @tparam Args  可以传入任意类型的参数，但是参数类型一定要限制为CPU寄存器认识的类型
     *               如，地址，值变量等 包括但不限于 long int short char等等。
     * @param args  第一个参数为目标进程的函数地址，表示在目标函数内部调用该函数，
     *              后面的参数均为传入目标进程函数的参数，但是要注意后面的参数一定也要来自目标进程。
     *              并且还需要注意第一个参数后面的参数数量要与目标函数定义的参数数量相等。
     * @return long 在目标进程内部调用函数完毕后，将返回值读取到本进程
     */
    template<class ...Args>
    long callRemoteFunc(Args ...args)
    {       
        constexpr int argsNum = sizeof...(args);
        static_assert(argsNum <= MAX_ARG_NUM, 
                     "the number of parameters is more than MAX_ARG_NUM");

        Elf64_Addr retAddr = 0;
        for (int i = 0; i < 20; i++)
        {
            if (!backupTarget())
                return 0;

            callRemoteFuncIdx<0>(args...);
 
            if (!updateTarget())
                return 0;

            retAddr = restoreTarget();
            if (retAddr)
                return retAddr;
                
            if (!stepTarget())
                return 0;
        }
        
        return 0;
    }

    /**
     * @brief 向目标进程的目标地址里面写字符串。
     * @return true 成功
     * @return false 失败
     */
    bool writeStrToTarget(Elf64_Addr &, const std::string &);

    /**
     * @brief 将目标进程链接的动态库的符号信息加载进本进程
     * @param soname 指定动态库名称
     * @return true 成功
     * @return false  失败  
     */
    bool loadSoFile(const std::string &soname);

    /**
     * @brief 获取目标进程连接的动态库的符号地址。与一定要在loadSoFile后面使用
     * 
     * @param soname 指定动态库名称
     * @param symname 符号名称
     * @return long 目标进程内部的符号地址
     */
    long getSym(const std::string &soname, const std::string &symname);

    /**
     * @brief 在目标进程内部注入线程
     * 
     * @param funcAddr 目标进程内部的函数地址
     * @param paramAddr 目标进程内部的参数地址
     * @return true 成功
     * @return false 失败
     */
    bool createThread(Elf64_Addr funcAddr, Elf64_Addr paramAddr);

    /**
     * @brief 目标进程函数劫持，暂时考虑是否弃用该接口
     * 
     * @return int 
     */
    int remoteFuncJump(Elf64_Addr &, Elf64_Addr &, Elf64_Addr &, Elf64_Addr &);

    /**
     * @brief 目标进程的系统调用表劫持，暂时考虑是否弃用该接口
     * 
     * @return true 
     * @return false 
     */
    bool injectSysTableInit();
private:
    template<int idx, class T, class ...Args>
    void callRemoteFuncIdx(T t, Args ...args)
    {
        mRegvec[idx](t);
        return callRemoteFuncIdx<idx+1>(args...);
    }

    template<int idx, class T>
    void callRemoteFuncIdx(T t)
    {
        mRegvec[idx](t);
        return;
    }

    void regvecInit();

    bool backupTarget(); 

    bool updateTarget();

    long restoreTarget();

    bool stepTarget();

    bool getSoInfo(const std::string &, std::string &, Elf64_Addr &);

    Elf64_Addr syscallJmp(const std::string &, const std::string &, 
                          const std::string &, Elf64_Addr);

private:
    int mPid;
    struct user_regs_struct *pNewRegs;
    struct user_regs_struct *pOrigRegs;
    TargetOpt *pTargetOpt;
    xed_decoded_inst_t *xedd;
    std::string mLicSoname;
    std::map<std::string, std::string> soMap;
    std::vector<std::function<void(long)>> mRegvec;
    SymTabs symTabs;
    unsigned char backupCode[8] = {0};
};

#endif
