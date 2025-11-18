#pragma once

#include "../lsm/LSMTree.h"
#include "../spatial/Point.h"
#include "../spatial/MBR.h"
#include <map>
#include <string>
#include <vector>
#include <memory>
#include <sstream>

using namespace std;
namespace sp = spatial;

namespace sql {

struct TableSchema {
    string name;
    vector<string> columns;
    vector<string> types;
    string spatialColumn;

    TableSchema() = default;
    TableSchema(const string& n) : name(n) {}
};

enum class CmdType { CREATE, INSERT, SELECT };

struct CreateCmd {
    string table;
    vector<string> columns;
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

public:
    void createTable(const TableSchema& schema) {
        tables[schema.name] = schema;
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
    map<string, lsm::LSMTree<T>*>& lsmTrees;

public:
    QueryExecutor(CatalogManager& cat,
                 map<string, lsm::LSMTree<T>*>& trees)
        : catalog(cat), lsmTrees(trees) {}

    string execute(const string& sql) {
        auto toLower = [](const string &s){ string o=s; for(char &c:o) c=static_cast<char>(tolower((unsigned char)c)); return o; };
        auto trim = [](const string &s){ size_t a = s.find_first_not_of(" \t\n\r"); if(a==string::npos) return string(); size_t b = s.find_last_not_of(" \t\n\r"); return s.substr(a,b-a+1); };
        auto splitArgs = [&](const string &s){ vector<string> out; string cur; for(char c: s){ if(c==','){ size_t a = cur.find_first_not_of(" \t\n\r"); size_t b = cur.find_last_not_of(" \t\n\r"); if(a==string::npos) out.push_back(string()); else out.push_back(cur.substr(a,b-a+1)); cur.clear(); } else cur.push_back(c);} if(!cur.empty()){ size_t a = cur.find_first_not_of(" \t\n\r"); size_t b = cur.find_last_not_of(" \t\n\r"); if(a==string::npos) out.push_back(string()); else out.push_back(cur.substr(a,b-a+1)); } return out; };

        string s = trim(sql);
        string l = toLower(s);

        if (l.rfind("create table", 0) == 0) {
            Command cmd; cmd.type = CmdType::CREATE; cmd.create = new CreateCmd();
            size_t pL = s.find('(');
            string header = (pL==string::npos) ? s : s.substr(0,pL);
            {
                istringstream iss(header);
                string w; iss>>w; iss>>w;
                iss>>cmd.create->table;
            }
            if (pL!=string::npos){ size_t pR = s.find_last_of(')'); if(pR!=string::npos && pR>pL){ string inside = s.substr(pL+1,pR-pL-1); auto parts = splitArgs(inside); for(auto &p:parts) cmd.create->columns.push_back(p);} }
            return executeCreate(cmd);
        }

        if (l.rfind("insert into",0)==0) {
            Command cmd; cmd.type = CmdType::INSERT; cmd.insert = new InsertCmd();
            size_t posValues = l.find("values");
            string header = (posValues==string::npos) ? s : s.substr(0,posValues);
            { istringstream iss(header); string w; iss>>w; iss>>w; iss>>cmd.insert->table; }
            if (posValues!=string::npos){ size_t pL = s.find('(', posValues); size_t pR = s.find(')', pL); if(pL!=string::npos && pR!=string::npos && pR>pL){ string inside = s.substr(pL+1,pR-pL-1); auto parts = splitArgs(inside); for(auto &p:parts) { 
                            if(p.size()>=2 && ((p.front()=='"'&&p.back()=='"')||(p.front()=='\''&&p.back()=='\''))) cmd.insert->values.push_back(p.substr(1,p.size()-2)); else cmd.insert->values.push_back(p);
                        } } }
            return executeInsert(cmd);
        }

        if (l.rfind("select",0)==0) {
            Command cmd; cmd.type = CmdType::SELECT; cmd.select = new SelectCmd();
            if (l.find("count(")!=string::npos) cmd.select->countOnly = true;
            size_t posFrom = l.find(" from ");
            if (posFrom!=string::npos){ size_t start = posFrom+6; size_t end = l.find_first_of(" \t\n;", start); if(end==string::npos) end = l.size(); cmd.select->table = trim(s.substr(start,end-start)); }
            size_t posWhere = l.find(" where ");
            if (posWhere!=string::npos){ size_t posSpatial = l.find("spatial_intersect", posWhere); if(posSpatial!=string::npos){ size_t pL = s.find('(', posSpatial); size_t pR = s.find(')', pL); if(pL!=string::npos && pR!=string::npos && pR>pL){ string inside = s.substr(pL+1,pR-pL-1); auto parts = splitArgs(inside); if(parts.size()>=5){ cmd.select->spatialColumn = parts[0]; try{ cmd.select->xmin = stod(parts[1]); cmd.select->ymin = stod(parts[2]); cmd.select->xmax = stod(parts[3]); cmd.select->ymax = stod(parts[4]); cmd.select->hasSpatial = true; }catch(...){}}}}}
            return executeSelect(cmd);
        }

        return string("Error: Unsupported or invalid statement");
    }

    string executeCreate(const Command& cmd) {
        if (!cmd.create) return "Error: Invalid CREATE command";
        TableSchema schema;
        schema.name = cmd.create->table;
        for (const auto &c : cmd.create->columns) {
            size_t colon = c.find(':');
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
        lsmTrees[schema.name] = new lsm::LSMTree<T>(2);
        return "Table '" + schema.name + "' created successfully";
    }

    string executeInsert(const Command& cmd) {
        if (!cmd.insert) return "Error: Invalid INSERT command";
        string tableName = cmd.insert->table;
        if (!catalog.tableExists(tableName)) return "Error: Table '" + tableName + "' does not exist";
        if (lsmTrees.find(tableName) == lsmTrees.end()) lsmTrees[tableName] = new lsm::LSMTree<T>(2);
        lsm::LSMTree<T>* tree = lsmTrees[tableName];
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
            tree->insert(p, payload);
            return "INSERT successful";
        }
        return "Error: Invalid INSERT values";
    }

    string executeSelect(const Command& cmd) {
        if (!cmd.select) return "Error: Invalid SELECT command";
        string tableName = cmd.select->table;
        if (!catalog.tableExists(tableName)) return "Error: Table '" + tableName + "' does not exist";
        auto it = lsmTrees.find(tableName);
        if (it == lsmTrees.end()) return "Error: LSM-tree not found for table '" + tableName + "'";
        lsm::LSMTree<T>* tree = it->second;
        vector<sp::SpatialRecord<T>> results;
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
        if (cmd.select->countOnly) return string("COUNT(*): ") + to_string(results.size());
        stringstream ss; ss << "Results (" << results.size() << " rows):\n";
        for (const auto &r : results) {
            ss << "Point: (";
            for (size_t i = 0; i < r.point.dimensions(); ++i) {
                if (i) ss << ", ";
                ss << r.point[i];
            }
            ss << ")\n";
        }
        return ss.str();
    }
};

}