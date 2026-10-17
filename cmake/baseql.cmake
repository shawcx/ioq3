# baseql: Quake Live feature parity mod, see docs/quakelive-plan.md
#
# Built from its own copy of the game sources in code/baseql so that the
# upstream game, cgame and ui directories stay untouched.

option(BUILD_BASEQL "Build the baseql mod" OFF)

if(NOT BUILD_BASEQL)
    return()
endif()

if(NOT BUILD_GAME_LIBRARIES AND NOT BUILD_GAME_QVMS)
    return()
endif()

include(utils/qvm_tools)
include(utils/set_output_dirs)

set(BASEQL "baseql")
set(BASEQL_DIR ${SOURCE_DIR}/baseql)

set(BASEQL_BG_SOURCES
    ${BASEQL_DIR}/game/bg_misc.c
    ${BASEQL_DIR}/game/bg_pmove.c
    ${BASEQL_DIR}/game/bg_slidemove.c
    ${BASEQL_DIR}/game/bg_lib.c
    ${BASEQL_DIR}/game/bg_tuning.c
)

set(BASEQL_CGAME_SOURCES
    ${BASEQL_DIR}/cgame/cg_main.c
    ${BASEQL_BG_SOURCES}
    ${BASEQL_DIR}/cgame/cg_consolecmds.c
    ${BASEQL_DIR}/cgame/cg_draw.c
    ${BASEQL_DIR}/cgame/cg_drawtools.c
    ${BASEQL_DIR}/cgame/cg_effects.c
    ${BASEQL_DIR}/cgame/cg_ents.c
    ${BASEQL_DIR}/cgame/cg_event.c
    ${BASEQL_DIR}/cgame/cg_info.c
    ${BASEQL_DIR}/cgame/cg_localents.c
    ${BASEQL_DIR}/cgame/cg_marks.c
    ${BASEQL_DIR}/cgame/cg_particles.c
    ${BASEQL_DIR}/cgame/cg_players.c
    ${BASEQL_DIR}/cgame/cg_playerstate.c
    ${BASEQL_DIR}/cgame/cg_predict.c
    ${BASEQL_DIR}/cgame/cg_scoreboard.c
    ${BASEQL_DIR}/cgame/cg_servercmds.c
    ${BASEQL_DIR}/cgame/cg_snapshot.c
    ${BASEQL_DIR}/cgame/cg_view.c
    ${BASEQL_DIR}/cgame/cg_weapons.c
    ${BASEQL_DIR}/cgame/cg_newdraw.c
    ${BASEQL_DIR}/ui/ui_shared.c
    ${GAME_MODULE_SHARED_SOURCES}
)

set(BASEQL_GAME_SOURCES
    ${BASEQL_DIR}/game/g_main.c
    ${BASEQL_DIR}/game/ai_chat.c
    ${BASEQL_DIR}/game/ai_cmd.c
    ${BASEQL_DIR}/game/ai_dmnet.c
    ${BASEQL_DIR}/game/ai_dmq3.c
    ${BASEQL_DIR}/game/ai_main.c
    ${BASEQL_DIR}/game/ai_team.c
    ${BASEQL_DIR}/game/ai_vcmd.c
    ${BASEQL_BG_SOURCES}
    ${BASEQL_DIR}/game/g_active.c
    ${BASEQL_DIR}/game/g_arenas.c
    ${BASEQL_DIR}/game/g_bot.c
    ${BASEQL_DIR}/game/g_client.c
    ${BASEQL_DIR}/game/g_cmds.c
    ${BASEQL_DIR}/game/g_combat.c
    ${BASEQL_DIR}/game/g_factory.c
    ${BASEQL_DIR}/game/g_items.c
    ${BASEQL_DIR}/game/g_json.c
    ${BASEQL_DIR}/game/g_mem.c
    ${BASEQL_DIR}/game/g_misc.c
    ${BASEQL_DIR}/game/g_missile.c
    ${BASEQL_DIR}/game/g_mover.c
    ${BASEQL_DIR}/game/g_session.c
    ${BASEQL_DIR}/game/g_spawn.c
    ${BASEQL_DIR}/game/g_svcmds.c
    ${BASEQL_DIR}/game/g_target.c
    ${BASEQL_DIR}/game/g_team.c
    ${BASEQL_DIR}/game/g_trigger.c
    ${BASEQL_DIR}/game/g_tuning.c
    ${BASEQL_DIR}/game/g_utils.c
    ${BASEQL_DIR}/game/g_weapon.c
    ${GAME_MODULE_SHARED_SOURCES}
)

set(BASEQL_UI_SOURCES
    ${BASEQL_DIR}/ui/ui_main.c
    ${BASEQL_DIR}/ui/ui_atoms.c
    ${BASEQL_DIR}/ui/ui_gameinfo.c
    ${BASEQL_DIR}/ui/ui_players.c
    ${BASEQL_DIR}/ui/ui_shared.c
    ${BASEQL_DIR}/game/bg_misc.c
    ${BASEQL_DIR}/game/bg_lib.c
    ${GAME_MODULE_SHARED_SOURCES}
)

# baseql always builds the Team Arena code paths
set(BASEQL_DEFINITIONS MISSIONPACK)

if(BUILD_GAME_LIBRARIES)
    set(CGAME_MODULE_BINARY_BASEQL ${CGAME_MODULE}_${BASEQL})
    set(GAME_MODULE_BINARY_BASEQL ${GAME_MODULE}_${BASEQL})
    set(UI_MODULE_BINARY_BASEQL ${UI_MODULE}_${BASEQL})

    add_library(                ${CGAME_MODULE_BINARY_BASEQL} SHARED ${BASEQL_CGAME_SOURCES} ${BASEQL_DIR}/cgame/cg_syscalls.c)
    target_compile_definitions( ${CGAME_MODULE_BINARY_BASEQL} PRIVATE CGAME ${BASEQL_DEFINITIONS})
    target_link_libraries(      ${CGAME_MODULE_BINARY_BASEQL} PRIVATE ${COMMON_LIBRARIES})
    set_target_properties(      ${CGAME_MODULE_BINARY_BASEQL} PROPERTIES OUTPUT_NAME ${CGAME_MODULE})
    set_output_dirs(            ${CGAME_MODULE_BINARY_BASEQL} SUBDIRECTORY ${BASEQL})

    add_library(                ${GAME_MODULE_BINARY_BASEQL} SHARED ${BASEQL_GAME_SOURCES} ${BASEQL_DIR}/game/g_syscalls.c)
    target_compile_definitions( ${GAME_MODULE_BINARY_BASEQL} PRIVATE QAGAME ${BASEQL_DEFINITIONS})
    target_link_libraries(      ${GAME_MODULE_BINARY_BASEQL} PRIVATE ${COMMON_LIBRARIES})
    set_target_properties(      ${GAME_MODULE_BINARY_BASEQL} PROPERTIES OUTPUT_NAME ${GAME_MODULE})
    set_output_dirs(            ${GAME_MODULE_BINARY_BASEQL} SUBDIRECTORY ${BASEQL})

    add_library(                ${UI_MODULE_BINARY_BASEQL} SHARED ${BASEQL_UI_SOURCES} ${BASEQL_DIR}/ui/ui_syscalls.c)
    target_compile_definitions( ${UI_MODULE_BINARY_BASEQL} PRIVATE UI ${BASEQL_DEFINITIONS})
    target_link_libraries(      ${UI_MODULE_BINARY_BASEQL} PRIVATE ${COMMON_LIBRARIES})
    set_target_properties(      ${UI_MODULE_BINARY_BASEQL} PROPERTIES OUTPUT_NAME ${UI_MODULE})
    set_output_dirs(            ${UI_MODULE_BINARY_BASEQL} SUBDIRECTORY ${BASEQL})
endif()

if(BUILD_GAME_QVMS)
    add_qvm(${CGAME_MODULE}qvm_${BASEQL}
        DEFINITIONS CGAME ${BASEQL_DEFINITIONS}
        OUTPUT_NAME ${CGAME_MODULE}
        OUTPUT_DIRECTORY ${BASEQL}/vm
        SOURCES ${BASEQL_CGAME_SOURCES} ${BASEQL_DIR}/cgame/cg_syscalls.asm)

    add_qvm(${GAME_MODULE}qvm_${BASEQL}
        DEFINITIONS QAGAME ${BASEQL_DEFINITIONS}
        OUTPUT_NAME ${GAME_MODULE}
        OUTPUT_DIRECTORY ${BASEQL}/vm
        SOURCES ${BASEQL_GAME_SOURCES} ${BASEQL_DIR}/game/g_syscalls.asm)

    add_qvm(${UI_MODULE}qvm_${BASEQL}
        DEFINITIONS UI ${BASEQL_DEFINITIONS}
        OUTPUT_NAME ${UI_MODULE}
        OUTPUT_DIRECTORY ${BASEQL}/vm
        SOURCES ${BASEQL_UI_SOURCES} ${BASEQL_DIR}/ui/ui_syscalls.asm)
endif()

# Data files the game module reads (factories, map pool), copied next to it
set(BASEQL_DATA_DIR ${BASEQL_DIR}/data)
file(GLOB_RECURSE BASEQL_DATA_FILES CONFIGURE_DEPENDS RELATIVE ${BASEQL_DATA_DIR} ${BASEQL_DATA_DIR}/*)
set(BASEQL_DATA_OUTPUTS "")
foreach(DATA_FILE ${BASEQL_DATA_FILES})
    set(DATA_OUTPUT ${CMAKE_BINARY_DIR}/$<CONFIG>/${BASEQL}/${DATA_FILE})
    add_custom_command(
        OUTPUT ${DATA_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E copy ${BASEQL_DATA_DIR}/${DATA_FILE} ${DATA_OUTPUT}
        DEPENDS ${BASEQL_DATA_DIR}/${DATA_FILE}
        COMMENT "Copying ${BASEQL}/${DATA_FILE}")
    list(APPEND BASEQL_DATA_OUTPUTS ${DATA_OUTPUT})

    get_filename_component(DATA_SUBDIR ${DATA_FILE} DIRECTORY)
    install(FILES ${BASEQL_DATA_DIR}/${DATA_FILE} DESTINATION ${BASEQL}/${DATA_SUBDIR})
endforeach()
add_custom_target(baseql_data ALL DEPENDS ${BASEQL_DATA_OUTPUTS})
