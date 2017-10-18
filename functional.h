class Dispatcher {
public:
    explicit Dispatcher() {}
    virtual ~Dispatcher() {}

    template<typename T, typename P1>
    void AddObserver(T method, const base::Callback<void(P1)> &callback) {
        AddObserver(method, callback, true);
    }

    template<typename T, typename P1>
    void AddObserver(T method, const base::Callback<void(P1)>& callback, bool repeating) {
        auto obj = new RepeatingCallback<P1>(repeating, callback);
        dispatcher_[function_cast<void*>(method)].push_back(reinterpret_cast<void*>(obj));
    }

    template<typename T>
    void RemoveObserver(T method, const base::Closure& callback) {
        auto it = dispatcher_.find(function_cast<void*>(method));
        if (it == dispatcher_.end()) return;

        auto child = it->second.find(callback);
        if (child == it->second.end()) return;
        it->second.erase(child);
    }

    template<typename T>
    void Run(T method) {
        auto it = dispatcher_.find(function_cast<void*>(method));
        if (it == dispatcher_.end()) return;

        for (auto child = it->second; child != it->second;) {
            child->Run();
            if (child->repeating() && ++child) continue;
            child = it->second.erase(child);
        }
    }

    template<typename T, typename P1>
    void Run(T method, P1 p1) {
        auto a  = base::Bind(p1);
    }

private:
    template<typename P1>
    class RepeatingCallback {
    public:
        typedef base::Callback<void(P1)> Callback;
        RepeatingCallback(bool repeating, const Callback& user_task) : repeating_(repeating), user_task_(user_task) {}
        RepeatingCallback(const RepeatingCallback<P1>& callback) : repeating_(callback.repeating_), user_task_(callback.user_task_) {}
        virtual ~RepeatingCallback() {}

        inline bool operator==(const Callback& user_task) { return user_task_.Equals(user_task); }
        inline bool operator==(const RepeatingCallback<P1>& callback) { return user_task_.Equals(callback.user_task_); }

        bool repeating() { return repeating_; }

        void Run(P1 p1) {
            if (!user_task_.is_null()) {
                user_task_.Run();
            }
        }

    private:
        const bool repeating_;
        Callback user_task_;
    };

    typedef RepeatingCallback<void> Clusure;

    std::map<void*, std::vector<void*>> dispatcher_;
    DISALLOW_COPY_AND_ASSIGN(Dispatcher);
};
