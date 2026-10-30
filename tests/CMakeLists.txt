function(usnip_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE usnip_core Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

usnip_add_test(tst_crypto)
usnip_add_test(tst_exchange)
usnip_add_test(tst_snippetstore)
