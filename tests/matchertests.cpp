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

#include "gtest/gtest.h"
#include "core.h"
#include "hamming.hpp"
#include "edit_distance.hpp"
#include <iostream>

using namespace std;
using namespace campers;


class MatcherTests : public testing::Test
{
};

static std::string randomWord() {
	std::string word;
	uint64_t length=random()%(MAX_WORD_LENGTH-MIN_WORD_LENGTH);
	for (uint64_t index=MIN_WORD_LENGTH;index<=MIN_WORD_LENGTH+length;++index) {
		word.append(1,(random()%26)+'a');
	}
	return std::move(word);
}

static std::vector<std::string> randomWordVector(uint64_t maxCount=MAX_QUERY_WORDS) {
	std::vector<std::string> words;
	for (uint64_t index=0;index<1+(random()%(maxCount-1));++index) {
		words.emplace_back(randomWord());
	}
	return std::move(words);
}

static std::string randomString(uint64_t wordCount) {
	std::string str;
	bool first=true;
	for (uint64_t index=0;index<wordCount;++index) {
		auto word=randomWord();
		if (first) {
			first=false;
			str+=word;
		} else {
			str+=" "; str+=word;
		}
	}
	return std::move(str);
}

TEST_F(MatcherTests,MassiveAdd) {
	EditDistance a;
	for (uint64_t index=0;index<1000000;++index)
		a.addQuery(index,random()%3,randomWordVector());

	std::string s=randomString(1000000);
	Tokenizer t;
	t.parse(s.c_str(),s.length());

	a.matchDocument(t);
}
