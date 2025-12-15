#pragma once
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <thread>


// 全局控制变量
std::atomic<bool> g_bContinueTesting{true};

// 异常类型枚举
enum class ExceptionType
{
    ACCESS_VIOLATION,
    STACK_OVERFLOW,
    ILLEGAL_INSTRUCTION,
    DIVIDE_BY_ZERO,
    PURE_CALL,
    INVALID_PARAMETER,
    THROW_CPP_EXCEPTION,
    MEMORY_ALLOCATION_FAILURE,
    TERMINATE_CALLED,
    UNEXPECTED_CALLED
};

// 获取随机异常类型
ExceptionType GetRandomException()
{
    static const std::vector<ExceptionType> exceptions = {
        ExceptionType::ACCESS_VIOLATION,
        ExceptionType::STACK_OVERFLOW,
        ExceptionType::ILLEGAL_INSTRUCTION,
        ExceptionType::DIVIDE_BY_ZERO,
        ExceptionType::PURE_CALL,
        ExceptionType::INVALID_PARAMETER,
        ExceptionType::THROW_CPP_EXCEPTION,
        ExceptionType::MEMORY_ALLOCATION_FAILURE,
        ExceptionType::TERMINATE_CALLED,
        ExceptionType::UNEXPECTED_CALLED};

    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, exceptions.size() - 1);

    return exceptions[dis(gen)];
}

// 触发访问违例异常
void TriggerAccessViolation()
{
    std::cout << "[异常] 触发访问违例异常..." << std::endl;
    int* pNull = nullptr;
    *pNull = 42;  // 写入空指针
}

// 触发栈溢出异常
void TriggerStackOverflow(int depth = 0)
{
    if (depth > 1000)
        return;  // 防止无限递归

    volatile int localVar[1000];  // 占用大量栈空间
    for (auto& var : localVar)
    {
        var = 0;
    }

    // 递归调用导致栈溢出
    TriggerStackOverflow(depth + 1);
}

// 触发非法指令异常
void TriggerIllegalInstruction()
{
    std::cout << "[异常] 触发非法指令异常..." << std::endl;
    // 注意：此方法在某些平台上可能不起作用或导致未定义行为
    // 这里使用一个未定义的操作，但可能不会在所有编译器上生成非法指令
    // 可以尝试使用汇编，但为了跨平台，这里仅作示例
    volatile int undefinedBehavior = 0;
    undefinedBehavior = undefinedBehavior / undefinedBehavior;  // 除零，但会被下面的函数处理
    // 更可靠的方法可能需要平台特定的代码
}

// 触发除零异常
void TriggerDivideByZero()
{
    std::cout << "[异常] 触发除零异常..." << std::endl;
    volatile int numerator = 10;
    volatile int denominator = 0;
    volatile int result = numerator / denominator;  // 除零
    (void) result;                                  // 避免未使用变量警告
}

// 触发纯虚函数调用异常
class AbstractClass
{
public:
    virtual void PureVirtualFunction() = 0;
    virtual ~AbstractClass() = default;
};

class DerivedClass : public AbstractClass
{
public:
    void PureVirtualFunction() override {}
};

void TriggerPureCall()
{
    std::cout << "[异常] 触发纯虚函数调用异常..." << std::endl;
    AbstractClass* pAbstract = new DerivedClass();
    delete pAbstract;

    // 在对象析构后调用纯虚函数
    pAbstract->PureVirtualFunction();  // 这将触发纯虚函数调用异常
}

// 触发无效参数异常
void TriggerInvalidParameter()
{
    std::cout << "[异常] 触发无效参数异常..." << std::endl;
#ifdef _MSC_VER
    // 使用微软的无效参数处理
    // 触发一个无效的参数，例如传递空指针给需要非空的函数
    // 这里使用 strcpy_s 作为示例
    char* invalidPtr = nullptr;
    strcpy_s(invalidPtr, 10, "test");  // 这将触发无效参数异常
#else
    // 对于非MSVC编译器，可以尝试其他方式，如除零或其他未定义行为
    volatile int zero = 0;
    volatile int result = 10 / zero;  // 除零
    (void) result;
#endif
}

// 触发C++异常
void TriggerCppException()
{
    std::cout << "[异常] 触发C++异常..." << std::endl;
    throw std::runtime_error("故意抛出的测试异常");
}

// 触发内存分配失败
void TriggerMemoryAllocationFailure()
{
    std::cout << "[异常] 触发内存分配失败..." << std::endl;
    const size_t hugeSize = static_cast<size_t>(-1);  // 极大的内存大小
    try
    {
        char* pMem = new char[hugeSize];
        delete[] pMem;
    }
    catch (const std::bad_alloc& e)
    {
        std::cout << "[捕获] 成功捕获内存分配失败: " << e.what() << std::endl;
        throw;  // 重新抛出以保持测试效果
    }
}

// terminate 处理函数
void TerminateHandler()
{
    std::cout << "[异常] terminate() 被调用" << std::endl;
    throw std::runtime_error("terminate handler exception");
}

void TriggerTerminateCalled()
{
    std::cout << "[异常] 触发terminate调用..." << std::endl;
    std::set_terminate([]()
                       {
                           std::cout << "[异常] 自定义 terminate 处理器被调用" << std::endl;
                           std::abort();  // 或者可以抛出异常，但通常 terminate 会导致程序终止
                       });

    // 调用 abort 会触发 terminate
    std::abort();
}

// unexpected 处理函数
void UnexpectedHandler()
{
    std::cout << "[异常] unexpected() 被调用" << std::endl;
    throw std::runtime_error("unexpected handler exception");
}

void TriggerUnexpectedCalled()
{
    std::cout << "[异常] 触发unexpected调用..." << std::endl;
    std::set_unexpected([]()
                        {
        std::cout << "[异常] 自定义 unexpected 处理器被调用" << std::endl;
        throw std::runtime_error("unexpected handler exception"); });

    // 抛出未在异常规范中声明的异常
    throw std::string("意外类型的异常");
}

// 设置异常处理（可选，用于测试异常处理机制）
void SetupExceptionHandlers()
{
    // 设置信号处理（例如 SIGSEGV, SIGFPE 等），但信号处理在不同平台上行为不同
    // 这里不设置，因为主要依赖于直接触发异常
}

// 主测试线程函数
void ExceptionTestThread()
{
    std::cout << "[线程] 异常测试线程启动，将持续10秒不定期触发各种异常..." << std::endl;
    SetupExceptionHandlers();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> timeDis(200, 1500);  // 200ms到1500ms间隔

    auto start = std::chrono::steady_clock::now();

    while (g_bContinueTesting &&
           std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() < 10000)
    {
        // 随机等待一段时间
        std::this_thread::sleep_for(std::chrono::milliseconds(timeDis(gen)));

        if (!g_bContinueTesting)
            break;

        // 随机选择一种异常触发
        ExceptionType excType = GetRandomException();

        try
        {
            switch (excType)
            {
                case ExceptionType::ACCESS_VIOLATION:
                    TriggerAccessViolation();
                    break;

                case ExceptionType::STACK_OVERFLOW:
                    std::cout << "[异常] 触发栈溢出异常..." << std::endl;
                    TriggerStackOverflow();
                    break;

                case ExceptionType::ILLEGAL_INSTRUCTION:
                    TriggerIllegalInstruction();
                    break;

                case ExceptionType::DIVIDE_BY_ZERO:
                    TriggerDivideByZero();
                    break;

                case ExceptionType::PURE_CALL:
                    TriggerPureCall();
                    break;

                case ExceptionType::INVALID_PARAMETER:
                    TriggerInvalidParameter();
                    break;

                case ExceptionType::THROW_CPP_EXCEPTION:
                    TriggerCppException();
                    break;

                case ExceptionType::MEMORY_ALLOCATION_FAILURE:
                    TriggerMemoryAllocationFailure();
                    break;

                case ExceptionType::TERMINATE_CALLED:
                    TriggerTerminateCalled();
                    break;

                case ExceptionType::UNEXPECTED_CALLED:
                    TriggerUnexpectedCalled();
                    break;

                default:
                    std::cout << "[异常] 未知异常类型" << std::endl;
                    break;
            }

            std::cout << "[线程] 异常测试完成，继续执行..." << std::endl
                      << std::endl;
        }
        catch (const std::exception& e)
        {
            std::cout << "[线程] 捕获到标准异常: " << e.what() << std::endl
                      << std::endl;
        }
        catch (...)
        {
            std::cout << "[线程] 捕获到未知异常" << std::endl
                      << std::endl;
        }
    }

    std::cout << "[线程] 异常测试线程结束" << std::endl;
}

// 停止测试的辅助函数
void StopExceptionTesting()
{
    g_bContinueTesting = false;
}