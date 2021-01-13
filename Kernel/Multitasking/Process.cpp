#include "Interrupts/Utilities.h"

#include "Memory/MemoryManager.h"

#include "Scheduler.h"

#include "Process.h"

namespace kernel {

Atomic<u32> Process::s_next_process_id;

RefPtr<Process> Process::create_idle_for_this_processor()
{
    String process_name = "cpu ";
    process_name << CPU::current_id() << " idle task";

    RefPtr<Process> process = new Process(AddressSpace::of_kernel(), IsSupervisor::YES, process_name.to_view());

    auto main_thread = Thread::create_idle(*process);
    process->m_threads.emplace(main_thread);

    CPU::current().set_idle_task(process);

    return process;
}

RefPtr<Process> Process::create_supervisor(Address entrypoint, StringView name, size_t stack_size)
{
    RefPtr<Process> process = new Process(AddressSpace::of_kernel(), IsSupervisor::YES, name);

    auto stack = MemoryManager::the().allocate_kernel_stack(String(name) + " thread 0 stack", stack_size);
    process->m_virtual_regions.emplace(stack);

    auto main_thread = Thread::create_supervisor(*process, stack->virtual_range().end(), entrypoint);
    process->m_threads.emplace(main_thread);

    Scheduler::the().register_process(process);

    return process;
}

void Process::create_thread(Address entrypoint, size_t stack_size)
{
    LOCK_GUARD(m_lock);

    RefPtr<Thread> thread;

    if (m_is_supervisor == IsSupervisor::YES) {
        String name = m_name;
        name << " thread " << m_next_thread_id.load() << " stack";
        auto stack = MemoryManager::the().allocate_kernel_stack(name, stack_size);
        m_virtual_regions.emplace(stack);
        thread = Thread::create_supervisor(*this, stack->virtual_range().end(), entrypoint);
    } else {
        ASSERT_NEVER_REACHED(); // trying to create user thread (TODO)
    }

    Scheduler::the().register_thread(thread.get());
}

Process::Process(AddressSpace& address_space, IsSupervisor is_supervisor, StringView name)
    : m_id(s_next_process_id++)
    , m_address_space(address_space)
    , m_is_supervisor(is_supervisor)
    , m_name(name)
{
}
}
