# SPDX-License-Identifier: Apache-2.0

#board_runner_args(dfu-util "--pid=0483:df11" "--alt=0" "--dfuse")
#board_runner_args(dfu-util "--pid=28e9:0189" "--alt=0" "--dfuse")

#include(${ZEPHYR_BASE}/boards/common/dfu-util.board.cmake)

board_runner_args(stm32flash "--baud-rate=115200" "--start-addr=0x08000000")# "--frame-length=48:48")

include(${ZEPHYR_BASE}/boards/common/stm32flash.board.cmake)
