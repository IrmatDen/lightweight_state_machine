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
    bool first_state_left     = false,
         second_state_entered = false;

    const lsm::state init  = lsm::state().on_leave([&first_state_left]    () { first_state_left = true;     }),
                     final = lsm::state().on_enter([&second_state_entered]() { second_state_entered = true; });

    lsm::machine<char> sm;
	sm << init
       << (init | final) ['q'];

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
       << (init | final) ['q'];

    sm.start();

    EXPECT_TRUE(first_state_left);
    EXPECT_TRUE(second_state_entered);
}

TEST(lightweight_state_machine_test, guarded_transition_accepted) {
    bool guard_executed = false,
         final_state_entered = false;

    const lsm::state init  = lsm::state(),
                     final = lsm::state().on_enter([&final_state_entered]() { final_state_entered = true; });

    lsm::machine<char> sm;
    sm << init
	   << (init | final) ['q'] ([&guard_executed]() { guard_executed = true; return true; });

    sm.start();
    sm.notify('q');

    EXPECT_TRUE(guard_executed);
    EXPECT_TRUE(final_state_entered);
}

TEST(lightweight_state_machine_test, guarded_transition_denied) {
    bool guard_executed = false,
         final_state_entered = false;

    const lsm::state init  = lsm::state(),
                     final = lsm::state().on_enter([&final_state_entered]() { final_state_entered = true; });

    lsm::machine<char> sm;
    sm << init
	   << (init | final) ['q'] ([&guard_executed]() { guard_executed = true; return false; });

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
       << (init | final_invalid) ['q'] ([]() { return false; })
       << (init | final_valid)   ['q'] ([]() { return true ; });

    sm.start();
    sm.notify('q');

    EXPECT_TRUE(valid_final_state);
    EXPECT_FALSE(invalid_final_state);
}

TEST(lightweight_state_machine_test, transition_with_one_action) {
    bool action_called = false;
    lsm::machine<char> sm;

    const lsm::state init  = lsm::state(),
                     final = lsm::state();

    sm << init
       << (init | final) ['q'] / [&action_called] () { action_called = true; };

    sm.start();
    sm.notify('q');

    EXPECT_TRUE(action_called);
}

TEST(lightweight_state_machine_test, transition_with_multiple_actions) {
    bool action_1_called = false,
         action_2_called = false;
    lsm::machine<char> sm;

    const lsm::state init  = lsm::state(),
                     final = lsm::state();

    sm << init
       << (init | final) ['q'] / [&action_1_called] () { action_1_called = true; }
                               / [&action_2_called] () { action_2_called = true; };

    sm.start();
    sm.notify('q');

    EXPECT_TRUE(action_1_called);
    EXPECT_TRUE(action_2_called);
}

TEST(lightweight_state_machine_test, multi_state_with_guards_tests) {
    // Taken from example on wikipedia: https://en.wikipedia.org/wiki/UML_state_machine#/media/File:UML_state_machine_Fig2.png
    /* Sum-up if image is unavailable:
     * - "keyboard" emulator state machine that stops running after 1000 keys have been pressed
     * - keyboard can be caps locked (doesn't count as a key press)
     * - 2 states: default (here called "standard" to avoid keyword conflict) and caps_locked
     * - states are plugged to themselves and to the final state (implemented as a 'broken' state here)
     * - transitions are guarded by the total number of keys pressed
     */
    
    enum class Event { key_pressed, caps_lock_pressed };
    unsigned int remaining_keys_count = 1000;
    bool is_broken = false;

    lsm::machine<Event> sm;

    const lsm::state standard     = lsm::state(),
                      caps_locked = lsm::state(),
                      broken      = lsm::state().on_enter([&is_broken, &sm]() { is_broken = true; sm.stop(); });

    auto too_many_keys_pressed = [&remaining_keys_count]() { return remaining_keys_count == 0; };
    // Waiting for wider acceptance of std::not_fn...
    auto keys_remaining = [&remaining_keys_count]() { return remaining_keys_count > 0; };

    sm << standard
       << (standard    | caps_locked)  [Event::caps_lock_pressed]
       << (caps_locked | standard)     [Event::caps_lock_pressed]
       << (standard    | standard)     [Event::key_pressed] (keys_remaining) / [&remaining_keys_count]() { remaining_keys_count--; }
       << (caps_locked | caps_locked)  [Event::key_pressed] (keys_remaining) / [&remaining_keys_count]() { remaining_keys_count--; }
       << (standard    | broken)       [Event::key_pressed] (too_many_keys_pressed)
       << (caps_locked | broken)       [Event::key_pressed] (too_many_keys_pressed);

    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<> distributor(0, 10); // 10% chances of pressing caps lock
    sm.start();

    // Used as a guard against looping indefinitely
    size_t loop_exec_count = 0;
    while (sm.is_running() && loop_exec_count < 1001)
    {
        const Event e = distributor(generator) == 0 ? Event::caps_lock_pressed : Event::key_pressed;
        sm.notify(e);

        if (e == Event::key_pressed) {
            loop_exec_count++;
        }
    }

    EXPECT_EQ(loop_exec_count, 1001);
    EXPECT_EQ(remaining_keys_count, 0);
    EXPECT_TRUE(is_broken);
}
