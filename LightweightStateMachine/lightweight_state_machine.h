#ifndef LIGHTWEIGHT_STATE_MACHINE_H
#define LIGHTWEIGHT_STATE_MACHINE_H

#include <functional>
#include <map>
#include <type_traits>
#include <vector>

namespace lightweight_state_machine
{
    typedef std::function<void()> enter_func;
    typedef std::function<void()> leave_func;
    typedef std::function<bool()> guard_func;

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

    public:
        transition(const state &from, const state &to, const event_type &on_this_event)
            : from_(from), to_(to), on_this_event_(on_this_event)
        {
        }

        self_type& operator/ (guard_func g) { guard_ = g; return *this; }

        const state& from() const { return from_; }
        const state& to() const { return to_; }
        const event_type& get_event() const { return on_this_event_; }
        bool check_guard() const { return !guard_ || guard_(); }

    private:
        const state &from_, &to_;
        event_type on_this_event_;
        guard_func guard_;
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
       machine() : current_state_(nullptr) {}

        template <typename TransitionEventType>
        self_type& operator<<(transition<TransitionEventType> &t) 
        {
            static_assert(std::is_same_v<TransitionEventType, event_type>, "You can't add a transition with an event type different from the machine");
            transitions_.insert(std::make_pair(std::make_pair(t.get_event(), &t.from()), t));
            return *this;
        }

        void start(const state *init) { current_state_ = init; current_state_->enter(); }
        void stop()                   { if (current_state_ != nullptr) current_state_->leave(); }

        void notify(const event_type &event)
        {
            transitions::key_type key(std::make_pair(event, current_state_));
            auto range = transitions_.equal_range(key);
            for (auto it = range.first; it != range.second; ++it)
            {
                if (it->second.check_guard())
                {
                    if (current_state_ != nullptr) current_state_->leave();
                    current_state_ = &it->second.to();
                    if (current_state_ != nullptr) current_state_->enter();
                    break;
                }
            }
        }

    private:
        const state* current_state_;
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
