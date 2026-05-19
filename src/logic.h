#include <cstddef>
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

/* =========================
   Internal representations
   ========================= */
using InternedString = size_t;

struct Formula {
    virtual void instantiate(const InternedString from, const InternedString to) = 0;
};

class InternedNames {
    public:
        //InternedNames() = default;
        //InternedNames(std::vector<string> &&names) : names(names) {
        //    for (size_t i = 0; i < names.size(); i++) {
        //        inv_map[names[i]] = i;
        //    }
        //}

        const std::string &operator[](const InternedString idx) const {
            return names[idx];
        }
        const size_t &operator[](const std::string &str) const {
            return inv_map.at(str);
        }
        void add(const std::string &str) {
            inv_map[str] = names.size();
            names.push_back(str);
        }
        const std::vector<std::string> &get_names() const {return names;}
        std::vector<std::string> &get_names() {return names;}
        bool contains(const std::string &str) {
            return inv_map.contains(str);
        }
    private:
        std::vector<std::string> names;
        std::unordered_map<std::string, InternedString> inv_map;
};

class PredicateDefinitions : public InternedNames {
    public:
        void add(const std::string &str, size_t arity = 0) {
            InternedNames::add(str);
            arities.push_back(arity);
        }
        size_t get_arity(const InternedString predicate) const {
            return arities[predicate];
        }
    private:
        std::vector<size_t> arities;
};

class ActionDefinitions : public InternedNames {
    public:
        void add(const std::string &str, size_t arity, std::shared_ptr<Formula> precondition, std::vector<Formula> add_list, std::vector<Formula> del_list) {
            InternedNames::add(str);
            arities.push_back(arity);
            preconditions.push_back(precondition);
            add_lists.push_back(add_list);
        }
        size_t get_arity(const InternedString predicate) const {
            return arities[predicate];
        }
    private:
        std::vector<size_t> arities;
        std::vector<std::shared_ptr<Formula>> preconditions;
        std::vector<std::vector<std::shared_ptr<Formula>>> add_lists;
        std::vector<std::vector<std::shared_ptr<Formula>>> del_lists;
};

// TODO:pack
struct Term {
    InternedString name;
    bool free{false};

    void instantiate(const InternedString from, const InternedString to) {
        if (free && name == from) name = to;
    };
};

struct PredicateInstance : virtual Formula {
    InternedString name;
    std::vector<Term> arguments;

    virtual void instantiate(const InternedString from, const InternedString to) {
        for (Term &t : arguments) {
            t.instantiate(from, to);
        }
    };
};

struct And : virtual Formula {
    std::vector<Formula> operands;
    virtual void instantiate(const InternedString from, const InternedString to) {
        for (Formula &f : operands) {
            f.instantiate(from, to);
        }
    };
};

struct Or : virtual Formula {
    std::vector<Formula> operands;
    virtual void instantiate(const InternedString from, const InternedString to) {
        for (Formula &f : operands) {
            f.instantiate(from, to);
        }
    };
};

struct Not : virtual Formula {
    std::unique_ptr<Formula> operand;
    virtual void instantiate(const InternedString from, const InternedString to) {
        operand->instantiate(from, to);
    };
};
