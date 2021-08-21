#pragma once

#include "Common/Types.h"
#include "Common/Macros.h"

namespace kernel {

struct PACKED PORTPMSCUSB2 {
    u32 L1S : 3;
    u32 RWE : 1;
    u32 BESL : 4;
    u32 L1DeviceSlot : 8;
    u32 HLE : 1;
    u32 RsvdP : 11;
    u32 PortTestControl : 4;
};

struct PACKED PORTPMSCUSB3 {
    u32 U1Timeout : 8;
    u32 U2Timeout : 8;
    u32 FLA : 1;
    u32 RsvdP : 15;
};

struct PACKED PORTLIUSB2 {
    u32 RsvdP;
};

struct PACKED PORTLIUSB3 {
    u32 LinkErrorCount : 16;
    u32 RLC : 4;
    u32 TLC : 4;
    u32 RsvdP : 8;
};

// Fully RsvdP if HLC = 0
struct PACKED PORTHLPMCUSB2 {
    u32 HIRDM : 2;
    u32 L1Timeout : 8;
    u32 BESLD : 4;
    u32 RsvdP : 18;
};

// Fully RsvdP if LSECC = 0
struct PACKED PORTHLPMCUSB3 {
    u32 LinkSoftErrorCount : 16;
    u32 RsvdP : 16;
};

struct PACKED PORTSC {
    u32 CCS : 1;
    u32 PED : 1;
    u32 RsvdZ : 1;
    u32 OCA : 1;
    u32 PR : 1;
    u32 PLS : 4;
    u32 PP : 1;
    u32 ROS : 4;
    u32 PIC : 2;
    u32 LWS : 1;
    u32 CSC : 1;
    u32 PEC : 1;
    u32 WRC : 1;
    u32 OCC : 1;
    u32 PRC : 1;
    u32 PLC : 1;
    u32 CEC : 1;
    u32 CAS : 1;
    u32 WCE : 1;
    u32 WDE : 1;
    u32 WOE : 1;
    u32 RsvdZ1 : 2;
    u32 DR : 1;
    u32 WPR : 1;

    static constexpr size_t offset = 0x0;
};

struct PACKED PortRegister {
    u32 PORTSC;
    u32 PORTPMSC;  // <-| Different meanings for USB2/USB3
    u32 PORTLI;    // <-|
    u32 PORTHLPMC; // <-|
};

struct PACKED HCSPARAMS1 {
    u32 MaxSlots : 8;
    u32 MaxIntrs : 11;
    u32 Rsvd : 5;
    u32 MaxPorts : 8;

    static constexpr size_t offset = 0x4;
};

struct PACKED HCSPARAMS2 {
    u32 IST : 4;
    u32 ERSTMax : 4;
    u32 Rsvd : 13;
    u32 MaxScratchpadBufsHi : 5;
    u32 SPR : 1;
    u32 MaxScratchpadBufsLo : 5;

    static constexpr size_t offset = 0x8;
};

struct PACKED HCSPARAMS3 {
    u32 U1DeviceExitLatency : 8;
    u32 Rsvd : 8;
    u32 U2DeviceExitLatency : 16;

    static constexpr size_t offset = 0xC;
};

struct PACKED HCCPARAMS1 {
    u32 AC64 : 1;
    u32 BNC : 1;
    u32 CSZ : 1;
    u32 PPC : 1;
    u32 PIND : 1;
    u32 LHRC : 1;
    u32 LTC : 1;
    u32 NSS : 1;
    u32 PAE : 1;
    u32 SPC : 1;
    u32 SEC : 1;
    u32 CFC : 1;
    u32 MaxPSASize : 4;
    u32 xECP : 16;

    static constexpr size_t offset = 0x10;
};

struct PACKED HCCPARAMS2 {
    u32 U3C : 1;
    u32 CMC : 1;
    u32 FSC : 1;
    u32 CTC : 1;
    u32 LEC : 1;
    u32 CIC : 1;
    u32 ETC : 1;
    u32 ETC_TSC : 1;
    u32 GSC : 1;
    u32 VTC : 1;
    u32 Rsvd : 22;

    static constexpr size_t offset = 0x1C;
};

struct PACKED CapabilityRegisters {
    u8 CAPLENGTH;
    u8 Rsvd;
    u16 HCIVERSION;

    u32 HCSPARAMS1;
    u32 HCSPARAMS2;
    u32 HCSPARAMS3;
    u32 HCCPARAMS1;

    u32 DBOFF;
    u32 RTSOFF;

    u32 HCCPARAMS2;

    u32 VTIOSOFF;
};

struct PACKED USBCMD {
    u32 RS : 1;
    u32 HCRST : 1;
    u32 INTE : 1;
    u32 HSEE : 1;
    u32 RsvdP : 3;
    u32 LHCRST : 1;
    u32 CSS : 1;
    u32 CRS : 1;
    u32 EWE : 1;
    u32 EU3S : 1;
    u32 RsvdP1 : 1;
    u32 CME : 1;
    u32 ETE : 1;
    u32 TSC_EN : 1;
    u32 VTIOE : 1;
    u32 RsvdP2 : 15;

    static constexpr size_t offset = 0x0;
};

struct PACKED USBSTS {
    u32 HCH : 1;
    u32 RsvdZ : 1;
    u32 HSE : 1;
    u32 EINT : 1;
    u32 PCD : 1;
    u32 RsvdZ1 : 3;
    u32 SSS : 1;
    u32 RSS : 1;
    u32 SRE : 1;
    u32 CNR : 1;
    u32 HCE : 1;
    u32 RsvdZ2 : 19;

    static constexpr size_t offset = 0x4;
};

struct PACKED CRCR {
    u64 RCS : 1;
    u64 CS : 1;
    u64 CA : 1;
    u64 CRR : 1;
    u64 RsvdP : 2;
    u64 CommandRingPointerLo : 26;
    u64 CommandRingPointerHi : 32;

    static constexpr size_t offset = 0x18;
};

struct PACKED DCBAAP {
    u32 DeviceContextBaseAddressArrayPointerLo;
    u32 DeviceContextBaseAddressArrayPointerHi;

    static constexpr size_t offset = 0x30;
};

struct PACKED CONFIG {
    u32 MaxSlotsEn : 8;
    u32 U3E : 1;
    u32 CIE : 1;
    u32 RsvdP : 22;

    static constexpr size_t offset = 0x38;
};

struct PACKED OperationalRegisters {
    u32 USBCMD;
    u32 USBSTS;

    u32 PAGESIZE;

    u64 RsvdZ;

    u32 DNCTRL;
    u64 CRCR;

    u64 RsvdZ1[2];

    u64 DCBAAP;
    u32 CONFIG;

    u8 RsvdZ2[0x400 - 0x3C];

    PortRegister port_registers[];
};

static_assert(sizeof(CapabilityRegisters) == 36, "Incorrect size of capability registers");
static_assert(sizeof(HCSPARAMS1) == 4, "Incorrect size of HCSPARAMS1");
static_assert(sizeof(HCSPARAMS2) == 4, "Incorrect size of HCSPARAMS2");
static_assert(sizeof(HCSPARAMS3) == 4, "Incorrect size of HCSPARAMS3");
static_assert(sizeof(HCCPARAMS1) == 4, "Incorrect size of HGCCPARAMS1");
static_assert(sizeof(HCCPARAMS2) == 4, "Incorrect size of HCSPARAMS2");

static_assert(sizeof(OperationalRegisters) == 0x400, "Incorrect size of operational registers");
static_assert(sizeof(USBCMD) == 4, "Incorrect size of USBCMD");
static_assert(sizeof(USBSTS) == 4, "Incorrect size of USBSTS");
static_assert(sizeof(CRCR) == 8, "Incorrect size of CRCR");
static_assert(sizeof(DCBAAP) == 8, "Incorrect size of DCBAAP");
static_assert(sizeof(CONFIG) == 4, "Incorrect size of CONFIG");

static_assert(sizeof(PortRegister) == 16, "Incorrect size of port register");
static_assert(sizeof(decltype(PortRegister::PORTSC)) == 4, "Incorrect size of PORTSC");

static_assert(sizeof(PORTPMSCUSB2) == 4, "Incorrect size of PORTPMSC for USB2");
static_assert(sizeof(PORTPMSCUSB3) == 4, "Incorrect size of PORTPMSC for USB3");

static_assert(sizeof(PORTLIUSB2) == 4, "Incorrect size of PORTPMSC for USB2");
static_assert(sizeof(PORTLIUSB3) == 4, "Incorrect size of PORTPMSC for USB3");

static_assert(sizeof(PORTHLPMCUSB2) == 4, "Incorrect size of PORTHLPMC for USB2");
static_assert(sizeof(PORTHLPMCUSB3) == 4, "Incorrect size of PORTHLPMC for USB3");

}