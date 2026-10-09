if(NOT BUILD_TESTS)
    return()
endif()

if(NOT BUILD_GAME_QVMS)
    message(FATAL_ERROR "BUILD_TESTS needs BUILD_GAME_QVMS for the test QVMs")
endif()

include(shared_sources)
include(utils/qvm_tools)

enable_testing()

# vmtest runs QVM code on the interpreter and the bytecode compiler and
# compares the two, so it needs a platform with HAVE_VM_COMPILED
set(VMTEST_SOURCES
    ${SOURCE_DIR}/tests/vm/vmtest.c
    ${SOURCE_DIR}/qcommon/q_math.c
    ${SOURCE_DIR}/qcommon/q_shared.c
    ${SOURCE_DIR}/qcommon/vm.c
    ${SOURCE_DIR}/qcommon/vm_aarch64.c
    ${SOURCE_DIR}/qcommon/vm_armv7l.c
    ${SOURCE_DIR}/qcommon/vm_interpreted.c
    ${SOURCE_DIR}/qcommon/vm_powerpc.c
    ${SOURCE_DIR}/qcommon/vm_sparc.c
    ${SOURCE_DIR}/qcommon/vm_x86.c
    ${ASM_SOURCES}
)

add_executable(vmtest ${VMTEST_SOURCES})
if(UNIX)
    target_link_libraries(vmtest PRIVATE m)
endif()

add_qvm(vmtest_qvm
    OUTPUT_NAME vmtest
    SOURCES
        ${SOURCE_DIR}/tests/vm/qvm/vmtest_qvm.c
        ${SOURCE_DIR}/tests/vm/qvm/vmtest_syscalls.asm)

set(VMTEST_DIR ${CMAKE_BINARY_DIR}/$<CONFIG>)

add_test(NAME vm_suite COMMAND vmtest ${VMTEST_DIR}/vmtest.qvm)

# real game modules against stub system calls: GAME_INIT, CG_INIT, UI_INIT
foreach(GAMEDIR ${BASEGAME} ${MISSIONPACK})
    add_test(NAME vm_smoke_${GAMEDIR}_qagame
        COMMAND vmtest --smoke game ${VMTEST_DIR}/${GAMEDIR}/vm/qagame.qvm 0 0 1234 0)
    add_test(NAME vm_smoke_${GAMEDIR}_cgame
        COMMAND vmtest --smoke cgame ${VMTEST_DIR}/${GAMEDIR}/vm/cgame.qvm 0 0 0 0)
    add_test(NAME vm_smoke_${GAMEDIR}_ui
        COMMAND vmtest --smoke ui ${VMTEST_DIR}/${GAMEDIR}/vm/ui.qvm 1 0)
endforeach()
