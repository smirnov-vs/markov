//
// Created by vladislav on 23.10.16.
//

#include "markov_chain.hpp"
#include <algorithm>
#include <sstream>
#include <fstream>
#include "hash_combine.hpp"

std::function<size_t (const std::vector<std::wstring>&)>
        MarkovChain::vectorHash = [](const std::vector<std::wstring>& v) {
            size_t size = v.size();
            size_t seed = 0;
            for (size_t i = 0; i < size; ++i)
                ::hash_combine(seed, v[i], i);
            return seed;
        };

std::function<bool (const std::vector<std::wstring>&, const std::vector<std::wstring>&)>
        MarkovChain::vectorEqual = [](const std::vector<std::wstring>& a, const std::vector<std::wstring>& b) {
            if (a.size() != b.size()) return false;
            size_t size = a.size();
            for (size_t i = 0; i < size; ++i) {
                if (a[i] != b[i]) return false;
            }
            return true;
        };

MarkovChain::MarkovChain()
        : baseToNode(BUCKET_SIZE, baseHash, baseEqual),
          wordsToBase(BUCKET_SIZE, vectorHash, vectorEqual),
          basesWeak(baseLess), nodesWeak(nodeLess) {}

MarkovChain MarkovChain::fromTextFile(const std::string& filename, int n) {
    MarkovChain chain;
    chain.degree = n;

    std::wifstream fs(filename);
    if (!fs) {
        fs.close();
        throw std::invalid_argument("File not accessible");
    }

    /*add first base*/
    std::wstring word;
    BasePtr base = std::make_shared<Base>();
    chain.bases[base->id] = base;
    std::vector<std::wstring> wordsBase;
    for (int i = 0; i < n; ++i) {
        if (!getWord(fs, word)) {
            throw std::runtime_error("Can't make first base");
        }
        NodePtr nodePtr = chain.createNode(word);
        base->nodes.push_back(nodePtr);
        wordsBase.push_back(word);
    }
    chain.wordsToBase[wordsBase] = base;
    chain.basesWeak.insert(base);

    if (!getWord(fs, word)) {
        throw std::runtime_error("Can't make first Node-Base relation");
    }
    NodePtr prev = chain.createNode(word);
    chain.baseToNode[base] = prev;
    ++base->childToCount[prev];
    /*first base end*/

    BasePtr newBase;
    while (getWord(fs, word)) {
        NodePtr node = chain.createNode(word);
        newBase = std::make_shared<Base>();
        for (size_t i = 1; i < base->nodes.size(); ++i) {
            newBase->nodes.push_back(base->nodes[i]);
        }
        newBase->nodes.push_back(prev);

        NodeWPtr& wNode = chain.baseToNode[newBase];
        if (wNode.lock() == nullptr) {
            wNode = node;
            chain.bases[newBase->id] = newBase;
            chain.basesWeak.insert(newBase);
            wordsBase.clear();
            for (const auto& w : newBase->nodes)
                wordsBase.push_back(w.lock()->value);
            chain.wordsToBase[wordsBase] = newBase;
        } else {
            newBase = chain.basesWeak.find(newBase)->lock();
            --Base::currentId;
        }

        ++newBase->childToCount[node];
        base->childToBase[prev] = newBase;
        prev = node;
        base = newBase;
    }

    newBase = std::make_shared<Base>();
    for (size_t i = 1; i < base->nodes.size(); ++i) {
        newBase->nodes.push_back(base->nodes[i]);
    }
    newBase->nodes.push_back(prev);
    auto baseIt = chain.basesWeak.find(newBase);
    if (baseIt != chain.basesWeak.end()) {
        BasePtr exBase = chain.basesWeak.find(newBase)->lock();
        newBase = exBase;
        base->childToBase[prev] = newBase;
    }

    return chain;
}

MarkovChain MarkovChain::fromSavedFile(const std::string& filename) {
    MarkovChain chain;
    std::wifstream fs(filename);
    if (!fs) {
        fs.close();
        throw std::invalid_argument("File not accessible");
    }

    fs >> chain.degree;

    size_t nodesSize, basesSize;
    fs >> nodesSize >> basesSize;

    for (size_t i = 0; i < nodesSize && fs; ++i) {
        NodePtr node = std::make_shared<Node>();
        fs >> *node;
        chain.nodes[node->id] = node;
    }

    for (size_t i = 0; i < basesSize; ++i)
        chain.bases[i] = std::make_shared<Base>();

    for (size_t i = 0; i < basesSize && fs; ++i) {
        readBase(fs, chain);
    }

    size_t bnSize;
    fs >> bnSize;
    for (size_t i = 0; i < basesSize && fs; ++i) {
        long bID, nID;
        fs >> bID >> nID;
        BasePtr base = chain.bases[bID];
        NodePtr node = chain.nodes[nID];
        chain.baseToNode[base] = node;

        std::vector<std::wstring> wds;
        for (const auto& n : base->nodes)
            wds.push_back(n.lock()->value);
        chain.wordsToBase[wds] = base;
    }

    if (!fs)
        throw std::runtime_error("Bad file");

    fs.close();
    return chain;
}

void MarkovChain::save(const std::string &filename) {
    std::wofstream fs(filename);
    if (!fs) {
        fs.close();
        throw std::invalid_argument(filename);
    }

    fs << degree << '\n';
    fs << nodes.size() << ' ' << bases.size() << '\n';

    for (const auto& n : nodes)
        fs << *n.second;
    for (const auto& b : bases)
        fs << *b.second;

    fs << baseToNode.size() << '\n';
    for (const auto& p : baseToNode)
        fs << p.first.lock()->id << ' ' << p.second.lock()->id << '\n';

    fs << std::flush;
    fs.close();
}

std::wstring MarkovChain::next(const std::vector<std::wstring> &base, int n) {
    std::vector<std::wstring> lowerBase;
    for (auto& s : base)
        lowerBase.push_back(toLowerCase(s));

    std::wstring result;
    for (const auto& b : base)
        result += b + L' ';
    result.pop_back();

    BasePtr basePtr =  wordsToBase[lowerBase].lock();
    if (basePtr == nullptr)
        return result;

    for(int i = 0; i < n; ++i) {
        long maxCount = 0;
        std::vector<NodeWPtr> maximumNodes;
        for (const auto& cc : basePtr->childToCount) {
            long nCount = cc.second;
            if (nCount >= maxCount) {
                if (nCount > maxCount) {
                    maximumNodes.clear();
                    maxCount = nCount;
                }
                maximumNodes.push_back(cc.first);
            }
        }

        size_t idx = 0;
        if (maximumNodes.size() > 1) {
            idx = rand() % maximumNodes.size();
        }

        NodePtr node = maximumNodes[idx].lock();

        result += L' ';
        result += node->value;

        basePtr = basePtr->childToBase[node].lock();
        if (basePtr == nullptr) break;


    }

    return result;
}

int MarkovChain::getDegree() {
    return degree;
}

MarkovChain::NodePtr MarkovChain::createNode(const std::wstring& word) {
    NodePtr node = std::make_shared<Node>(word);
    auto it = nodesWeak.find(node);
    if (it == nodesWeak.end()) {
        nodesWeak.insert(node);
        nodes[node->id] = node;
    } else {
        node = it->lock();
    }
    return node;
}

void MarkovChain::readBase(std::wifstream &fs, MarkovChain& markovChain) {
    long id;
    fs >> id;
    BasePtr& base = markovChain.bases[id];

    for (size_t i = 0; i < markovChain.degree; ++i) {
        long nId;
        fs >> nId;
        base->nodes.push_back(markovChain.nodes[nId]);
    }

    size_t ccSize;
    fs >> ccSize;
    for (size_t i = 0; i < ccSize; ++i) {
        long nId, count;
        fs >> nId >> count;
        base->childToCount[markovChain.nodes[nId]] = count;
    }

    size_t cbSize;
    fs >> cbSize;
    for (size_t i = 0; i < cbSize; ++i) {
        long nId, bId;
        fs >> nId >> bId;
        base->childToBase[markovChain.nodes[nId]] = markovChain.bases[bId];
    }
}

bool MarkovChain::getWord(std::wifstream &fs, std::wstring& word) {
    word = L"";
    while (fs && word == L"") {
        fs >> word;
        word = toLowerCase(removePunctuation(word));
    }

    return !word.empty();
}

std::wstring MarkovChain::removePunctuation(const std::wstring &text) {
    std::wstring result;
    const std::locale loc;
    std::remove_copy_if(text.begin(), text.end(), std::back_inserter(result),
                        [&] (auto c) { return std::ispunct(c, loc); }
    );
    return result;
}

std::wstring MarkovChain::toLowerCase(const std::wstring &text) {
    std::wstring result(text.size(), 0);
    const std::locale loc;
    std::transform(text.begin(), text.end(), result.begin(),
                   [&] (auto c) { return std::tolower(c, loc); });
    return result;
}