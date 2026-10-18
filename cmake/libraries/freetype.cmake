if(NOT USE_FREETYPE)
    return()
endif()

if(NOT BUILD_RENDERER_GL1 AND NOT BUILD_RENDERER_GL2)
    return()
endif()

include(utils/disable_warnings)

set(INTERNAL_FREETYPE_DIR ${SOURCE_DIR}/thirdparty/freetype-${FREETYPE_VERSION})

if(USE_INTERNAL_FREETYPE)
    # FreeType's own CMake list, less the cache API and bzip2 support; each
    # module builds from one file that includes the rest
    set(FREETYPE_SOURCES
        ${INTERNAL_FREETYPE_DIR}/src/autofit/autofit.c
        ${INTERNAL_FREETYPE_DIR}/src/base/ftbase.c
        ${INTERNAL_FREETYPE_DIR}/src/base/ftbbox.c
        ${INTERNAL_FREETYPE_DIR}/src/base/ftbdf.c
        ${INTERNAL_FREETYPE_DIR}/src/base/ftbitmap.c
        ${INTERNAL_FREETYPE_DIR}/src/base/ftcid.c
        ${INTERNAL_FREETYPE_DIR}/src/base/ftdebug.c
        ${INTERNAL_FREETYPE_DIR}/src/base/ftfstype.c
        ${INTERNAL_FREETYPE_DIR}/src/base/ftgasp.c
        ${INTERNAL_FREETYPE_DIR}/src/base/ftglyph.c
        ${INTERNAL_FREETYPE_DIR}/src/base/ftgxval.c
        ${INTERNAL_FREETYPE_DIR}/src/base/ftinit.c
        ${INTERNAL_FREETYPE_DIR}/src/base/ftmm.c
        ${INTERNAL_FREETYPE_DIR}/src/base/ftotval.c
        ${INTERNAL_FREETYPE_DIR}/src/base/ftpatent.c
        ${INTERNAL_FREETYPE_DIR}/src/base/ftpfr.c
        ${INTERNAL_FREETYPE_DIR}/src/base/ftstroke.c
        ${INTERNAL_FREETYPE_DIR}/src/base/ftsynth.c
        ${INTERNAL_FREETYPE_DIR}/src/base/ftsystem.c
        ${INTERNAL_FREETYPE_DIR}/src/base/fttype1.c
        ${INTERNAL_FREETYPE_DIR}/src/base/ftwinfnt.c
        ${INTERNAL_FREETYPE_DIR}/src/bdf/bdf.c
        ${INTERNAL_FREETYPE_DIR}/src/cff/cff.c
        ${INTERNAL_FREETYPE_DIR}/src/cid/type1cid.c
        ${INTERNAL_FREETYPE_DIR}/src/gzip/ftgzip.c
        ${INTERNAL_FREETYPE_DIR}/src/lzw/ftlzw.c
        ${INTERNAL_FREETYPE_DIR}/src/pcf/pcf.c
        ${INTERNAL_FREETYPE_DIR}/src/pfr/pfr.c
        ${INTERNAL_FREETYPE_DIR}/src/psaux/psaux.c
        ${INTERNAL_FREETYPE_DIR}/src/pshinter/pshinter.c
        ${INTERNAL_FREETYPE_DIR}/src/psnames/psnames.c
        ${INTERNAL_FREETYPE_DIR}/src/raster/raster.c
        ${INTERNAL_FREETYPE_DIR}/src/sdf/sdf.c
        ${INTERNAL_FREETYPE_DIR}/src/sfnt/sfnt.c
        ${INTERNAL_FREETYPE_DIR}/src/smooth/smooth.c
        ${INTERNAL_FREETYPE_DIR}/src/svg/svg.c
        ${INTERNAL_FREETYPE_DIR}/src/truetype/truetype.c
        ${INTERNAL_FREETYPE_DIR}/src/type1/type1.c
        ${INTERNAL_FREETYPE_DIR}/src/type42/type42.c
        ${INTERNAL_FREETYPE_DIR}/src/winfonts/winfnt.c
    )
    disable_warnings(${FREETYPE_SOURCES})
    set_source_files_properties(${FREETYPE_SOURCES} PROPERTIES
        COMPILE_DEFINITIONS FT2_BUILD_LIBRARY)
    set(FREETYPE_INCLUDE_DIRS ${INTERNAL_FREETYPE_DIR}/include)
    list(APPEND RENDERER_LIBRARY_SOURCES ${FREETYPE_SOURCES})
else()
    find_package(Freetype REQUIRED)
endif()

list(APPEND RENDERER_INCLUDE_DIRS ${FREETYPE_INCLUDE_DIRS})
list(APPEND RENDERER_LIBRARIES ${FREETYPE_LIBRARIES})
