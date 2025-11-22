#pragma once

#include "../lsm/LSMTree.h"
#include "../spatial/Point.h"
#include "../spatial/MBR.h"
#include <map>
#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <iostream>
#include <fstream>
#include <filesystem>
#include "../json.hpp"

using namespace std;
using json = nlohmann::json;
namespace sp = spatial;
namespace fs = std::filesystem;

namespace sql {

template<typename T>
struct TableIndex {
    lsm::LSMTree<T>* primary;
    lsm::LSMTree<T>* secondary;
    TableIndex() : primary(nullptr), secondary(nullptr) {}
};

struct TableSchema {
    string name;
    vector<string> columns;
    vector<string> types;
    string spatialColumn;
    string mergePolicy = "Tiered";
    int policyParam = 4;
    string spatialComparator = "Simple";
    TableSchema() = default;
    TableSchema(const string& n) : name(n) {}
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(TableSchema, name, columns, types, spatialColumn, mergePolicy, policyParam,spatialComparator)
};

enum class CmdType { CREATE, INSERT, SELECT };

struct CreateCmd {
    string table;
    vector<string> columns;
    string policyName = "Tiered";
    int policyParam = 4;
    string comparatorName = "Simple";
};

struct InsertCmd {
    string table;
    vector<string> values;
};

struct SelectCmd {
    string table;
    bool countOnly = false;
    bool hasSpatial = false;
    string spatialColumn;
    double xmin = 0, ymin = 0, xmax = 0, ymax = 0;
};

struct Command {
    CmdType type = CmdType::SELECT;
    CreateCmd* create = nullptr;
    InsertCmd* insert = nullptr;
    SelectCmd* select = nullptr;

    ~Command() {
        delete create;
        delete insert;
        delete select;
    }
};

class CatalogManager {
private:
    map<string, TableSchema> tables;
    const string DATA_DIR = "data";
    const string CATALOG_FILE = "data/catalog.json";
    void saveToDisk() {
        if (!fs::exists(DATA_DIR)) {
            fs::create_directories(DATA_DIR);
        }
        json j = tables;
        ofstream file(CATALOG_FILE);
        if (file.is_open()) {
            file << j.dump(4);
            file.close();
        }
    }

    void loadFromDisk() {
        if (!fs::exists(CATALOG_FILE)) return;

        ifstream file(CATALOG_FILE);
        if (file.is_open()) {
            try {
                json j;
                file >> j;
                tables = j.get<map<string, TableSchema>>();
            } catch (...) {
            }
        }
    }
public:
    CatalogManager() {
        loadFromDisk();
    }
    vector<string> getAllTableNames() const {
        vector<string> names;
        for (const auto& pair : tables) {
            names.push_back(pair.first);
        }
        return names;
    }
    void createTable(const TableSchema& schema) {
        tables[schema.name] = schema;
        saveToDisk();
    }

    bool tableExists(const string& name) const {
        return tables.find(name) != tables.end();
    }

    const TableSchema& getTable(const string& name) const {
        auto it = tables.find(name);
        if (it == tables.end()) {
            throw runtime_error("Table not found: " + name);
        }
        return it->second;
    }

    TableSchema& getTable(const string& name) {
        auto it = tables.find(name);
        if (it == tables.end()) {
            throw runtime_error("Table not found: " + name);
        }
        return it->second;
    }
};

template<typename T = int>
class QueryExecutor {
private:
    CatalogManager& catalog;
    map<string, TableIndex<T>>& tableIndices;
    lsm::GlobalBudget* globalBudget;
public:
    QueryExecutor(CatalogManager& cat,
                 map<string, TableIndex<T>>& indices,
                 lsm::GlobalBudget* budget)
        : catalog(cat), tableIndices(indices), globalBudget(budget) {}

    string execute(const string& sql) {
        auto toLower = [](const string &s){
            string o=s;
            for(char &c:o) c=static_cast<char>(tolower((unsigned char)c));
            return o;
        };

        auto trim = [](const string &s){
            size_t a = s.find_first_not_of(" \t\n\r");
            if(a==string::npos) return string();
            size_t b = s.find_last_not_of(" \t\n\r");
            return s.substr(a,b-a+1);
        };

        auto splitArgs = [&](const string &s){
            vector<string> out;
            string cur;
            for(char c: s){
                if(c==','){
                    size_t a = cur.find_first_not_of(" \t\n\r");
                    size_t b = cur.find_last_not_of(" \t\n\r");
                    if(a==string::npos) out.push_back(string());
                    else out.push_back(cur.substr(a,b-a+1));
                    cur.clear();
                } else cur.push_back(c);
            }
            if(!cur.empty()){
                size_t a = cur.find_first_not_of(" \t\n\r");
                size_t b = cur.find_last_not_of(" \t\n\r");
                if(a==string::npos) out.push_back(string());
                else out.push_back(cur.substr(a,b-a+1));
            }
            return out;
        };

        string s = trim(sql);
        string l = toLower(s);

        if (l.rfind("create table", 0) == 0) {
            Command cmd; cmd.type = CmdType::CREATE; cmd.create = new CreateCmd();
            size_t pL = l.find('(');
            string header = (pL==string::npos) ? l : l.substr(0,pL);
            {
                istringstream iss(header);
                string w; iss>>w; iss>>w;
                iss>>cmd.create->table;
            }
            if (pL!=string::npos){
                size_t pR = l.find_last_of(')');
                if (pR + 1 < l.length()) {
                        string suffix = l.substr(pR + 1);
                        stringstream ss(suffix);
                        string word;
                        while (ss >> word) {
                            string wLower = toLower(word);
                            if (wLower == "with") {
                                string next;
                                if (ss >> next && toLower(next) == "policy") {
                                    if (ss >> cmd.create->policyName) {
                                        int val;
                                        while (isspace(ss.peek())) ss.ignore();
                                        if (isdigit(ss.peek())) {
                                            ss >> val;
                                            cmd.create->policyParam = val;
                                        }
                                    }
                                }
                            }
                            if (wLower == "comparator") {
                                ss >> cmd.create->comparatorName;
                            }
                        }
                    }
            }
            return executeCreate(cmd);
        }

        if (l.rfind("insert into",0)==0) {
            Command cmd; cmd.type = CmdType::INSERT; cmd.insert = new InsertCmd();
            size_t posValues = l.find("values");
            string header = (posValues==string::npos) ? l : l.substr(0,posValues);
            {
                istringstream iss(header);
                string w; iss>>w; iss>>w;
                iss>>cmd.insert->table;
            }
            if (posValues!=string::npos){
                size_t pL = s.find('(', posValues);
                size_t pR = s.find(')', pL);
                if(pL!=string::npos && pR!=string::npos && pR>pL){
                    string inside = s.substr(pL+1,pR-pL-1);
                    auto parts = splitArgs(inside);
                    for(auto &p:parts) {
                        if(p.size()>=2 && ((p.front()=='"'&&p.back()=='"')||(p.front()=='\''&&p.back()=='\'')))
                            cmd.insert->values.push_back(p.substr(1,p.size()-2));
                        else
                            cmd.insert->values.push_back(p);
                    }
                }
            }
            return executeInsert(cmd);
        }

        if (l.rfind("select",0)==0) {
            Command cmd; cmd.type = CmdType::SELECT; cmd.select = new SelectCmd();
            if (l.find("count(")!=string::npos) cmd.select->countOnly = true;
            size_t posFrom = l.find(" from ");
            if (posFrom!=string::npos){
                size_t start = posFrom+6;
                size_t end = l.find_first_of(" \t\n;", start);
                if(end==string::npos) end = l.size();
                cmd.select->table = trim(s.substr(start,end-start));
            }
            size_t posWhere = l.find(" where ");
            if (posWhere!=string::npos){
                size_t posSpatial = l.find("spatial_intersect", posWhere);
                if(posSpatial!=string::npos){
                    size_t pL = s.find('(', posSpatial);
                    size_t pR = s.find(')', pL);
                    if(pL!=string::npos && pR!=string::npos && pR>pL){
                        string inside = s.substr(pL+1,pR-pL-1);
                        auto parts = splitArgs(inside);
                        if(parts.size()>=5){
                            cmd.select->spatialColumn = parts[0];
                            try{
                                cmd.select->xmin = stod(parts[1]);
                                cmd.select->ymin = stod(parts[2]);
                                cmd.select->xmax = stod(parts[3]);
                                cmd.select->ymax = stod(parts[4]);
                                cmd.select->hasSpatial = true;
                            } catch(...) {

                            }
                        }
                    }
                }
            }
            return executeSelect(cmd);
        }

        return string("Error: Unsupported or invalid statement");
    }

    string executeCreate(const Command& cmd) {
        if (!cmd.create) return "Error: Invalid CREATE command";

        string tableName = cmd.create->table;
        if (catalog.tableExists(tableName)) {
            return "Error: Table '" + tableName + "' already exists.";
        }

        TableSchema schema;
        schema.name = cmd.create->table;
        schema.mergePolicy = cmd.create->policyName;
        schema.policyParam = cmd.create->policyParam;
        schema.spatialComparator = cmd.create->comparatorName;

        for (const auto &c : cmd.create->columns) {
            size_t colon = c.find(' ');
            if (colon != string::npos) {
                schema.columns.push_back(c.substr(0, colon));
                schema.types.push_back(c.substr(colon + 1));
                string t = c.substr(colon + 1);
                if (t == "POINT" || t == "point" || t == "GEOMETRY" || t == "geometry") {
                    schema.spatialColumn = c.substr(0, colon);
                }
            }
        }
        catalog.createTable(schema);
        TableIndex<T> indices;

        auto* pPolicy = lsm::PolicyFactory<T>::create(schema.mergePolicy, schema.policyParam);
        auto* sPolicy = lsm::PolicyFactory<T>::create(schema.mergePolicy, schema.policyParam);

        sp::ISpatialComparator<T>* pComp;
        sp::ISpatialComparator<T>* sComp;

        sp::Point minP({-180.0, -90.0});
        sp::Point maxP({180.0, 90.0});
        sp::MBR worldBounds(minP, maxP);

        if (schema.spatialComparator == "hilbert") {
            sComp = new sp::HilbertComparatorAdapter<T>(worldBounds);
        } else {
            sComp = new sp::SimpleComparatorAdapter<T>();
        }

        pComp = new sp::SimpleComparatorAdapter<T>();

        string pName = schema.name;
        string sName = schema.name;

        indices.primary = new lsm::LSMTree<T>(pName, globalBudget, pPolicy, 1024, pComp, false);
        indices.secondary = new lsm::LSMTree<T>(sName, globalBudget, sPolicy, 24, sComp, true);

        tableIndices[schema.name] = indices;
        return "Table '" + schema.name + "' created successfully";
    }

    string executeInsert(const Command& cmd) {
        if (!cmd.insert) return "Error: Invalid INSERT command";
        string tableName = cmd.insert->table;
        if (!catalog.tableExists(tableName)) return "Error: Table '" + tableName + "' does not exist";
        if (tableIndices.find(tableName) == tableIndices.end()) {
            TableSchema savedSchema = catalog.getTable(tableName);
            auto* pPolicy = lsm::PolicyFactory<T>::create(savedSchema.mergePolicy, savedSchema.policyParam);
            auto* sPolicy = lsm::PolicyFactory<T>::create(savedSchema.mergePolicy, savedSchema.policyParam);

            sp::ISpatialComparator<T>* pComp = new sp::SimpleComparatorAdapter<T>();
            sp::ISpatialComparator<T>* sComp;

            if (savedSchema.spatialComparator == "Hilbert") {
                sp::Point minP({-180.0, -90.0});
                sp::Point maxP({180.0, 90.0});
                sp::MBR world(minP, maxP);
                sComp = new sp::HilbertComparatorAdapter<T>(world);
            } else {
                sComp = new sp::SimpleComparatorAdapter<T>();
            }

            string pName = tableName;
            string sName = tableName;

            TableIndex<T> indices;
            indices.primary = new lsm::LSMTree<T>(pName, globalBudget, pPolicy, 1024, pComp, false);
            indices.secondary = new lsm::LSMTree<T>(sName, globalBudget, sPolicy, 24, sComp, true);

            tableIndices[tableName] = indices;
        }
        TableIndex<T>& indices = tableIndices[tableName];
        vector<double> coords;
        T payload = T();
        for (size_t i = 0; i < cmd.insert->values.size(); ++i) {
            const string &v = cmd.insert->values[i];
            try {
                double d = stod(v);
                coords.push_back(d);
            } catch (...) {

            }
        }
        if (coords.size() >= 2) {
            sp::Point p({coords[0], coords[1]});
            if (coords.size() > 2) payload = static_cast<T>(coords[2]);
            bool pSuccess = indices.primary->insert(p, payload);
            bool sSuccess = indices.secondary->insert(p, payload);
            if (pSuccess && sSuccess) {
                const auto& metrics = indices.secondary->getMetrics();
                stringstream ss;
                ss << "INSERT successful (P & S)";
                ss << "\n[W-METRICS] WA: " << metrics.writeAmplification;
                ss << " | Total Writes: " << metrics.totalWrites;
                return ss.str();
            } else {
                return "Error: Insert failed (Budget exceeded or Memory Error)";
            }
        }
        return "Error: Invalid INSERT values";
    }

    string executeClean(const string& tableNameRaw) {
        string tableName = tableNameRaw;
        tableName.erase(0, tableName.find_first_not_of(" \t\n\r"));
        tableName.erase(tableName.find_last_not_of(" \t\n\r") + 1);
        if (!catalog.tableExists(tableName)) {
            return "Error: Table '" + tableName + "' does not exist.";
        }

        auto it = tableIndices.find(tableName);
        if (it != tableIndices.end()) {
            delete it->second.primary;
            delete it->second.secondary;
            tableIndices.erase(it);
        }

        string tableDir = "data/" + tableName;
        try {
            if (fs::exists(tableDir)) {
                uintmax_t n = fs::remove_all(tableDir);
                cout << "[CLEAN] Deleted " << n << " files/directories." << endl;
            }
            fs::create_directories(tableDir);
        } catch (const fs::filesystem_error& e) {
            return "Error deleting files: " + string(e.what());
        }

        TableSchema schema = catalog.getTable(tableName);

        auto* pPolicy = lsm::PolicyFactory<T>::create(schema.mergePolicy, schema.policyParam);
        auto* sPolicy = lsm::PolicyFactory<T>::create(schema.mergePolicy, schema.policyParam);

        sp::ISpatialComparator<T>* pComp = new sp::SimpleComparatorAdapter<T>();
        sp::ISpatialComparator<T>* sComp;

        if (schema.spatialComparator == "Hilbert") {
            sp::Point minP({-180.0, -90.0});
            sp::Point maxP({180.0, 90.0});
            sp::MBR world(minP, maxP);
            sComp = new sp::HilbertComparatorAdapter<T>(world);
        } else {
            sComp = new sp::SimpleComparatorAdapter<T>();
        }

        string pName = tableName;
        string sName = tableName;

        TableIndex<T> newIndices;
        newIndices.primary = new lsm::LSMTree<T>(pName, globalBudget, pPolicy, 1024, pComp, false);
        newIndices.secondary = new lsm::LSMTree<T>(sName, globalBudget, sPolicy, 24, sComp,true);



        tableIndices[tableName] = newIndices;

        return "Table '" + tableName + "' cleaned successfully (Data and Metrics reset).";
    }

    string executeSelect(const Command& cmd) {
        if (!cmd.select) return "Error: Invalid SELECT command";
        string tableName = cmd.select->table;
        if (!catalog.tableExists(tableName)) return "Error: Table '" + tableName + "' does not exist";
        auto it = tableIndices.find(tableName);
        if (it == tableIndices.end()) return "Error: LSM-tree not found for table '" + tableName + "'";
        lsm::LSMTree<T>* tree = it->second.secondary;

        uint64_t initialRA = tree->getMetrics().readAmplification;

        vector<sp::SpatialRecord<T>> results;
        auto start_time = chrono::high_resolution_clock::now();

        if (cmd.select->hasSpatial) {
            sp::MBR box(sp::Point({cmd.select->xmin, cmd.select->ymin}), sp::Point({cmd.select->xmax, cmd.select->ymax}));
            results = tree->spatialRangeQuery(box);

        } else {
            sp::MBR full(2);
            sp::Point lo({-1e9, -1e9});
            sp::Point hi({1e9, 1e9});
            full.setLower(lo); full.setUpper(hi);
            results = tree->spatialRangeQuery(full);
        }

        auto end_time = chrono::high_resolution_clock::now();
        double latencyMs = chrono::duration<double, milli>(end_time - start_time).count();
        uint64_t finalRA = tree->getMetrics().readAmplification;
        uint64_t queryRA = finalRA - initialRA;

        const auto& metrics = tree->getMetrics();
        stringstream ss;
        if (cmd.select->countOnly) {
            ss << "COUNT(*): " << results.size();
        } else {
            ss << "Results (" << results.size() << " rows):\n";
            for (const auto &r : results) {
                ss << "Point: (";
                for (size_t i = 0; i < r.point.dimensions(); ++i) {
                    if (i) ss << ", ";
                    ss << r.point[i];
                }
                ss << ")\n";
            }
        }

        ss << "\n[R-METRICS] RA: " << queryRA;
        ss << " | Latency: " << fixed << setprecision(2) << latencyMs << " ms";
        return ss.str();
    }
};

}
