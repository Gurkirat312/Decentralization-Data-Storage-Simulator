#include <iostream>
#include <unordered_map>
#include <vector>
#include <string>
using namespace std;

class Hashing {
public:
    unsigned long long SingleHash(const string& input) {
        unsigned long long hash = 0;
        unsigned long long prime = 31, power = 1;
        for (char c : input) {
            hash += (c - 'a' + 1) * power;
            power *= prime;
        }
        return hash;
    }

    string toBase36(unsigned long long num) {
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

    unsigned long long DoubleHash(const string& name, const string& mail) {
        string input = name + mail;
        unsigned long long hash1 = SingleHash(input);
        string base36Str = toBase36(hash1);
        unsigned long long hash2 = SingleHash(base36Str);
        unsigned long long finalHash = hash1 + (hash2 * 1315423911ULL);
        return finalHash;
    }
};

class Node {
public:
    int id;
    unordered_map<unsigned long long, pair<string, string>> storage;

    Node(int nodeId) : id(nodeId) {}

    void store(unsigned long long key, const string& socialID, const string& password) {
        storage[key] = {socialID, password};
        cout << "Node " << id << " stored data at key " << key << endl;
    }

    pair<string, string> retrieve(unsigned long long key) {
        if (storage.find(key) != storage.end()) {
            return storage[key];
        }
        return {"", ""};
    }
};

class LocalController {
private:
    vector<Node> nodes;
    Hashing hasher;

public:
    string location;

    LocalController(string loc, int numNodes) : location(loc) {
        for (int i = 0; i < numNodes; i++) {
            nodes.emplace_back(i);
        }
    }

    void addData(const string& name, const string& mail, const string& socialID, const string& password) {
        unsigned long long key = hasher.DoubleHash(name, mail);
        int nodeId = key % nodes.size();
        nodes[nodeId].store(key, socialID, password);
    }

    pair<string, string> getData(const string& name, const string& mail) {
        unsigned long long key = hasher.DoubleHash(name, mail);
        int nodeId = key % nodes.size();
        return nodes[nodeId].retrieve(key);
    }
};

class GlobalController {
private:
    vector<LocalController*> regions;
    Hashing hasher;

public:
    void addRegion(LocalController* region) {
        regions.push_back(region);
    }

    void addData(const string& name, const string& mail, const string& socialID, const string& password) {
        unsigned long long key = hasher.DoubleHash(name, mail);
        int regionId = key % regions.size(); // pick region
        cout << "GlobalController routed data to region: " << regions[regionId]->location << endl;
        regions[regionId]->addData(name, mail, socialID, password);
    }

    void getData(const string& name, const string& mail) {
        unsigned long long key = hasher.DoubleHash(name, mail);
        int regionId = key % regions.size(); // pick region
        auto data = regions[regionId]->getData(name, mail);

        if (data.first != "") {
            cout << "Data found in region " << regions[regionId]->location << ":\n";
            cout << "User : " << name << endl;
            cout << "   Social ID: " << data.first << "\n";
            cout << "   Password : " << data.second << "\n";
        } else {
            cout << "Data not found!" << endl;
        }
    }
};

int main() {
    LocalController Delhi("Delhi", 3);
    LocalController Mumbai("Mumbai", 2);
    LocalController Banglore("Banglore", 4);

    GlobalController global;
    global.addRegion(&Delhi);
    global.addRegion(&Mumbai);
    global.addRegion(&Banglore);

    //the name should be username and it should be unique
    global.addData("Gurkirat", "gurkirat@gmail.com", "SOCIAL123", "pass123");
    global.addData("Gurkirat", "gurkirat@yahoo.com", "SOCIAL456", "word456");
    global.addData("Alice", "alice@europe.com", "SOCIAL789", "pwd789");

    cout << "\n--- Retrieving Data ---\n";
    global.getData("Gurkirat", "gurkirat@gmail.com");
    global.getData("Ansh", "ansh@yahoo.com");
    global.getData("Gurkirat" , "gurkirat@yahoo.com");
    global.getData("Alice", "alice@europe.com");

    return 0;
}
