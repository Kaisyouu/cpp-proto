#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <thread>

// Global control variable
std::atomic<bool> g_bContinueTesting{true};

// Exception type enumeration
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
    ABORT_CALLED,
    SEGMENTATION_FAULT_SIMULATION
};

// Get random exception type
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
        ExceptionType::ABORT_CALLED,
        ExceptionType::SEGMENTATION_FAULT_SIMULATION};

    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, exceptions.size() - 1);

    return exceptions[dis(gen)];
}

// Trigger access violation exception
void TriggerAccessViolation()
{
    std::cout << "[Exception] Triggering access violation exception..." << std::endl;
    int* pNull = nullptr;
    *pNull = 42;  // Writing to null pointer
}

// Trigger stack overflow exception
void TriggerStackOverflow(int depth = 0)
{
    if (depth > 500)
    {  // Reduce depth to prevent system freeze
        std::cout << "[Exception] Stack overflow prevention mechanism triggered, stopping recursion" << std::endl;
        return;
    }

    volatile int localVars[500];  // Reduce stack space usage
    for (auto& var : localVars)
    {
        var = depth;
    }

    // Recursive call causing stack overflow
    TriggerStackOverflow(depth + 1);
}

// Trigger illegal instruction exception - simulated with division by zero
void TriggerIllegalInstruction()
{
    std::cout << "[Exception] Triggering illegal instruction exception (simulated with division by zero)..." << std::endl;
    volatile int zero = 0;
    volatile int result = 1 / zero;  // Division by zero may cause floating point exception
    (void) result;
}

// Trigger divide by zero exception
void TriggerDivideByZero()
{
    std::cout << "[Exception] Triggering divide by zero exception..." << std::endl;
    volatile int numerator = 10;
    volatile int denominator = 0;
    volatile int result = numerator / denominator;
    (void) result;
}

// Trigger pure virtual function call exception
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
    std::cout << "[Exception] Triggering pure virtual function call exception..." << std::endl;

    // Using smart pointer for proper lifecycle management
    auto pAbstract = std::make_unique<DerivedClass>();

    // Can't directly call pure virtual function as object is complete
    // We simulate a corrupted object state
    AbstractClass* rawPtr = pAbstract.release();  // Release ownership but don't delete object
    delete rawPtr;                                // Delete object

    // Now call pure virtual function on deleted object
    // This causes undefined behavior, may trigger pure virtual function call exception
    // Note: This may not always trigger, but worth testing
    try
    {
        rawPtr->PureVirtualFunction();
    }
    catch (...)
    {
        std::cout << "[Caught] Pure virtual function call exception caught" << std::endl;
    }
}

// Trigger invalid parameter exception
void TriggerInvalidParameter()
{
    std::cout << "[Exception] Triggering invalid parameter exception..." << std::endl;
#ifdef _MSC_VER
    // MSVC specific invalid parameter test
    char* invalidPtr = nullptr;
    // This will trigger Microsoft's invalid parameter handling
    errno_t err = strcpy_s(invalidPtr, 10, "test");
    if (err != 0)
    {
        std::cout << "[Caught] strcpy_s returned error code: " << err << std::endl;
    }
#else
    // Alternative for other compilers
    volatile int* invalidPtr = nullptr;
    *invalidPtr = 42;  // Dereferencing null pointer
#endif
}

// Trigger C++ exception
void TriggerCppException()
{
    std::cout << "[Exception] Triggering C++ exception..." << std::endl;
    throw std::runtime_error("Intentionally thrown test exception");
}

// Trigger memory allocation failure
void TriggerMemoryAllocationFailure()
{
    std::cout << "[Exception] Triggering memory allocation failure..." << std::endl;
    try
    {
        // Try to allocate huge memory block
        const size_t hugeSize = static_cast<size_t>(1024) * 1024 * 1024 * 4;  // 4GB
        char* pMem = new char[hugeSize];
        delete[] pMem;
    }
    catch (const std::bad_alloc& e)
    {
        std::cout << "[Caught] Successfully caught memory allocation failure: " << e.what() << std::endl;
        throw;  // Re-throw to maintain test effect
    }
}

// Terminate handler function
void CustomTerminateHandler()
{
    std::cout << "[Exception] Custom terminate handler called" << std::endl;
    // Note: Throwing an exception in terminate handler is dangerous, usually leads to std::abort()
    std::exit(EXIT_FAILURE);  // Use exit instead of abort to avoid signal loop
}

void TriggerTerminateCalled()
{
    std::cout << "[Exception] Triggering terminate call..." << std::endl;
    std::set_terminate(CustomTerminateHandler);

    // Set condition that will call terminate
    throw 42;  // Throwing non-standard exception, may not directly call terminate without exception specification
    // More direct method is to call std::terminate()
    // std::terminate();
}

// Alternative to unexpected - using noexcept violation
void TriggerAbortCalled()
{
    std::cout << "[Exception] Triggering abort call..." << std::endl;
    std::abort();
}

// Simulate segmentation fault (Unix-style, may cause access violation on Windows)
void TriggerSegmentationFaultSimulation()
{
    std::cout << "[Exception] Simulating segmentation fault..." << std::endl;
    // On Windows, this usually causes access violation, similar to ACCESS_VIOLATION
    int* ptr = reinterpret_cast<int*>(0xDEADBEEF);
    *ptr = 123;  // Writing to invalid address
}

// Setup exception handlers
void SetupExceptionHandlers()
{
    // Setup terminate handler
    std::set_terminate(CustomTerminateHandler);

    // Note: std::set_unexpected removed in C++17, not used here

    std::cout << "[Setup] Exception handlers configured" << std::endl;
}

// Main test thread function
void ExceptionTestThread()
{
    std::cout << "[Thread] Exception test thread started, will trigger various exceptions irregularly for 10 seconds..." << std::endl;
    SetupExceptionHandlers();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> timeDis(200, 1500);  // 200ms to 1500ms interval

    auto start = std::chrono::steady_clock::now();

    while (g_bContinueTesting &&
           std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start)
                   .count() < 10000)
    {
        // Random wait
        std::this_thread::sleep_for(std::chrono::milliseconds(timeDis(gen)));

        if (!g_bContinueTesting)
            break;

        // Randomly select an exception to trigger
        ExceptionType excType = GetRandomException();

        try
        {
            switch (excType)
            {
                case ExceptionType::ACCESS_VIOLATION:
                    TriggerAccessViolation();
                    break;

                case ExceptionType::STACK_OVERFLOW:
                    std::cout << "[Exception] Triggering stack overflow exception..." << std::endl;
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

                case ExceptionType::ABORT_CALLED:
                    TriggerAbortCalled();
                    break;

                case ExceptionType::SEGMENTATION_FAULT_SIMULATION:
                    TriggerSegmentationFaultSimulation();
                    break;

                default:
                    std::cout << "[Exception] Unknown exception type" << std::endl;
                    break;
            }

            std::cout << "[Thread] Exception test completed, continuing execution..." << std::endl
                      << std::endl;
        }
        catch (const std::exception& e)
        {
            std::cout << "[Thread] Caught standard exception: " << e.what() << std::endl
                      << std::endl;
        }
        catch (...)
        {
            std::cout << "[Thread] Caught unknown or non-standard exception" << std::endl
                      << std::endl;
        }
    }

    std::cout << "[Thread] Exception test thread ended" << std::endl;
}

// Helper function to stop testing
void StopExceptionTesting()
{
    g_bContinueTesting = false;
}