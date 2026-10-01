# One test driver per component: <component>.t.cpp builds <component>.t and
# registers it with CTest under the component's name.
function(journalq_test component)
  add_executable(${component}.t ${component}.t.cpp)
  target_link_libraries(${component}.t PRIVATE journalq)
  target_compile_options(${component}.t PRIVATE -Wall -Wextra)
  add_test(NAME ${component} COMMAND ${component}.t)
endfunction()

journalq_test(bigendian)
journalq_test(crc32c)
journalq_test(protocol)
