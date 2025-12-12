// p2p_sim_threads.cpp
#include <iostream>
#include <unordered_map>
#include <vector>
#include <string>
#include <ctime>
#include <cstdlib>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <random>

using namespace std;

/* ---- Hashing & Encryption (kept and slightly reused) ---- */

class Hashing {
public:
    unsigned long long SingleHash(const string& input) const {
        unsigned long long hash = 0;
        const unsigned long long prime = 31ULL;
        unsigned long long power = 1ULL;
        for (unsigned char ch : input) {
            hash += (unsigned long long)(ch + 1) * power;
            power *= prime;
        }
        return hash;
    }

    string toBase36(unsigned long long num) const {
        const string chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        string result;
        while (num > 0) {
            int remainder = num % 36;
            result = chars[remainder] + result;
            num /= 36;
        }
        if (result.empty()) result = "0";
        return result;
    }

    unsigned long long DoubleHash(const string& a, const string& b) const {
        string input = a + "|" + b;
        unsigned long long h1 = SingleHash(input);
        string b36 = toBase36(h1);
        unsigned long long h2 = SingleHash(b36);
        unsigned long long finalHash = h1 + (h2 * 1315423911ULL);
        return finalHash;
    }

    // node key helper
    unsigned long long NodeHash(const string& nodeId) const {
        return SingleHash("NODE:" + nodeId);
    }
};

class Encryptor {
public:
    static string encrypt(const string &data) {
        int key = rand() % 10 + 1;
        string result;
        for (unsigned char c : data)
            result += char((c + key) % 128);
        result += "-" + to_string(key);
        return result;
    }

    static string decrypt(const string &data) {
        size_t pos = data.find_last_of('-');
        if (pos == string::npos) return "";
        string encPart = data.substr(0, pos);
        int key = stoi(data.substr(pos + 1));
        string result;
        for (unsigned char c : encPart)
            result += char((c - key + 128) % 128);
        return result;
    }
};

/* ---- Forward declarations ---- */
class Node;
class Network;

/* ---- Global mutex for cout (keeps prints readable) ---- */
static mutex cout_mtx;

/* ---- Node class: holds data and routes requests ---- */

class Node {
public:
    string id; // readable id e.g. "N0"
    unsigned long long nodeKey;
    atomic<bool> isAlive;

    // key -> (encryptedSocial, encryptedPassword)
    unordered_map<unsigned long long, pair<string, string>> storage;

    Network* net; // pointer to network registry
    Hashing hasher;

    // mutex to protect storage and prints that depend on storage
    mutable mutex storage_mtx;

    Node(const string& nodeId, Network* network) : id(nodeId), isAlive(true), net(network) {
        nodeKey = hasher.NodeHash(nodeId);
    }

    // Simulate receiving a store request (from any peer)
    void receiveStore(unsigned long long key, const string& socialEncrypted, const string& passEncrypted, int replication_factor);

    // Simulate receiving a get request (from any peer) - returns pair(empty,empty) if not found
    pair<string, string> receiveGet(unsigned long long key);

    // locally store (no routing) and print info (synchronized)
    void localStore(unsigned long long key, const string& socialEncrypted, const string& passEncrypted) {
        lock_guard<mutex> lg(storage_mtx);
        storage[key] = {socialEncrypted, passEncrypted};
        {
            lock_guard<mutex> cout_lk(cout_mtx);
            cout << "[Node " << id << "] Stored key " << key << " (local)." << endl;
        }
    }

    pair<string, string> localGet(unsigned long long key) {
        lock_guard<mutex> lg(storage_mtx);
        auto it = storage.find(key);
        if (it != storage.end()) return it->second;
        return {"", ""};
    }

    void fail() {
        bool expected = true;
        if (isAlive.compare_exchange_strong(expected, false)) {
            lock_guard<mutex> cout_lk(cout_mtx);
            cout << "[Node " << id << "] Simulated FAIL (node down).\n";
        }
    }

    void recover() {
        bool expected = false;
        if (isAlive.compare_exchange_strong(expected, true)) {
            lock_guard<mutex> cout_lk(cout_mtx);
            cout << "[Node " << id << "] Recovered (node up).\n";
        }
    }
};

/* ---- Network: registry / bootstrap / helper routing (does NOT act as controller) ---- */

class Network {
public:
    vector<Node*> nodes;
    mutable mutex nodes_mtx;

    // Add node to network registry (bootstrap)
    void registerNode(Node* n) {
        {
            lock_guard<mutex> lg(nodes_mtx);
            nodes.push_back(n);
        }
        sortNodesByKey();
        lock_guard<mutex> cout_lk(cout_mtx);
        cout << "[Network] Registered node " << n->id << " key=" << n->nodeKey << "\n";
    }

    // sorted list of nodes by nodeKey for consistent hashing ring
    void sortNodesByKey() {
        lock_guard<mutex> lg(nodes_mtx);
        sort(nodes.begin(), nodes.end(), [](Node* a, Node* b){
            return a->nodeKey < b->nodeKey;
        });
    }

    // find the owner/responsible node for a given key: smallest nodeKey >= key, else wrap to first
    Node* findOwnerNode(unsigned long long key) {
        lock_guard<mutex> lg(nodes_mtx);
        if (nodes.empty()) return nullptr;
        // ensure nodes are sorted (safe to call; we hold mutex)
        // find first alive node with nodeKey >= key
        for (Node* n : nodes) {
            if (!n->isAlive.load()) continue; // skip dead nodes when routing to find a live owner
            if (n->nodeKey >= key) return n;
        }
        // wrap-around: return first alive node
        for (Node* n : nodes) if (n->isAlive.load()) return n;
        return nullptr;
    }

    // return successors (alive) of a given node in the ring (excluding the node itself).
    vector<Node*> successorsOf(Node* n, int count) {
        vector<Node*> succ;
        lock_guard<mutex> lg(nodes_mtx);
        if (nodes.empty()) return succ;
        int idx = -1;
        for (size_t i = 0; i < nodes.size(); ++i) if (nodes[i] == n) { idx = (int)i; break; }
        if (idx == -1) return succ;
        int i = (idx + 1) % nodes.size();
        while ((int)succ.size() < count) {
            Node* cand = nodes[i];
            if (cand->isAlive.load()) succ.push_back(cand);
            i = (i + 1) % nodes.size();
            if (i == idx) break; // full circle
        }
        return succ;
    }

    // utility: get list of all node ids (for debugging)
    string nodeListStr() {
        ostringstream oss;
        lock_guard<mutex> lg(nodes_mtx);
        oss << "[Network nodes]";
        for (Node* n : nodes) {
            oss << " " << n->id << (n->isAlive.load() ? "" : "(down)");
        }
        return oss.str();
    }
};

/* ---- Node methods that use Network for routing & replication ---- */

void Node::receiveStore(unsigned long long key, const string& socialEncrypted, const string& passEncrypted, int replication_factor) {
    if (!isAlive.load()) {
        lock_guard<mutex> cout_lk(cout_mtx);
        cout << "[Node " << id << "] is DOWN. Can't process store request locally.\n";
        return;
    }

    // Find the owner (responsible node) for the key
    Node* owner = net->findOwnerNode(key);
    if (owner == nullptr) {
        lock_guard<mutex> cout_lk(cout_mtx);
        cout << "[Node " << id << "] No alive nodes to store data.\n";
        return;
    }

    // If I'm the owner, store locally and replicate to successors.
    if (owner == this) {
        localStore(key, socialEncrypted, passEncrypted);
        // replicate to successors
        auto succs = net->successorsOf(this, replication_factor);
        for (Node* s : succs) {
            if (s == this) continue;
            if (s->isAlive.load()) {
                // replicate asynchronously to simulate networked replication
                // but still use localStore which is thread-safe
                thread replicate_thread([s, key, socialEncrypted, passEncrypted]() {
                    // slight delay to simulate network latency
                    this_thread::sleep_for(chrono::milliseconds(20));
                    s->localStore(key, socialEncrypted, passEncrypted);
                    lock_guard<mutex> cout_lk(cout_mtx);
                    cout << "[Replication] Key " << key << " replicated to " << s->id << "\n";
                });
                replicate_thread.detach();
            } else {
                lock_guard<mutex> cout_lk(cout_mtx);
                cout << "[Node " << id << "] successor " << s->id << " is down; replication skipped.\n";
            }
        }
    } else {
        // route to owner: simulate sending message across network by calling owner's receiveStore
        {
            lock_guard<mutex> cout_lk(cout_mtx);
            cout << "[Node " << id << "] Routing STORE request for key " << key << " to owner " << owner->id << endl;
        }
        // call owner (no locks held) - owner will synchronize internally
        owner->receiveStore(key, socialEncrypted, passEncrypted, replication_factor);
    }
}

pair<string, string> Node::receiveGet(unsigned long long key) {
    if (!isAlive.load()) {
        lock_guard<mutex> cout_lk(cout_mtx);
        cout << "[Node " << id << "] is DOWN. Can't process get request.\n";
        return {"", ""};
    }
    Node* owner = net->findOwnerNode(key);
    if (owner == nullptr) {
        lock_guard<mutex> cout_lk(cout_mtx);
        cout << "[Node " << id << "] No alive nodes to get data.\n";
        return {"", ""};
    }

    if (owner == this) {
        // attempt read locally
        auto p = localGet(key);
        if (p.first != "") {
            lock_guard<mutex> cout_lk(cout_mtx);
            cout << "[Node " << id << "] Found key " << key << " locally (owner).\n";
            return p;
        }
        // if not found locally (maybe lost), try successors (replicas)
        auto succs = net->successorsOf(this, 3); // try a small number
        for (Node* s : succs) {
            if (!s->isAlive.load()) continue;
            auto rp = s->localGet(key);
            if (rp.first != "") {
                lock_guard<mutex> cout_lk(cout_mtx);
                cout << "[Node " << id << "] Owner couldn't find key locally, but successor " << s->id << " had replica.\n";
                return rp;
            }
        }
        return {"", ""};
    } else {
        // route to owner
        {
            lock_guard<mutex> cout_lk(cout_mtx);
            cout << "[Node " << id << "] Routing GET request for key " << key << " to owner " << owner->id << endl;
        }
        return owner->receiveGet(key);
    }
}

/* ---- Demonstration main: create network, nodes, store & retrieve concurrently ---- */

int main() {
    srand((unsigned)time(nullptr));
    Hashing hasher;
    Network network;

    // Create nodes (simulate multiple independent peers)
    vector<Node*> nodeList;
    for (int i = 0; i < 6; ++i) {
        string nid = "N" + to_string(i);
        Node* n = new Node(nid, &network);
        nodeList.push_back(n);
        network.registerNode(n);
    }

    {
        lock_guard<mutex> cout_lk(cout_mtx);
        cout << network.nodeListStr() << "\n\n";
    }

    // Choose a replication factor
    int replication_factor = 2; // owner + 2 successors

    // thread-safe random generator for picking entry nodes
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> distEntry(0, (int)nodeList.size() - 1);

    // client operation lambdas (they may run in parallel threads)
    auto sendStoreFromRandom = [&](const string& name, const string& mail, const string& socialID, const string& password) {
        unsigned long long key = hasher.DoubleHash(name, mail);
        string encSocial = Encryptor::encrypt(socialID);
        string encPass = Encryptor::encrypt(password);
        int entryIdx = distEntry(gen);
        Node* entry = nodeList[entryIdx];
        {
            lock_guard<mutex> cout_lk(cout_mtx);
            cout << "\n[Client] Sending STORE for '" << name << "' via node " << entry->id << " key=" << key << "\n";
        }
        entry->receiveStore(key, encSocial, encPass, replication_factor);
    };

    auto sendGetFromRandom = [&](const string& name, const string& mail) {
        unsigned long long key = hasher.DoubleHash(name, mail);
        int entryIdx = distEntry(gen);
        Node* entry = nodeList[entryIdx];
        {
            lock_guard<mutex> cout_lk(cout_mtx);
            cout << "\n[Client] Sending GET for '" << name << "' via node " << entry->id << " key=" << key << "\n";
        }
        auto p = entry->receiveGet(key);
        if (p.first != "") {
            lock_guard<mutex> cout_lk(cout_mtx);
            cout << "[Client] Received encrypted data. Decrypting...\n";
            cout << "   Social: " << Encryptor::decrypt(p.first) << "\n";
            cout << "   Pass  : " << Encryptor::decrypt(p.second) << "\n";
        } else {
            lock_guard<mutex> cout_lk(cout_mtx);
            cout << "[Client] Data not found in network.\n";
        }
    };

    // Start a bunch of concurrent store/get operations
    vector<thread> workers;

    // start stores concurrently
    workers.emplace_back(thread(sendStoreFromRandom, "Gurkirat", "gurkirat@gmail.com", "SOCIAL123", "pass123"));
    workers.emplace_back(thread(sendStoreFromRandom, "Ansh", "ansh@yahoo.com", "SOCIAL", "password"));
    workers.emplace_back(thread(sendStoreFromRandom, "Alice", "alice@europe.com", "SOCIAL789", "pwd789"));
    workers.emplace_back(thread(sendStoreFromRandom, "Bob", "bob@nowhere.com", "SOC_B", "bpass"));
    workers.emplace_back(thread(sendStoreFromRandom, "Carol", "carol@site.io", "SOC_C", "cpass"));

    // small delay so replication threads can run
    this_thread::sleep_for(chrono::milliseconds(100));

    // concurrent gets
    for (int i = 0; i < 6; ++i) {
        workers.emplace_back(thread(sendGetFromRandom, "Gurkirat", "gurkirat@gmail.com"));
        workers.emplace_back(thread(sendGetFromRandom, "Alice", "alice@europe.com"));
    }

    // Simulate node failure after some time in separate thread
    workers.emplace_back(thread([&network, &hasher]() {
        this_thread::sleep_for(chrono::milliseconds(200));
        unsigned long long gkey = hasher.DoubleHash("Gurkirat", "gurkirat@gmail.com");
        Node* gOwner = network.findOwnerNode(gkey);
        if (gOwner) gOwner->fail();
    }));

    // After failure, do more gets to test replication serving reads
    workers.emplace_back(thread([&]() {
        this_thread::sleep_for(chrono::milliseconds(300));
        sendGetFromRandom("Gurkirat", "gurkirat@gmail.com");
        sendGetFromRandom("Ansh", "ansh@yahoo.com");
    }));

    // Join all workers
    for (auto &t : workers) if (t.joinable()) t.join();

    // final state
    {
        lock_guard<mutex> cout_lk(cout_mtx);
        cout << "\nFinal network: " << network.nodeListStr() << "\n";
    }

    // Clean up
    for (Node* n : nodeList) delete n;
    return 0;
}
