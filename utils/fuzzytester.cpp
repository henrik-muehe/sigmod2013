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

#include "core.h"
#include "random.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <unordered_set>
#include <vector>
#include <string>
#include <cassert>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>

using namespace std;

campers::Random r;
unordered_set<QueryID> activeQueries;
unordered_set<DocID> pendingResults;
vector<string> rndWords;
uint64_t hashvalue;

// for sane test
unsigned maxActiveQueries=100;
unsigned maxPendingResults=100;

// for large scale test
uint64_t wordCount=1000*1000;
uint64_t activeQueryCount=10*1000;


static std::string randomWord() {
	std::string word;
	uint64_t length=random()%(MAX_WORD_LENGTH-MIN_WORD_LENGTH);
	for (uint64_t index=0;index<=MIN_WORD_LENGTH+length;++index) {
		word.append(1,(random()%26)+'a');
	}
	return std::move(word);
}

static std::vector<std::string> randomWordVector(uint64_t maxCount=MAX_QUERY_WORDS) {
	std::vector<std::string> words;
	for (uint64_t index=0;index<1+(random()%(maxCount));++index) {
		words.emplace_back(rndWords[random()%rndWords.size()]);
	}
	return std::move(words);
}

static std::string randomDoc(uint64_t wordCount) {
	std::string str;
	bool first=true;
	for (uint64_t index=0;index<wordCount;++index) {
		auto word=rndWords[random()%rndWords.size()];
		if (first) {
			first=false;
			str+=word;
		} else {
			str+=" "; str+=word;
		}
	}
	return std::move(str);
}

struct Stats {
	uint64_t minDocLength;
	uint64_t maxDocLength;
	uint64_t maxActiveQueries;
	uint64_t maxNumberResults;
	uint64_t exactQueries;
	uint64_t hammingQueries[4];
	uint64_t editQueries[4];
	uint64_t callsMatch;
	uint64_t callsStart;
	uint64_t callsEnd;
	uint64_t callsNextRes;
	Stats() {
		reset();
	}
	void reset() {
		memset(this,0x0,sizeof(Stats));
	}
	void docLen(uint64_t docLength) {
		minDocLength=min(docLength,minDocLength);
		maxDocLength=max(docLength,maxDocLength);
	}
	void dump() {
		//cerr << endl;
		//cerr << string(100,'-') << endl;
		//cerr << "-- Fun Stats " << string(87,'-') << endl;
		//cerr << string(100,'-') << endl;
		//cerr << "docLength:          [" << minDocLength << "," << maxDocLength << "]" << endl;
		//cerr << "maxActiveQueries:   " << maxActiveQueries << endl;
		//cerr << "maxNumberResults:   " << maxNumberResults << endl;
		//cerr << "exactQueries:       " << exactQueries << endl;
		//cerr << "hammingQueries:     " << hammingQueries[0] << "|" << hammingQueries[1] << "|" << hammingQueries[2] << "|" << hammingQueries[3] << endl;
		//cerr << "editQueries:        " << editQueries[0] << "|" << editQueries[1] << "|" << editQueries[2] << "|" << editQueries[3] << endl;
		//cerr << "callsMatch:         " << callsMatch << endl;
		//cerr << "callsStart:         " << callsStart << endl;
		//cerr << "callsEnd:           " << callsEnd << endl;
		//cerr << "callsNextResult:    " << callsNextRes << endl;
		//cerr << string(100,'-') << endl;
	}
	~Stats() {
		dump();
	}
};

Stats stats;

template <typename T>
string toStr(const T& t) {
	stringstream tmp;
	tmp << t;
	return tmp.str();
}

template <typename Iter>
string join(const Iter& begin, const Iter& end, const string& sep = " ") {
	stringstream tmp;
	for (Iter it=begin; it!=end; ++it) {
		if (it!=begin)
			tmp << sep;
		tmp << *it;
	}
	return tmp.str();
}

uint64_t rnd(uint64_t min, uint64_t max) {
	assert(min<=max);
	auto res = min+(r.next()%((max-min)+1));
	assert(res>=min && res<=max);
	return res;
}
uint64_t rnd(uint64_t limit) {
	return r.next()%limit;
}

typedef vector<string> Doc;
static uint64_t nextDocId=1;
static uint64_t nextQueryId=1;

struct Query {
	MatchType t;
	unsigned dist;
	vector<string> words;

	// Constructor
	Query() : dist(0) {}

	// Move Constructor
	Query(Query&& other) {
		t=other.t;
		dist=other.dist;
		words=move(other.words);
	}

	// Random Query Generator
	static Query generateQuery() {
		Query q;
		q.t=(MatchType)rnd(0,2);
		if (q.t) q.dist=rnd(0,3);
		q.words=randomWordVector();
		return move(q);
	}
};

Doc generateDoc() {
	Doc result;
	result=randomWordVector(rnd(1,MAX_DOC_LENGTH/MAX_WORD_LENGTH));
	return move(result);
}

void startQuery() {
	++stats.callsStart;
	const auto q=Query::generateQuery();
	auto id=nextQueryId++;
	ErrorCode ret = StartQuery(id, join(q.words.begin(), q.words.end()).c_str(), q.t, q.dist);
	//cerr << "StartQuery(" << id << ",'" << join(q.words.begin(), q.words.end()).c_str() << "'," << q.t << "," << q.dist << ")" << endl;
	if (ret!=EC_SUCCESS)
		throw runtime_error("start query error in Query "+toStr(id)+": "+toStr(ret));
	activeQueries.insert(id);
	stats.maxActiveQueries=max(stats.maxActiveQueries,activeQueries.size());
	switch(q.t) {
		case MT_EXACT_MATCH:
			++stats.exactQueries;
			break;
		case MT_HAMMING_DIST:
			++stats.hammingQueries[q.dist];
			break;
		case MT_EDIT_DIST:
			++stats.editQueries[q.dist];
			break;
		default: throw runtime_error("unknown query type "+toStr(q.t));
	}
}

void endQuery() {
	++stats.callsEnd;
	assert(activeQueries.size());
	auto it = activeQueries.begin();
	ErrorCode ret = EndQuery(*it);
	//cerr << "EndQuery(" << *it << ")" << endl;
	if (ret!=EC_SUCCESS)
		throw runtime_error("end query error in Query "+toStr(*it)+": "+toStr(ret));
	activeQueries.erase(it);
}

void matchDocument() {
	++stats.callsMatch;
	Doc d = generateDoc();
	stats.docLen(d.size());
	auto id=nextDocId++;
	ErrorCode ret = MatchDocument(id, join(d.begin(), d.end()).c_str());
	//cerr << "MatchDocument(" << id << ",size=" << d.size() << ")" << endl;
	if (ret!=EC_SUCCESS)
		throw runtime_error("match document error in Doc "+toStr(id));
	pendingResults.insert(id);
}

void getNextAvailRes() {
	++stats.callsNextRes;
	assert(pendingResults.size());
	DocID id;
	unsigned numResults;
	QueryID* idsPtr;
	ErrorCode result = GetNextAvailRes(&id, &numResults, &idsPtr);
	hashvalue+=numResults;
	//cerr << "GetNextAvailRes() " << flush;
	if (result == EC_FAIL)
		throw runtime_error("get next available result error: "+toStr(result));
	if (result == EC_NO_AVAIL_RES)
		if (pendingResults.size())
			throw runtime_error("bogus EC_NO_AVAIL_RES");
	auto it=pendingResults.find(id);
	if (it==pendingResults.end())
		throw runtime_error("get next available result error: unknown id "+toStr(id));
	pendingResults.erase(it);
	stats.maxNumberResults=max(stats.maxNumberResults,static_cast<uint64_t>(numResults));
	//cerr << "retrieves "<<numResults<<" results for Doc " << id << endl;

}

void saneTest(unsigned iterations=1) {
	// Create index
	//cerr << "InitializeIndex()..." << flush;
	InitializeIndex();
	//cerr << "done." << endl;

	// Reset
	nextDocId=1;
	nextQueryId=1;
	activeQueries.clear();
	pendingResults.clear();


	unsigned limit;
	for (unsigned i=0; i<iterations; ++i) {
		//cerr << "iteration " << i << endl;
		limit=rnd(1+(maxActiveQueries/100),maxActiveQueries);
		while (activeQueries.size() < limit)
			startQuery();

		limit=rnd(1+(maxPendingResults/100),maxPendingResults);
		while (pendingResults.size() < limit)
			matchDocument();

		while (pendingResults.size())
			getNextAvailRes();

		while (activeQueries.size())
			endQuery();

		maxActiveQueries*=2;
		maxPendingResults*=2;
		//cerr << endl;
	}
	//cerr << "\nDestroyIndex()..." << flush;
	// Destroy index
	DestroyIndex();
	//cerr << "done." << endl;
}

void killMe(unsigned iterations) {
	InitializeIndex();
	// Reset
	nextDocId=1;
	nextQueryId=1;
	activeQueries.clear();
	pendingResults.clear();

	for (unsigned i=0; i<iterations; ++i) {
		unsigned op=r.next()%4;
		try {
			switch(op) {
				case 0:
					startQuery();
					break;
				case 1:
					if (activeQueries.size())
						matchDocument();
					break;
				case 2:
					if (pendingResults.size())
						getNextAvailRes();
					break;
				case 3:
					if (activeQueries.size())
						endQuery();
					break;
				default: throw runtime_error("invalid op");
			}
		} catch (const runtime_error& ex) {
			//cerr << "Kill Me caught an exception: " << ex.what() << endl;
		}
	}
	DestroyIndex();
}

void largeScale(unsigned iterations) {
	uint64_t qId=0;
	uint64_t dId=0;
	for (unsigned iter=0; iter<iterations; ++iter) {
		// Start Queries
		for (uint64_t q=0; q<activeQueryCount; ++q) {
			unsigned qType = rnd(0,2);
			auto v=randomWordVector();
			cout << "s " << ++qId << " " << qType << " " << (qType?rnd(0,3):0) << " " << v.size();
			for (const auto& w : v) cout << " " << w;
			cout << endl;
		}

		// Match Documents
		for (unsigned d=0; d<48; ++d) {
			auto tmp=MAX_DOC_LENGTH/MAX_WORD_LENGTH;
			auto n=rnd(tmp/4,tmp);
			cout << "m " << ++dId << " " << n << " " << randomDoc(n) << endl;
			//cout << "r " << dId << " 1 1" << endl; // Pseudo-Retrieve
		}

		// End Queries
		uint64_t endId=qId;
		for (uint64_t q=0; q<activeQueryCount; ++q) {
			cout << "e " << endId-- << endl;
		}
	}
}

enum class TestType : unsigned {Sane,KillMe,LargeScale};

int main(int argc, char* argv[]) {
	const uint64_t loops = argc >= 2 ? strtoul(argv[1], nullptr, 10) : 1;
	const uint64_t iterations = argc >= 3 ? strtoul(argv[2], nullptr, 10) : 1;
	TestType testType = TestType::Sane;
	if (argc >= 4) {
		if (strcmp(argv[3],"KillMe")==0)
			testType=TestType::KillMe;
		else if (strcmp(argv[3],"Sane")==0)
			testType=TestType::Sane;
		else if (strcmp(argv[3],"LargeScale")==0)
			testType=TestType::LargeScale;
		else {
			//cerr << "Unknown test type" << endl;
			return -1;
		}
	}

	// Setup
	for (uint64_t i=0; i<wordCount; ++i)
		rndWords.push_back(randomWord());

	// Sane Test
	switch (testType) {
		case TestType::Sane: {
			//cerr << "BEGIN: Sane Test" << endl;
			for (unsigned i=0; i<loops; ++i)
				saneTest(iterations);
			//cerr << "END: Sane Test" << endl;
			break;
		}
		case TestType::KillMe: {
			//cerr << "BEGIN: KillMe Test" << endl;
			for (unsigned i=0; i<loops; ++i)
					killMe(iterations);
			//cerr << "END: KillMe Test" << endl;
			break;
		}
		case TestType::LargeScale: {
			if (getenv("ACTIVEQUERIES"))
				activeQueryCount=strtoul(getenv("ACTIVEQUERIES"),nullptr,10);
			if (getenv("WORDCOUNT"))
				wordCount=strtoul(getenv("WORDCOUNT"),nullptr,10);
			//cerr << "BEGIN: LargeScale Test with " << activeQueryCount << " queries and " << wordCount << " random words." << endl;
			for (unsigned i=0; i<loops; ++i)
					largeScale(iterations);
			//cerr << "END: LargeScale Test" << endl;
			break;
		}
		default: throw runtime_error("unknown test type");
	}

	std::cout << hashvalue << std::endl;
	return 0;
}
