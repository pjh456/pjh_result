# One executable per example; each doubles as an integration-level smoke check.
foreach(ex IN ITEMS basic pipeline error_prop void_result custom_error
                    option_basic option_chain option_interop)
    add_executable(example_${ex} ${ex}.cpp)
    target_link_libraries(example_${ex} PRIVATE pjh_result)
endforeach()
