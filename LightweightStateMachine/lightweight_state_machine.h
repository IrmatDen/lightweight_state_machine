#ifndef LIGHTWEIGHT_STATE_MACHINE_H
#define LIGHTWEIGHT_STATE_MACHINE_H

#include <cassert>
#include <functional>
#include <map>
#include <type_traits>
#include <vector>

namespace lightweight_state_machine
{
    typedef std::function<void()> enter_func;
    typedef std::function<void()> leave_func;
    typedef std::function<bool()> guard_func;
    typedef std::function<void()> action_func;

    class state
    {
    public:
        state() = default;
        state(const state&) = default;
        state& operator=(const state&) = default;
        ~state() = default;

        state& on_enter(enter_func e) { on_enter_ = e; return *this; }
        state& on_leave(leave_func l) { on_leave_ = l; return *this; }

        void enter() const { if (on_enter_) on_enter_(); }
        void leave() const { if (on_leave_) on_leave_(); }

    private:
        enter_func on_enter_;
        leave_func on_leave_;
    };

    template <typename Event>
    class transition
    {
    public:
        typedef Event event_type;
        typedef transition<event_type> self_type;
        typedef std::vector<action_func> actions;

    public:
        transition(const state &from, const state &to, const event_type &on_this_event)
            : from_(from), to_(to), on_this_event_(on_this_event)
        {
        }

        self_type& operator() (guard_func g) { guard_ = std::move(g); return *this; }
        self_type& operator/ (action_func a) { actions_.push_back(std::move(a)); return *this; }

        const state& from() const { return from_; }
        const state& to() const { return to_; }
        const event_type& get_event() const { return on_this_event_; }
        bool check_guard() const { return !guard_ || guard_(); }
        void invoke_actions() const { for(auto &a : actions_) { a(); } }

    private:
        const state &from_, &to_;
        event_type on_this_event_;
        guard_func guard_;
        actions actions_;

    };

    template <typename Event>
    class machine
    {
    public:
        static_assert(!std::is_same_v<Event, void>, "void-event typed machine are illegal");

    public:
        typedef Event event_type;
        typedef machine<event_type> self_type;
        typedef std::multimap<std::pair<event_type, const state*>, transition<event_type>> transitions;

   public:
       machine() : is_running_(false), initial_state_(nullptr), current_state_(nullptr) {}
	   machine(const machine&) = default;
	   machine& operator=(const machine&) = default;
	   ~machine() = default;

	   self_type& operator<<(const state &initial_state)
	   {
		   assert(initial_state_ == nullptr);
		   initial_state_ = &initial_state;
		   return *this;
	   }

        template <typename TransitionEventType>
        self_type& operator<<(transition<TransitionEventType> &t) 
        {
            static_assert(std::is_same_v<TransitionEventType, event_type>, "You can't add a transition with an event type different from the machine");
            transitions_.insert(std::make_pair(std::make_pair(t.get_event(), &t.from()), t));
            return *this;
        }

        void start()	{ current_state_ = initial_state_; is_running_ = true; current_state_->enter(); }
        void stop()     { if (current_state_ != nullptr) current_state_->leave(); is_running_ = false; }

        bool is_running() const { return is_running_; }
        bool is_stopped() const { return !is_running(); }

        void notify(const event_type &event)
        {
            transitions::key_type key(std::make_pair(event, current_state_));
            auto range = transitions_.equal_range(key);
            for (auto it = range.first; it != range.second; ++it)
            {
                if (it->second.check_guard())
                {
                    if (current_state_ != nullptr) current_state_->leave();
                    it->second.invoke_actions();
                    current_state_ = &it->second.to();
                    if (current_state_ != nullptr) current_state_->enter();
                    break;
                }
            }
        }

    private:
        bool is_running_;
		const state *initial_state_, *current_state_;
        transitions transitions_;
    };

    namespace details
    {
        struct transition_builder
        {
            transition_builder(const state &from, const state &to)
                : from_(from), to_(to)
            {
            }

            template<typename Event>
            transition<Event> operator[](const Event &on_this_event)
            {
                return transition<Event>(from_, to_, on_this_event);
            }

            const state &from_, &to_;
        };
    }
}

lightweight_state_machine::details::transition_builder operator|(const lightweight_state_machine::state &s1, const lightweight_state_machine::state &s2)
{
    return lightweight_state_machine::details::transition_builder(s1, s2);
}

#endif
