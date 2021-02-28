#include "AHCIPort.h"
#include "AHCI.h"

namespace kernel {

AHCIPort::AHCIPort(AHCI& parent, size_t index)
    : m_controller(parent)
    , m_index(index)
{
}

void AHCIPort::read_synchronous(Address into_virtual_address, LBARange range)
{
    m_controller.read_synchronous(m_index, into_virtual_address, range);
}

void AHCIPort::write_synchronous(Address from_virtual_address, LBARange range)
{
    m_controller.write_synchronous(m_index, from_virtual_address, range);
}

StorageDevice::Info AHCIPort::query_info() const
{
    auto& state = m_controller.state_of_port(m_index);

    Info info {};
    info.lba_count = state.lba_count;
    info.lba_size = state.logical_sector_size;
    info.optimal_read_size = state.physical_sector_size;

    return info;
}

}