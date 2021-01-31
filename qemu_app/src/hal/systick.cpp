
#include "hal/systick.h"

struct SysTickRegs {
  volatile union {
    uint32_t reg;
    struct {
      uint32_t enable : 1;
      uint32_t tickint : 1;
      uint32_t clksource : 1;
      uint32_t : 13;
      uint32_t countflag: 1;
    } bits;
  } CSR;
  volatile union {
    uint32_t reg;
    struct {
      uint32_t rv : 24;
    } bits;
  } RVR;
  volatile union {
    uint32_t reg;
    struct {
      uint32_t cv: 24;
    } bits;
  } CVR;
  volatile union {
    uint32_t reg;
    struct {
      uint32_t cal_value: 24;
      uint32_t : 6;
      uint32_t skew: 1;
      uint32_t noref: 1;
    } bits;
  } CALVR;
};

static __attribute__((section(".bss.systick_regs"))) SysTickRegs s_systick_regs;

extern "C" void SysTick_Handler() {
  SysTick::getInstance().m_ticks++;
}

void SysTick::init(uint32_t core_clk_hz) {
  m_ticks = 0;
  s_systick_regs.CSR.bits.enable = false;

  m_max_count = core_clk_hz / TICKS_PER_SECOND;
  const uint32_t reload_value = m_max_count - 1;
  s_systick_regs.RVR.bits.rv = reload_value;

  s_systick_regs.CSR.bits.clksource = true;
  s_systick_regs.CSR.bits.tickint = true;
  s_systick_regs.CSR.bits.enable = true;
}

uint64_t SysTick::getTickCount() {
  uint32_t coarse = getCoarseTickCount();
  uint32_t fine = getFineTickCount();

  if ((coarse != getCoarseTickCount()) &&
      (fine < (m_max_count / 2))) {
    coarse = getCoarseTickCount();
  }

  return static_cast<uint64_t>(m_max_count) * coarse + fine;
}

uint32_t SysTick::getFineTickCount() {
  return m_max_count - s_systick_regs.CVR.reg - 1;
}

void SysTick::delay(uint32_t coarse_ticks) {
  const uint32_t start = m_ticks;
  while((m_ticks - start) < coarse_ticks);
}
