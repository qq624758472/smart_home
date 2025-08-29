################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
F:/smart_home/zyzb010/apps/common/factory_reset.c \
F:/smart_home/zyzb010/apps/common/firmwareEncryptChk.c \
F:/smart_home/zyzb010/apps/common/main.c \
F:/smart_home/zyzb010/apps/common/module_test.c 

OBJS += \
./apps/common/factory_reset.o \
./apps/common/firmwareEncryptChk.o \
./apps/common/main.o \
./apps/common/module_test.o 


# Each subdirectory must supply rules for building sources it contributes
apps/common/factory_reset.o: F:/smart_home/zyzb010/apps/common/factory_reset.c
	@echo 'Building file: $<'
	@echo 'Invoking: TC32 Compiler'
	tc32-elf-gcc -ffunction-sections -fdata-sections -I../../../apps/common -I../../../apps/sampleGW -I../../../platform -I../../../platform/tc32 -I../../../proj/common -I../../../proj -I../../../zigbee/common/includes -I../../../zigbee/zbapi -I../../../zigbee/bdb/includes -I../../../zigbee/gp -I../../../zigbee/zcl -I../../../zigbee/ota -I../../../zbhci -DCOORDINATOR=1 -DMCU_CORE_8258=1 -D__PROJECT_TL_GW__=1 -Wall -O2 -fpack-struct -fshort-enums -finline-small-functions -std=gnu99 -fshort-wchar -fms-extensions -c -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

apps/common/firmwareEncryptChk.o: F:/smart_home/zyzb010/apps/common/firmwareEncryptChk.c
	@echo 'Building file: $<'
	@echo 'Invoking: TC32 Compiler'
	tc32-elf-gcc -ffunction-sections -fdata-sections -I../../../apps/common -I../../../apps/sampleGW -I../../../platform -I../../../platform/tc32 -I../../../proj/common -I../../../proj -I../../../zigbee/common/includes -I../../../zigbee/zbapi -I../../../zigbee/bdb/includes -I../../../zigbee/gp -I../../../zigbee/zcl -I../../../zigbee/ota -I../../../zbhci -DCOORDINATOR=1 -DMCU_CORE_8258=1 -D__PROJECT_TL_GW__=1 -Wall -O2 -fpack-struct -fshort-enums -finline-small-functions -std=gnu99 -fshort-wchar -fms-extensions -c -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

apps/common/main.o: F:/smart_home/zyzb010/apps/common/main.c
	@echo 'Building file: $<'
	@echo 'Invoking: TC32 Compiler'
	tc32-elf-gcc -ffunction-sections -fdata-sections -I../../../apps/common -I../../../apps/sampleGW -I../../../platform -I../../../platform/tc32 -I../../../proj/common -I../../../proj -I../../../zigbee/common/includes -I../../../zigbee/zbapi -I../../../zigbee/bdb/includes -I../../../zigbee/gp -I../../../zigbee/zcl -I../../../zigbee/ota -I../../../zbhci -DCOORDINATOR=1 -DMCU_CORE_8258=1 -D__PROJECT_TL_GW__=1 -Wall -O2 -fpack-struct -fshort-enums -finline-small-functions -std=gnu99 -fshort-wchar -fms-extensions -c -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

apps/common/module_test.o: F:/smart_home/zyzb010/apps/common/module_test.c
	@echo 'Building file: $<'
	@echo 'Invoking: TC32 Compiler'
	tc32-elf-gcc -ffunction-sections -fdata-sections -I../../../apps/common -I../../../apps/sampleGW -I../../../platform -I../../../platform/tc32 -I../../../proj/common -I../../../proj -I../../../zigbee/common/includes -I../../../zigbee/zbapi -I../../../zigbee/bdb/includes -I../../../zigbee/gp -I../../../zigbee/zcl -I../../../zigbee/ota -I../../../zbhci -DCOORDINATOR=1 -DMCU_CORE_8258=1 -D__PROJECT_TL_GW__=1 -Wall -O2 -fpack-struct -fshort-enums -finline-small-functions -std=gnu99 -fshort-wchar -fms-extensions -c -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


