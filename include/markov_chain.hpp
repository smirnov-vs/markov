//
// Created by vladislav on 23.10.16.
//

#ifndef MARKOV_MARKOVCHAIN_HPP
#define MARKOV_MARKOVCHAIN_HPP

#include <string>
#include <vector>
#include <set>
#include <map>
#include <unordered_map>
#include <memory>

class MarkovChain {

    static const size_t BUCKET_SIZE = 10;

    struct Base; struct Node;
    typedef std::shared_ptr<Base> BasePtr;
    typedef std::shared_ptr<Node> NodePtr;
    typedef std::weak_ptr<Base> BaseWPtr;
    typedef std::weak_ptr<Node> NodeWPtr;

    friend std::wostream &operator<<(std::wostream&, const MarkovChain::Node&);
    friend std::wistream &operator>>(std::wistream&, MarkovChain::Node&);
    friend std::wostream &operator<<(std::wostream&, const MarkovChain::Base&);

    static std::function<size_t (const NodeWPtr&)> nodeHash;
    static std::function<bool (const NodeWPtr&, const NodeWPtr&)> nodeEqual;
    static std::function<bool (const NodeWPtr&, const NodeWPtr&)> nodeLess;
    static std::function<size_t (const BaseWPtr&)> baseHash;
    static std::function<bool (const BaseWPtr&, const BaseWPtr&)> baseEqual;
    static std::function<bool (const BaseWPtr&, const BaseWPtr&)> baseLess;
    static std::function<size_t (const std::vector<std::wstring>&)> vectorHash;
    static std::function<bool (const std::vector<std::wstring>&, const std::vector<std::wstring>&)> vectorEqual;

    static bool getWord(std::wifstream& fs, std::wstring& word);
    static std::wstring removePunctuation(const std::wstring& text);
    static std::wstring toLowerCase(const std::wstring& text);

    static void readBase(std::wifstream& fs, MarkovChain& markovChain);

    struct Base {
        static long currentId;

        long id;
        std::vector<NodeWPtr> nodes;
        std::unordered_map<NodeWPtr, long, decltype(nodeHash), decltype(nodeEqual)> childToCount;
        std::unordered_map<NodeWPtr, BaseWPtr, decltype(nodeHash), decltype(nodeEqual)> childToBase;

        Base();
    };

    struct Node {
        static long currentId;

        long id;
        std::wstring value;

        Node();
        Node(const std::wstring& value);
    };

    int degree;

    std::unordered_map<BaseWPtr, NodeWPtr, decltype(baseHash), decltype(baseEqual)> baseToNode;
    std::unordered_map<std::vector<std::wstring>, BaseWPtr, decltype(vectorHash), decltype(vectorEqual)> wordsToBase;

    std::map<long, NodePtr> nodes;
    std::map<long, BasePtr> bases;
    std::set<BaseWPtr, decltype(baseLess)> basesWeak;
    std::set<NodeWPtr, decltype(nodeLess)> nodesWeak;

    MarkovChain();

public:
    static MarkovChain fromTextFile(const std::string& filename, int n);
    static MarkovChain fromSavedFile(const std::string& filename);

    NodePtr createNode(const std::wstring& word);
    void save(const std::string& filename);
    std::wstring next(const std::vector<std::wstring>& base, int n);
    int getDegree();
};

#endif //MARKOV_MARKOVCHAIN_HPP
