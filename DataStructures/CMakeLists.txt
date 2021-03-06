cmake_minimum_required(VERSION 2.8)
project(DataStructure)

set(CMAKE_C_FLAGS "-Wall -g -lm")

include_directories(.)

# Subparts delcaration
add_subdirectory(hello)
add_subdirectory("singly-linked-list")
add_subdirectory(list)
add_subdirectory(stack)
add_subdirectory(kmp)
add_subdirectory("binary-search-tree")

# Run tests
set(RESULT_FILE test.results)

add_custom_command(
    OUTPUT ${RESULT_FILE}

    COMMAND rm -rf ${RESULT_FILE}

    # TODO make this shorter...
    COMMAND echo "Running hello..." >> ${RESULT_FILE}
    COMMAND hello/hello_test >> ${RESULT_FILE}
    COMMAND echo -e >> ${RESULT_FILE}
    COMMAND echo "hello finished..." >> ${RESULT_FILE}

    COMMAND echo "Running singly linked list..." >> ${RESULT_FILE}
    COMMAND "singly-linked-list/singly-linked-list_test" >> ${RESULT_FILE}
    COMMAND echo -e >> ${RESULT_FILE}
    COMMAND echo "singly linked list finished..." >> ${RESULT_FILE}

    COMMAND echo "Running list..." >> ${RESULT_FILE}
    COMMAND "list/list_test" >> ${RESULT_FILE}
    COMMAND echo -e >> ${RESULT_FILE}
    COMMAND echo "list finished..." >> ${RESULT_FILE}
    
    COMMAND echo "Running stack brackets..." >> ${RESULT_FILE}
    COMMAND "stack/brackets_test" >> ${RESULT_FILE}
    COMMAND echo -e >> ${RESULT_FILE}
    COMMAND echo "stack brackets finished..." >> ${RESULT_FILE}

    COMMAND echo "Running stack binary expression..." >> ${RESULT_FILE}
    COMMAND "stack/binary_expression_test" >> ${RESULT_FILE}
    COMMAND echo -e >> ${RESULT_FILE}
    COMMAND echo "stack binary expression finished..." >> ${RESULT_FILE}

    COMMAND echo "Running kmp..." >> ${RESULT_FILE}
    COMMAND "kmp/kmp_test" >> ${RESULT_FILE}
    COMMAND echo -e >> ${RESULT_FILE}
    COMMAND echo "kmp finished..." >> ${RESULT_FILE}

    COMMAND echo "Running bst..." >> ${RESULT_FILE}
    COMMAND "binary-search-tree/bst_test" >> ${RESULT_FILE}
    COMMAND echo -e >> ${RESULT_FILE}
    COMMAND echo "bst finished..." >> ${RESULT_FILE}

    DEPENDS hello/hello_test
    DEPENDS "singly-linked-list/singly-linked-list_test"
    DEPENDS "list/list_test"
    DEPENDS "stack/brackets_test"
    DEPENDS "stack/binary_expression_test"
    DEPENDS "kmp/kmp_test"
)

add_custom_target(
    test ALL
    DEPENDS ${RESULT_FILE}
    DEPENDS hello/hello_test
    DEPENDS "singly-linked-list/singly-linked-list_test"
    DEPENDS "list/list_test"
    DEPENDS "stack/brackets_test"
    DEPENDS "stack/binary_expression_test"
    DEPENDS "kmp/kmp_test"
    DEPENDS "binary-search-tree/bst_test"
)
