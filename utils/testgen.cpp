/*
Copyright 2013 Henrik Mühe and Florian Funke

This file is part of CampersCoreBurner.

CampersCoreBurner is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

CampersCoreBurner is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with CampersCoreBurner.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <algorithm>
#include <fstream>
#include <vector>
#include "metrics.hpp"
#include "core.h"
#include <unordered_set>
#include <map>
#include <set>
#include <string>
#include <sstream>
#include <iostream>
#include <cstdint>
#include "random.hpp"
// #define NDEBUG // uncomment to disable assert()
#include <cassert>
#include <stdexcept>

using namespace std;
using campers::Random;


void edit(string& str, unsigned operations, Random& r) {
    enum class Edit : unsigned {Add=0, Remove=1, Change=2};
        for (unsigned editOp=0; editOp<operations; ++editOp) {
        unsigned i = r.next() % 3;
        switch (static_cast<Edit>(i)) {
            case Edit::Add: {
                unsigned pos = r.next() % (str.size()+1);
                char c = 'a'+(r.next()%26);
                if (pos == str.size())
                    str+=c;
                else
                    str.insert(pos, 1, c);
                break;
            }
            case Edit::Remove: {
                unsigned pos = r.next() % str.size();
                str.erase(str.begin()+pos);
                break;
            }
            case Edit::Change: {
                unsigned pos = r.next() % str.size();
                char c = 'a'+(r.next()%26);
                str[pos]=c;
                break;
            }
            default: throw;
        }
    }
}

static char rndChar(Random& r) {
    return 'a'+(r.next()%26);
}

void levenshtein(string& str, unsigned operations, Random& r) {
    set<unsigned> changes;
    for (unsigned editOp=0; editOp<operations; ++editOp) { // first some changes
        if (r.next()%2) {
            unsigned pos = r.next() % str.size();
            str[pos]=/*'x'*/rndChar(r);
            changes.insert(pos);
        }
    }
    for (unsigned editOp=changes.size(); editOp<operations; ++editOp) { // then some inserts
        unsigned pos = r.next() % (str.size()+1);
        if (pos == str.size())
            str+=rndChar(r);
        else
            str.insert(pos, 1, rndChar(r));
    }
}

void hamming(string& str, unsigned operations, Random& r) {
    for (unsigned editOp=0; editOp<operations; ++editOp) {
        unsigned pos = r.next() % str.size();
        str[pos]=rndChar(r);
    }
}

template <typename T>
static T randBetween(Random& r, T min, T max) {
    T res = min + r.next()%(1+(max-min));
    assert(res >= min);
    assert(res <= max);
    return res;
}

template <typename T>
static T randFloatBetween(Random& r, T min, T max) {
    T res = min + (r.next()*1.0f)/numeric_limits<uint64_t>::max()*(max-min);
    assert(res >= min);
    assert(res <= max);
    return round(res);
}

struct DataSet {
    unordered_set<string> index;
    vector<string> words;

    vector<vector<string>> documents;
    vector<vector<string>> exactQueries;
    vector<std::pair<unsigned, vector<string>>> hammingQueries;
    vector<std::pair<unsigned, vector<string>>> levenshteinQueries;

    uint64_t cycles;
    uint64_t queriesInSystem;

    DataSet(const char* fileName, uint64_t limit=~0ull) : cycles(1), queriesInSystem(~0) {
        string word;
        ifstream in(fileName);
        uint64_t ctr=0;
        if (in.is_open()) {
            while (in >> word) {
                if (/*word.find('x')==string::npos &&*/ word.size() >= MIN_WORD_LENGTH && word.size() <= MAX_WORD_LENGTH && std::all_of(word.begin(), word.end(), [](char c){ return c>='a'&&c<='z'; })) {
                    ++ctr;
                    index.insert(word);
                }
                if (ctr==limit)
                    break;
            }
            for (const auto& w : index)
                words.push_back(w);
        } else {
            throw runtime_error("cannot open file");
        }
    }

    void dumpDocStats() const {
        std::vector<uint64_t> len;
        for (const auto& doc : documents)
            len.push_back(doc.size());
        cerr << "Document Stats ------------" << endl;
        uint64_t cnt = len.size();
        uint64_t min = *min_element(len.begin(), len.end());
        uint64_t max = *max_element(len.begin(), len.end());
        uint64_t sum = accumulate(len.begin(), len.end(), 0);
        nth_element(len.begin(), len.begin()+(len.size()/2), len.end());
        uint64_t med = len[len.size()/2];
        cerr << "Cnt: " << cnt << endl;
        cerr << "Min: " << min << endl;
        cerr << "Max: " << max << endl;
        cerr << "Avg: " << sum/cnt << endl;
        cerr << "Med: " << med << endl;
    }

    void dump(const char* fileName) {
        ofstream out(fileName);

        vector<string> queries;
        for (size_t i=0, limit=exactQueries.size(); i<limit; ++i) {
            stringstream qs;
            qs << ' ' << 0/*match*/ << ' ' << 0 << ' ' << exactQueries[i].size();
            for (const auto& w : exactQueries[i])
                 qs << ' ' << w;
            queries.push_back(qs.str());
        }
        for (const auto& q : hammingQueries) {
            stringstream qs;
            qs << ' ' << 1/*hamming*/ << ' ' << q.first << ' ' << q.second.size();
            for (const auto& w : q.second)
                 qs << ' ' << w;
            queries.push_back(qs.str());
        }
        for (const auto& q : levenshteinQueries) {
            stringstream qs;
            qs << ' ' << 2/*levenshtein*/ << ' ' << q.first << ' ' << q.second.size();
            for (const auto& w : q.second)
                 qs << ' ' << w;
            queries.push_back(qs.str());
        }
        random_shuffle(queries.begin(), queries.end());
        random_shuffle(documents.begin(), documents.end());

        // dump initial queries
        uint32_t nextQueryId = 0;
        unordered_set<uint32_t> activeQueries;
        while(activeQueries.size()<queriesInSystem) {
            out << "s " << nextQueryId+1 << queries[nextQueryId] << endl;
            activeQueries.insert(nextQueryId++);
        }

        // normal execution
        Random r;
        uint64_t docsPerRun=documents.size()/cycles;
        uint64_t queriesChangePerRun=(queries.size()-queriesInSystem)/cycles; // about right .. never mind decimal places ..
        for (uint64_t c=0; c<cycles; ++c) {
            // dump docsPerRun documents
            for (unsigned i=0, limit=docsPerRun; i<limit; ++i)
                dumpDocument(out, (c*docsPerRun)+i);

            // replace 50% of the queries
            for (size_t i=0, limit=queriesChangePerRun; i<limit; ++i) {
                assert(!activeQueries.empty());
                auto it = activeQueries.begin();
                out << "e " << *it+1 << endl;
                activeQueries.erase(it);
            }
            for (size_t i=0, limit=queriesChangePerRun; i<limit; ++i) {
                assert(nextQueryId<queries.size());
                out << "s " << nextQueryId+1 << queries[nextQueryId] << endl;
                activeQueries.insert(nextQueryId++);
            }
        }
    }
    const vector<string>* operator->() const { return &words; }
    bool contains(const string& s) const { return index.find(s)!=index.end(); }


    void createHammingQueries(uint64_t queryCount, uint64_t matchingQueryCount, unsigned avgQuerySize, unsigned deviation, const vector<unsigned>& distances, unsigned unique) {
        auto queryWordCount=unique;
        Random r;

        // create matching query words
        map<unsigned, vector<string>> q;
        uint64_t sz=0;
        while (sz != queryWordCount) {
            string queryWord(getRandomWord(r, words));
            unsigned dist = distances[r.next()%distances.size()];
            hamming(queryWord, dist, r);
            q[dist].push_back(queryWord);
            ++sz;
        }

        // create non-matching query words
        unsigned maxDist = *max_element(distances.begin(), distances.end());
        while (sz!=2*queryWordCount) {
            string queryWord(getRandomWord(r, words));
            unsigned dist = maxDist+distances[r.next()%distances.size()];
            hamming(queryWord, dist, r);
            q[dist].push_back(queryWord);
            ++sz;
        }
        createDistQueries(hammingQueries, q, queryCount, matchingQueryCount, avgQuerySize, deviation, distances);
    }

    void createLevenshteinQueries(uint64_t queryCount, uint64_t matchingQueryCount, unsigned avgQuerySize, unsigned deviation, const vector<unsigned>& distances, unsigned unique) {
        auto queryWordCount=unique;
        Random r;

        // create matching query words
        map<unsigned, vector<string>> q;
        uint64_t sz=0;
        while (sz != queryWordCount) {
            string queryWord(getRandomWord(r, words));
            unsigned dist = distances[r.next()%distances.size()];
            levenshtein(queryWord, dist, r);
            q[dist].push_back(queryWord);
            ++sz;
        }

        // create non-matching query words
        unsigned maxDist = *max_element(distances.begin(), distances.end());
        while (sz!=2*queryWordCount) {
            string queryWord(getRandomWord(r, words));
            unsigned dist = maxDist+distances[r.next()%distances.size()];
            levenshtein(queryWord, dist, r);
            q[dist].push_back(queryWord);
            ++sz;
        }
        createDistQueries(levenshteinQueries, q, queryCount, matchingQueryCount, avgQuerySize, deviation, distances);
    }

    void createExactQueries(uint64_t queryCount, uint64_t matchingQueryCount, unsigned avgQuerySize, unsigned deviation, unsigned unique) {
        auto queryWordCount=unique;

        // create matching query words
        vector<string> matches;
        uint64_t skip=words.size()/queryWordCount;
        for (auto it = words.begin(), limit=words.end(); it!=limit && matches.size()!=queryWordCount; it+=skip)
            matches.push_back(*it);
        assert(matches.size()==queryWordCount);

        // create non-matching query words
        vector<string> rejects;
        for (Random r; rejects.size()!=queryWordCount;) {
            string query(getRandomWord(r, words));
            unsigned i=0;
            do {
                edit(query, 1+(r.next()%3), r);
                ++i;
            } while (contains(query) && i != 10); // try at most 10 times
            rejects.push_back(query);
        }
        assert(rejects.size()==queryWordCount);
        createQueries(exactQueries, matches, rejects, queryCount, matchingQueryCount, avgQuerySize, deviation);
    }

    void createDocuments(uint64_t documentCount, uint64_t avgDocSize, uint64_t deviation) {
        Random r;
        uint64_t nextDocId=documents.size();
        documents.resize(nextDocId+documentCount);
        for (uint64_t docId=nextDocId; docId < nextDocId+documentCount; ++docId) {
            uint32_t limit = (avgDocSize-deviation)+(r.next()%((2*deviation)+1));
            assert(limit > 0);
            for (uint64_t i=0; i<limit; ++i)
                documents[docId].push_back(getRandomWord(r, words));
        }

        for (uint64_t docId=nextDocId; docId < nextDocId+documentCount; ++docId) {
            const auto& doc = documents[docId];
            assert(doc.size()<=avgDocSize+deviation && doc.size()>=avgDocSize-deviation);
        }
    }

    private:
    template <typename T0, typename T1, typename T2>
    void createQueries(T0& resultContainer, const T1& matchContainer, const T2& rejectContainer, uint64_t queryCount, uint64_t matchingQueryCount, unsigned avgQuerySize, unsigned deviation) {
        // create queries
        resultContainer.resize(queryCount);
        Random r;
        // matching queries
        for (uint64_t qId=0, limit=matchingQueryCount; qId<limit; ++qId) {
            for (unsigned i=0, querySize=randBetween(r, avgQuerySize-deviation, avgQuerySize+deviation); i<querySize; ++i)
                resultContainer[qId].push_back(getRandomWord(r, matchContainer));
            assert(resultContainer[qId].size()>=avgQuerySize-deviation && resultContainer[qId].size()<=avgQuerySize+deviation);
        }
        // rejecting queries
        for (uint64_t qId=matchingQueryCount, limit=queryCount; qId<limit; ++qId) {
            for (unsigned i=0, querySize=randBetween(r, avgQuerySize-deviation, avgQuerySize+deviation); i<querySize; ++i) {
                if (!i || r.next()%2==0)
                    resultContainer[qId].push_back(getRandomWord(r, matchContainer));
                else
                    resultContainer[qId].push_back(getRandomWord(r, rejectContainer));
            }
            random_shuffle(resultContainer[qId].begin(), resultContainer[qId].end());
            if (!(resultContainer[qId].size()>=avgQuerySize-deviation && resultContainer[qId].size()<=avgQuerySize+deviation)) {
                throw;
            }
        }
        random_shuffle(resultContainer.begin(), resultContainer.end());
    }

    template <typename T0, typename T1>
    void createDistQueries(T0& resultContainer, T1& distQueries, uint64_t queryCount, uint64_t matchingQueryCount, unsigned avgQuerySize, unsigned deviation, const vector<unsigned>& acceptedDistances) {
        // create queries
        resultContainer.resize(queryCount);
        Random r;
        vector<unsigned> distances;
        unsigned cutoff=0;
        unsigned maxDist=*max_element(acceptedDistances.begin(), acceptedDistances.end());
        for (auto it = distQueries.begin(), limit = distQueries.end(); it!=limit; ++it) {
            if (it->first<=maxDist)
                 ++cutoff;
            distances.push_back(it->first);
        }

        // matching queries
        for (uint64_t qId=0, limit=matchingQueryCount; qId<limit; ++qId) {
            unsigned distance = acceptedDistances[r.next()%acceptedDistances.size()];
            resultContainer[qId].first = distance;
            for (unsigned i=0, querySize=randBetween(r, avgQuerySize-deviation, avgQuerySize+deviation); i<querySize; ++i)
                resultContainer[qId].second.push_back(getRandomWord(r, distQueries[distance]));
            assert(resultContainer[qId].second.size()>=avgQuerySize-deviation && resultContainer[qId].second.size()<=avgQuerySize+deviation);
        }
        // rejecting queries
        for (uint64_t qId=matchingQueryCount, limit=queryCount; qId<limit; ++qId) {
            unsigned distanceReject = distances[cutoff+(r.next()%(distances.size()-cutoff))];
            unsigned distanceAccept = acceptedDistances[r.next()%acceptedDistances.size()];
            //std::cerr << resultContainer[qId].second.size() << endl;
            //assert(distanceReject >= avgQuerySize-deviation && distanceReject <= avgQuerySize+deviation);
            resultContainer[qId].first = distanceAccept;
            for (unsigned i=0, querySize=randBetween(r, avgQuerySize-deviation, avgQuerySize+deviation); i<querySize; ++i) {
                unsigned d = (i==0)?distanceReject:(r.next()%2?distanceReject:distanceAccept);
                resultContainer[qId].second.push_back(getRandomWord(r, distQueries[d]));
            }
            assert(resultContainer[qId].second.size()>=avgQuerySize-deviation && resultContainer[qId].second.size()<=avgQuerySize+deviation);
        }
        random_shuffle(resultContainer.begin(), resultContainer.end());
    }

    template <typename T>
    const string& getRandomWord(Random& r, const T& container) const {
        const string* ret=0;
        do {
            ret = &container[r.next()%container.size()];
        } while (ret->size() > 31 || ret->size() < 4);
        return *ret;
    }

    void dumpDocument(ofstream& out, uint32_t documentId) {
        out << "m " << documentId << ' ' << documents[documentId].size();
        for (const auto& w : documents[documentId])
            out << ' ' << w;
        out << endl;
    }
};


int main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "usage: " << argv[0] << " <wordFile> <out file> [<limit>]" << endl;
        return -1;
    }

    DataSet dataSet(argv[1], argc>3?atoi(argv[3]):5000);

    uint64_t queryCountEx=28861;
    uint64_t queryCountHa=28487;
    uint64_t queryCountEd=29052;
    uint64_t matchingQueryFraction=10;
    uint64_t avgQuerySize=3;
    uint64_t querySizeDeviation=2;
    uint64_t queriesInSystem=18600;
    vector<unsigned> distancesHa = {1,2,2,2,2,3,3,3,3};
    vector<unsigned> distancesEd = {1,2,2,2,2,2,3};

    uint64_t documentCount = 16368;
    uint64_t avgDocSize = 8322;
    uint64_t docSizeDeviation = 1000;

    uint64_t cycles = 341;

    // assume a valid configuration --  otherwise the algorithm crashes
    // assert(queryCount % queriesInSystem == 0);
    // assert(documentCount%dataSet.cycles == 0);
    // assert(avgDocSize > docSizeDeviation);

    // create queries
    dataSet.createExactQueries(queryCountEx, queryCountEx/matchingQueryFraction, avgQuerySize, querySizeDeviation, 328*0.5/*unique words*/);
    dataSet.createHammingQueries(queryCountHa, queryCountHa/matchingQueryFraction, avgQuerySize, querySizeDeviation, distancesHa, 36301*0.6/*unique words*/);
    dataSet.createLevenshteinQueries(queryCountEd, queryCountEx/matchingQueryFraction, avgQuerySize, querySizeDeviation, distancesEd, 34165*0.75/*unique words*/);
    
    // create documents/messages
    dataSet.createDocuments(documentCount-(10000+50+50), (avgDocSize+10000)/10, docSizeDeviation/10);
    dataSet.createDocuments(10000, 812/10, 1); // add some more documents for median
    dataSet.createDocuments(50, (140*1000)/10, 100); // add some more long documents
    dataSet.createDocuments(50, 140/10, 2); // add some more short documents
    dataSet.dumpDocStats();

    dataSet.queriesInSystem=queriesInSystem;
    dataSet.cycles=cycles;

    dataSet.dump(argv[2]);

    return 0;
}
