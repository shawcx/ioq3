# Writes a Make-style depfile listing the headers one QVM source includes, so
# the source is recompiled when one of them changes. q3cpp -M prints
# "<object>.$O: <header>" for each include, mixed into the preprocessed output.
#
# Arguments: Q3CPP, FLAGS (list), SOURCE, TARGET, DEPFILE

execute_process(
    COMMAND ${Q3CPP} -M ${FLAGS} ${SOURCE}
    OUTPUT_VARIABLE OUTPUT
    ERROR_QUIET)

string(REGEX MATCHALL "\\$O: [^\n]+" LINES "${OUTPUT}")

set(HEADERS "")
foreach(LINE IN LISTS LINES)
    string(SUBSTRING "${LINE}" 4 -1 HEADER)
    string(REPLACE " " "\\ " HEADER "${HEADER}")
    list(APPEND HEADERS "${HEADER}")
endforeach()
list(REMOVE_DUPLICATES HEADERS)
list(JOIN HEADERS " " HEADERS)

file(WRITE ${DEPFILE} "${TARGET}: ${HEADERS}\n")
