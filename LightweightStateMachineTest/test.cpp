#include "pch.h"

#include "../LightweightStateMachine/lightweight_state_machine.h"
#include "test.h"

namespace lsm = lightweight_state_machine;

TEST(lightweight_state_machine_test, test_empty_state) {
    lsm::state s;

    lsm::machine<char> sm;
	sm << s;

    sm.start();
    sm.stop();

    // Notifies that no exceptions were raised
    EXPECT_TRUE(true);
}

TEST(lightweight_state_machine_test, single_state_enter_is_called) {
    const int golden = 42;
    int result = 0;

    auto s = lsm::state().on_enter([&]() { result = golden; });

    lsm::machine<char> sm;
	sm << s;
    sm.start();

    EXPECT_EQ(result, golden);
}

TEST(lightweight_state_machine_test, single_state_leave_is_called) {
    bool leave_called = false;

    auto s = lsm::state().on_leave([&]() { leave_called = true; });

    lsm::machine<char> sm;
	sm << s;

    sm.start();
    // Force the state to leave.
    sm.stop();

    EXPECT_TRUE(leave_called);
}

TEST(lightweight_state_machine_test, external_transition_test) {
    bool first_state_left = false,
         second_state_entered = false;

    const lsm::state init  = lsm::state().on_leave([&first_state_left]    () { first_state_left = true;     }),
                     final = lsm::state().on_enter([&second_state_entered]() { second_state_entered = true; });

    lsm::machine<char> sm;
	sm << init
       << (init | final)['q'];

    sm.start();
    sm.notify('q');

    EXPECT_TRUE(first_state_left);
    EXPECT_TRUE(second_state_entered);
}

TEST(lightweight_state_machine_test, transition_from_state) {
    bool first_state_left = false,
        second_state_entered = false;
    lsm::machine<char> sm;

    const lsm::state init  = lsm::state().on_enter([&sm]() { sm.notify('q'); })
                                         .on_leave([&first_state_left]() { first_state_left = true;     }),
                     final = lsm::state().on_enter([&second_state_entered]() { second_state_entered = true; });

    sm << init
        << (init | final)['q'];

    sm.start();

    EXPECT_TRUE(first_state_left);
    EXPECT_TRUE(second_state_entered);
}

TEST(lightweight_state_machine_test, guarded_transition_accepted) {
    bool guard_executed = false,
         final_state_entered = false;

    const lsm::state init = lsm::state(),
                     final = lsm::state().on_enter([&final_state_entered]() { final_state_entered = true; });

    lsm::machine<char> sm;
    sm << init
	   << (init | final)['q'] / [&guard_executed]() { guard_executed = true; return true; };

    sm.start();
    sm.notify('q');

    EXPECT_TRUE(guard_executed);
    EXPECT_TRUE(final_state_entered);
}

TEST(lightweight_state_machine_test, guarded_transition_denied) {
    bool guard_executed = false,
        final_state_entered = false;

    const lsm::state init = lsm::state(),
                     final = lsm::state().on_enter([&final_state_entered]() { final_state_entered = true; });

    lsm::machine<char> sm;
    sm << init
	   << (init | final)['q'] / [&guard_executed]() { guard_executed = true; return false; };

    sm.start();
    sm.notify('q');

    EXPECT_TRUE(guard_executed);
    EXPECT_FALSE(final_state_entered);
}

TEST(lightweight_state_machine_test, shared_trigger) {
    bool invalid_final_state = false,
         valid_final_state = false;

    const lsm::state init          = lsm::state(),
                     final_invalid = lsm::state().on_enter([&invalid_final_state]() { invalid_final_state = true; }),
                     final_valid   = lsm::state().on_enter([&valid_final_state]  () { valid_final_state   = true; });

    lsm::machine<char> sm;
    sm << init
       << (init | final_invalid) ['q'] / []() { return false; }
       << (init | final_valid)   ['q'] / []() { return true; };

    sm.start();
    sm.notify('q');

    EXPECT_TRUE(valid_final_state);
    EXPECT_FALSE(invalid_final_state);
}
