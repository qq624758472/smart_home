################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
S_UPPER_SRCS += \
F:/smart_home/zyzb010/platform/boot/826x/cstartup_826x.S 

OBJS += \
./platform/boot/826x/cstartup_826x.o 


# Each subdirectory must supply rules for building sources it contributes
platform/boot/826x/cstartup_826x.o: F:/smart_home/zyzb010/platform/boot/826x/cstartup_826x.S
	@echo 'Building file: $<'
	@echo 'Invoking: TC32 CC/Assembler'
	tc32-elf-gcc -DMCU_CORE_8258 -DMCU_STARTUP_8258 -I../../../apps/sampleLight -c -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


