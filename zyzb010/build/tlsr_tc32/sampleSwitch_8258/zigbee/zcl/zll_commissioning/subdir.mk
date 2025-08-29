################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
F:/smart_home/zyzb010/zigbee/zcl/zll_commissioning/zcl_toucklink_security.c \
F:/smart_home/zyzb010/zigbee/zcl/zll_commissioning/zcl_zllTouchLinkDiscovery.c \
F:/smart_home/zyzb010/zigbee/zcl/zll_commissioning/zcl_zllTouchLinkJoinOrStart.c \
F:/smart_home/zyzb010/zigbee/zcl/zll_commissioning/zcl_zll_commissioning.c 

OBJS += \
./zigbee/zcl/zll_commissioning/zcl_toucklink_security.o \
./zigbee/zcl/zll_commissioning/zcl_zllTouchLinkDiscovery.o \
./zigbee/zcl/zll_commissioning/zcl_zllTouchLinkJoinOrStart.o \
./zigbee/zcl/zll_commissioning/zcl_zll_commissioning.o 


# Each subdirectory must supply rules for building sources it contributes
zigbee/zcl/zll_commissioning/zcl_toucklink_security.o: F:/smart_home/zyzb010/zigbee/zcl/zll_commissioning/zcl_toucklink_security.c
	@echo 'Building file: $<'
	@echo 'Invoking: TC32 Compiler'
	tc32-elf-gcc -ffunction-sections -fdata-sections -I../../../apps/common -I../../../apps/sampleSwitch -I../../../platform -I../../../proj/common -I../../../proj -I../../../zigbee/common/includes -I../../../zigbee/zbapi -I../../../zigbee/bdb/includes -I../../../zigbee/zcl -I../../../zigbee/ota -I../../../zbhci -DEND_DEVICE=1 -DMCU_CORE_8258=1 -D__PROJECT_TL_SWITCH__=1 -Wall -O2 -fpack-struct -fshort-enums -finline-small-functions -std=gnu99 -fshort-wchar -fms-extensions -c -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

zigbee/zcl/zll_commissioning/zcl_zllTouchLinkDiscovery.o: F:/smart_home/zyzb010/zigbee/zcl/zll_commissioning/zcl_zllTouchLinkDiscovery.c
	@echo 'Building file: $<'
	@echo 'Invoking: TC32 Compiler'
	tc32-elf-gcc -ffunction-sections -fdata-sections -I../../../apps/common -I../../../apps/sampleSwitch -I../../../platform -I../../../proj/common -I../../../proj -I../../../zigbee/common/includes -I../../../zigbee/zbapi -I../../../zigbee/bdb/includes -I../../../zigbee/zcl -I../../../zigbee/ota -I../../../zbhci -DEND_DEVICE=1 -DMCU_CORE_8258=1 -D__PROJECT_TL_SWITCH__=1 -Wall -O2 -fpack-struct -fshort-enums -finline-small-functions -std=gnu99 -fshort-wchar -fms-extensions -c -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

zigbee/zcl/zll_commissioning/zcl_zllTouchLinkJoinOrStart.o: F:/smart_home/zyzb010/zigbee/zcl/zll_commissioning/zcl_zllTouchLinkJoinOrStart.c
	@echo 'Building file: $<'
	@echo 'Invoking: TC32 Compiler'
	tc32-elf-gcc -ffunction-sections -fdata-sections -I../../../apps/common -I../../../apps/sampleSwitch -I../../../platform -I../../../proj/common -I../../../proj -I../../../zigbee/common/includes -I../../../zigbee/zbapi -I../../../zigbee/bdb/includes -I../../../zigbee/zcl -I../../../zigbee/ota -I../../../zbhci -DEND_DEVICE=1 -DMCU_CORE_8258=1 -D__PROJECT_TL_SWITCH__=1 -Wall -O2 -fpack-struct -fshort-enums -finline-small-functions -std=gnu99 -fshort-wchar -fms-extensions -c -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

zigbee/zcl/zll_commissioning/zcl_zll_commissioning.o: F:/smart_home/zyzb010/zigbee/zcl/zll_commissioning/zcl_zll_commissioning.c
	@echo 'Building file: $<'
	@echo 'Invoking: TC32 Compiler'
	tc32-elf-gcc -ffunction-sections -fdata-sections -I../../../apps/common -I../../../apps/sampleSwitch -I../../../platform -I../../../proj/common -I../../../proj -I../../../zigbee/common/includes -I../../../zigbee/zbapi -I../../../zigbee/bdb/includes -I../../../zigbee/zcl -I../../../zigbee/ota -I../../../zbhci -DEND_DEVICE=1 -DMCU_CORE_8258=1 -D__PROJECT_TL_SWITCH__=1 -Wall -O2 -fpack-struct -fshort-enums -finline-small-functions -std=gnu99 -fshort-wchar -fms-extensions -c -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


