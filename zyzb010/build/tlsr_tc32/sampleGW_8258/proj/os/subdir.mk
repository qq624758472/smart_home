################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
F:/smart_home/zyzb010/proj/os/ev.c \
F:/smart_home/zyzb010/proj/os/ev_buffer.c \
F:/smart_home/zyzb010/proj/os/ev_poll.c \
F:/smart_home/zyzb010/proj/os/ev_queue.c \
F:/smart_home/zyzb010/proj/os/ev_timer.c 

OBJS += \
./proj/os/ev.o \
./proj/os/ev_buffer.o \
./proj/os/ev_poll.o \
./proj/os/ev_queue.o \
./proj/os/ev_timer.o 


# Each subdirectory must supply rules for building sources it contributes
proj/os/ev.o: F:/smart_home/zyzb010/proj/os/ev.c
	@echo 'Building file: $<'
	@echo 'Invoking: TC32 Compiler'
	tc32-elf-gcc -ffunction-sections -fdata-sections -I../../../apps/common -I../../../apps/sampleGW -I../../../platform -I../../../platform/tc32 -I../../../proj/common -I../../../proj -I../../../zigbee/common/includes -I../../../zigbee/zbapi -I../../../zigbee/bdb/includes -I../../../zigbee/gp -I../../../zigbee/zcl -I../../../zigbee/ota -I../../../zbhci -DCOORDINATOR=1 -DMCU_CORE_8258=1 -D__PROJECT_TL_GW__=1 -Wall -O2 -fpack-struct -fshort-enums -finline-small-functions -std=gnu99 -fshort-wchar -fms-extensions -c -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

proj/os/ev_buffer.o: F:/smart_home/zyzb010/proj/os/ev_buffer.c
	@echo 'Building file: $<'
	@echo 'Invoking: TC32 Compiler'
	tc32-elf-gcc -ffunction-sections -fdata-sections -I../../../apps/common -I../../../apps/sampleGW -I../../../platform -I../../../platform/tc32 -I../../../proj/common -I../../../proj -I../../../zigbee/common/includes -I../../../zigbee/zbapi -I../../../zigbee/bdb/includes -I../../../zigbee/gp -I../../../zigbee/zcl -I../../../zigbee/ota -I../../../zbhci -DCOORDINATOR=1 -DMCU_CORE_8258=1 -D__PROJECT_TL_GW__=1 -Wall -O2 -fpack-struct -fshort-enums -finline-small-functions -std=gnu99 -fshort-wchar -fms-extensions -c -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

proj/os/ev_poll.o: F:/smart_home/zyzb010/proj/os/ev_poll.c
	@echo 'Building file: $<'
	@echo 'Invoking: TC32 Compiler'
	tc32-elf-gcc -ffunction-sections -fdata-sections -I../../../apps/common -I../../../apps/sampleGW -I../../../platform -I../../../platform/tc32 -I../../../proj/common -I../../../proj -I../../../zigbee/common/includes -I../../../zigbee/zbapi -I../../../zigbee/bdb/includes -I../../../zigbee/gp -I../../../zigbee/zcl -I../../../zigbee/ota -I../../../zbhci -DCOORDINATOR=1 -DMCU_CORE_8258=1 -D__PROJECT_TL_GW__=1 -Wall -O2 -fpack-struct -fshort-enums -finline-small-functions -std=gnu99 -fshort-wchar -fms-extensions -c -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

proj/os/ev_queue.o: F:/smart_home/zyzb010/proj/os/ev_queue.c
	@echo 'Building file: $<'
	@echo 'Invoking: TC32 Compiler'
	tc32-elf-gcc -ffunction-sections -fdata-sections -I../../../apps/common -I../../../apps/sampleGW -I../../../platform -I../../../platform/tc32 -I../../../proj/common -I../../../proj -I../../../zigbee/common/includes -I../../../zigbee/zbapi -I../../../zigbee/bdb/includes -I../../../zigbee/gp -I../../../zigbee/zcl -I../../../zigbee/ota -I../../../zbhci -DCOORDINATOR=1 -DMCU_CORE_8258=1 -D__PROJECT_TL_GW__=1 -Wall -O2 -fpack-struct -fshort-enums -finline-small-functions -std=gnu99 -fshort-wchar -fms-extensions -c -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

proj/os/ev_timer.o: F:/smart_home/zyzb010/proj/os/ev_timer.c
	@echo 'Building file: $<'
	@echo 'Invoking: TC32 Compiler'
	tc32-elf-gcc -ffunction-sections -fdata-sections -I../../../apps/common -I../../../apps/sampleGW -I../../../platform -I../../../platform/tc32 -I../../../proj/common -I../../../proj -I../../../zigbee/common/includes -I../../../zigbee/zbapi -I../../../zigbee/bdb/includes -I../../../zigbee/gp -I../../../zigbee/zcl -I../../../zigbee/ota -I../../../zbhci -DCOORDINATOR=1 -DMCU_CORE_8258=1 -D__PROJECT_TL_GW__=1 -Wall -O2 -fpack-struct -fshort-enums -finline-small-functions -std=gnu99 -fshort-wchar -fms-extensions -c -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


