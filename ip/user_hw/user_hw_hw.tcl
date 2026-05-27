# TCL File for user_HW QSys component
# Custom hardware: adds 1 to each ASCII char, 0xF7 becomes 0x00

package require -exact sopc 10.0

set_module_property DESCRIPTION "User HW - String char processor (char+1, 0xF7->0x00)"
set_module_property NAME user_HW
set_module_property VERSION 1.0
set_module_property INTERNAL false
set_module_property GROUP "Lab7"
set_module_property DISPLAY_NAME "User HW String Processor"
set_module_property TOP_LEVEL_HDL_FILE user_hw.vhd
set_module_property TOP_LEVEL_HDL_MODULE user_HW
set_module_property INSTANTIATE_IN_SYSTEM_MODULE true
set_module_property EDITABLE true
set_module_property ANALYZE_HDL TRUE

# files
add_file user_hw.vhd {SYNTHESIS SIMULATION}

# clock interface
add_interface clock clock end
set_interface_property clock ENABLED true
add_interface_port clock clk clk Input 1

# reset interface
add_interface reset reset end
set_interface_property reset associatedClock clock
set_interface_property reset synchronousEdges DEASSERT
set_interface_property reset ASSOCIATED_CLOCK clock
set_interface_property reset ENABLED true
add_interface_port reset reset_n reset_n Input 1

# Avalon-MM slave interface
add_interface s1 avalon end
set_interface_property s1 addressAlignment NATIVE
set_interface_property s1 associatedClock clock
set_interface_property s1 associatedReset reset
set_interface_property s1 burstOnBurstBoundariesOnly false
set_interface_property s1 explicitAddressSpan 0
set_interface_property s1 holdTime 0
set_interface_property s1 isMemoryDevice false
set_interface_property s1 isNonVolatileStorage false
set_interface_property s1 linewrapBursts false
set_interface_property s1 maximumPendingReadTransactions 0
set_interface_property s1 printableDevice false
set_interface_property s1 readLatency 0
set_interface_property s1 readWaitTime 1
set_interface_property s1 setupTime 0
set_interface_property s1 timingUnits Nanoseconds
set_interface_property s1 writeWaitTime 0
set_interface_property s1 ASSOCIATED_CLOCK clock
set_interface_property s1 ENABLED true

add_interface_port s1 chipselect chipselect Input 1
add_interface_port s1 address address Input 3
add_interface_port s1 write write Input 1
add_interface_port s1 writedata writedata Input 32
add_interface_port s1 read read Input 1
add_interface_port s1 readdata readdata Output 32
