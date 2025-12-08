idf_component_register(SRCS "src/ubpf_esp32.c"
                            "../../vm/ubpf_vm.c"
                            "../../vm/ubpf_loader.c"
                            "../../vm/ubpf_jit.c"
                            "../../vm/ubpf_jit_support.c"
                            "../../vm/ubpf_instruction_valid.c"
                       INCLUDE_DIRS "include" "compat" "../../vm/inc" "../../vm"
                       REQUIRES nvs_flash)

# Ensure ubpf_config.h is found
target_compile_definitions(${COMPONENT_LIB} PRIVATE UBPF_HAS_ELF_H=1 UBPF_MAX_INSTS=4096)

# Suppress warnings on 32-bit RISC-V (ubpf assumes 64-bit pointers in places)
target_compile_options(${COMPONENT_LIB} PRIVATE 
    -Wno-error=int-to-pointer-cast 
    -Wno-error=pointer-to-int-cast 
    -Wno-error=cast-function-type
)
