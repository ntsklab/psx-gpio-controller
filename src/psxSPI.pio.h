// Wrapper header: prefer generated pio header (from build), fall back to simple defines.

#ifndef PSXSPI_PIO_H
#define PSXSPI_PIO_H

/* Prefer the generated pio header (psxSPI.pio.h) from the build tree if it
   exists at ../build/device_files/psx-device/psxSPI.pio.h (relative to src/).
   This avoids shadowing the generated header with this wrapper when both are
   present. If that isn't present, fall back to other checks or to minimal
   PIN_* defines. */

#if defined(__has_include)
#  if __has_include("../build/device_files/psx-device/psxSPI.pio.h")
#    include "../build/device_files/psx-device/psxSPI.pio.h"
#  elif __has_include("device_files/psx-device/psxSPI.pio.h")
#    include "device_files/psx-device/psxSPI.pio.h"
#  else
#    define PIN_DAT 3
#    define PIN_CMD 4
#    define PIN_SEL 10
#    define PIN_CLK 6
#    define PIN_ACK 7
#  endif
#else
/* No __has_include: try the build-relative path, then the simple name. */
#  include "../build/device_files/psx-device/psxSPI.pio.h"
#endif

#endif // PSXSPI_PIO_H
